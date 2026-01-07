#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <jansson.h>
#include <errno.h>

#include "dlz_minimal.h"

/******************************************************************************
 * DEFINES & CONSTANTS
 ******************************************************************************/
#define NB_REFRESH_INTERVAL_SECONDS 300  // N minutes (e.g. 5 mins)
#define NB_USER_AGENT "bind-dlz-netbird/1.0"
#define NB_MAX_URL_LEN 512

/******************************************************************************
 * DATA STRUCTURES
 ******************************************************************************/

/* Linked List Node for DNS Records */
typedef struct nb_record {
    char *hostname;         // e.g., "nas"
    char *ip;              // e.g., "100.64.0.5"
    struct nb_record *next;
} nb_record_t;

/* Global State (The "Survivor" Struct) */
typedef struct nb_state {
    // Configuration
    char *api_key;
    char *api_url;
    char *zone_name;
    
    // Concurrency Control
    pthread_rwlock_t lock;      // The "Traffic Light"
    pthread_t thread_id;        // Background storage thread
    volatile int stop_flag;     // Signal to kill thread
    
    // Data Storage (Shadow Table)
    nb_record_t *records;       // Head of the active linked list
    
    // Logging helper
    dns_dlz_write_log_t log; 
} nb_state_t;

/******************************************************************************
 * LOGGING HELPER
 ******************************************************************************/
static void nb_log(nb_state_t *state, int level, const char *fmt, ...) {
    if (state && state->log) {
        va_list args;
        va_start(args, fmt);
        // In a real generic implementation, we'd format this. 
        // BIND's log function takes varargs, so passing through is tricky without proper vfmt 
        // support in the minimal header, but standard syslog-style strings work.
        // For simplicity/safety here without vsnprintf buffer risks:
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        state->log(level, "%s", buffer);
        va_end(args);
    }
}

/******************************************************************************
 * JSON ENGINE & BACKGROUND THREAD
 ******************************************************************************/

/* CURL Write Callback */
struct string_buffer {
    char *ptr;
    size_t len;
};

static size_t write_func(void *ptr, size_t size, size_t nmemb, struct string_buffer *s) {
    size_t new_len = s->len + size * nmemb;
    char *ptr_realloc = realloc(s->ptr, new_len + 1);
    if (ptr_realloc == NULL) {
        return 0; // Out of memory
    }
    s->ptr = ptr_realloc;
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

/* Frees a linked list of records */
static void free_record_list(nb_record_t *head) {
    nb_record_t *current = head;
    while (current != NULL) {
        nb_record_t *next = current->next;
        free(current->hostname);
        free(current->ip);
        free(current);
        current = next;
    }
}

/* Fetches and Parses Netbird API */
static void fetch_and_update(nb_state_t *state) {
    CURL *curl;
    CURLcode res;
    struct string_buffer chunk = {0};
    
    chunk.ptr = malloc(1); // will grow
    chunk.ptr[0] = '\0';
    chunk.len = 0;

    curl = curl_easy_init();
    if (!curl) {
        nb_log(state, ISC_LOG_ERROR, "Netbird DLZ: Curl init failed");
        free(chunk.ptr);
        return;
    }

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", state->api_key);
    
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, state->api_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, NB_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10s timeout

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        nb_log(state, ISC_LOG_ERROR, "Netbird DLZ: Curl perform failed: %s", curl_easy_strerror(res));
        // Error Handling: API down? Keep old cache (do nothing).
        goto cleanup;
    }
    
    // Parse JSON
    json_error_t error;
    json_t *root = json_loads(chunk.ptr, 0, &error);
    
    if (!root) {
        nb_log(state, ISC_LOG_ERROR, "Netbird DLZ: JSON parse error on line %d: %s", error.line, error.text);
        goto cleanup;
    }

    if (!json_is_array(root)) {
        nb_log(state, ISC_LOG_ERROR, "Netbird DLZ: JSON root is not an array");
        json_decref(root);
        goto cleanup;
    }

    // Build In-Memory Shadow Table (Linked List)
    nb_record_t *new_list_head = NULL;
    size_t index;
    json_t *value;

    json_array_foreach(root, index, value) {
        json_t *j_host = json_object_get(value, "name");
        json_t *j_ip = json_object_get(value, "ip"); // Assuming simple 'ip' field, might be nested in real Netbird

        if (json_is_string(j_host) && json_is_string(j_ip)) {
            nb_record_t *node = malloc(sizeof(nb_record_t));
            if (!node) continue;
            
            // Handle FQDNs by stripping the domain part (e.g. "host.example.com" -> "host")
            node->hostname = strdup(json_string_value(j_host));
            if (node->hostname) {
                char *dot = strchr(node->hostname, '.');
                if (dot) *dot = '\0';

                // Sanitize: Replace spaces with hyphens for valid DNS labels
                for (char *p = node->hostname; *p; p++) {
                    if (*p == ' ') *p = '-';
                }
            }
            node->ip = strdup(json_string_value(j_ip));
            node->next = new_list_head;
            new_list_head = node;
        }
    }

    json_decref(root);

    // Atomic Swap
    // "Traffic Light": Ensure BIND isn't reading while we swap pointer
    pthread_rwlock_wrlock(&state->lock);
    
    nb_record_t *old_list = state->records;
    state->records = new_list_head;
    
    pthread_rwlock_unlock(&state->lock);
    
    nb_log(state, ISC_LOG_INFO, "Netbird DLZ: Cache updated successfully");

    // Clean up old list outside the lock (safe now, no one is reading it)
    free_record_list(old_list);

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.ptr);
}

/* The Background Thread Function */
static void *nb_update_thread(void *arg) {
    nb_state_t *state = (nb_state_t *)arg;
    
    // Initial fetch
    fetch_and_update(state);
    
    while (!state->stop_flag) {
        int sleep_counter = 0;
        // Sleep in small chunks to allow faster shutdown
        while (sleep_counter < NB_REFRESH_INTERVAL_SECONDS && !state->stop_flag) {
            sleep(1);
            sleep_counter++;
        }
        
        if (!state->stop_flag) {
            fetch_and_update(state);
        }
    }
    return NULL;
}

/******************************************************************************
 * BIND SDK INTERFACE (DLZ Minimal API)
 ******************************************************************************/

/* 
 * dlz_create()
 * Standard constructor. Spawns the pthread.
 */
isc_result_t dlz_create(const char *dlzname, unsigned int argc, char *argv[],
                        void **dbdata, const char **helper_name) {
    (void)dlzname;
    (void)helper_name;
    
    // Expect args: <zone_name> <api_key> <api_url>
    if (argc < 3) {
        // We log to stderr here because state->log isn't ready
        fprintf(stderr, "Netbird DLZ usage: dlz_netbird <zone> <api_key> <api_url>\n");
        return ISC_R_FAILURE;
    }

    nb_state_t *state = calloc(1, sizeof(nb_state_t));
    if (!state) return ISC_R_NOMEMORY;

    // Initialize Config
    state->zone_name = strdup(argv[1]);
    state->api_key = strdup(argv[2]);
    state->api_url = (argc >= 4) ? strdup(argv[3]) : strdup("https://api.netbird.io/api/peers");

    // Initialize Lock
    if (pthread_rwlock_init(&state->lock, NULL) != 0) {
        free(state);
        return ISC_R_FAILURE;
    }
    
    state->stop_flag = 0;
    state->records = NULL;

    // Start Management Plane Thread
    if (pthread_create(&state->thread_id, NULL, nb_update_thread, state) != 0) {
        pthread_rwlock_destroy(&state->lock);
        free(state);
        return ISC_R_FAILURE;
    }

    *dbdata = state;
    return ISC_R_SUCCESS;
}

/*
 * dlz_destroy()
 * Destructor. Cleans up memory and kills background thread.
 */
void dlz_destroy(void *dbdata) {
    nb_state_t *state = (nb_state_t *)dbdata;
    if (!state) return;

    // Signal thread to stop
    state->stop_flag = 1;
    pthread_join(state->thread_id, NULL);

    // Clean up memory
    pthread_rwlock_destroy(&state->lock);
    free_record_list(state->records);
    
    free(state->zone_name);
    free(state->api_key);
    free(state->api_url);
    free(state);
}

/*
 * dlz_findzonedb()
 * Not strictly used for minimal DLZ but often required.
 */
isc_result_t dlz_findzonedb(void *dbdata, const char *name,
                            void **user_data, void **driver_data) {
    (void)dbdata;
    (void)name;
    (void)user_data;
    (void)driver_data;

    // Simply check if it's the right zone?
    // Minimal DLZ often skips this or just returns success if we only handle one zone.
    return ISC_R_SUCCESS;
}

/*
 * dlz_lookup()
 * The "Hot Path". Thread-safe, non-blocking memory lookup.
 */
isc_result_t dlz_lookup(const char *zone, const char *name, void *driver_data,
                        dns_sdlz_putrr_t putrr, dns_sdlz_putnamedrr_t putnamedrr,
                        void *ptr) {
    (void)putnamedrr;
    nb_state_t *state = (nb_state_t *)driver_data;
    isc_result_t result = ISC_R_NOTFOUND;

    if (strcmp(zone, state->zone_name) != 0) {
        return ISC_R_NOTFOUND;
    }

    // Acquire Read Lock (allows concurrent lookups)
    pthread_rwlock_rdlock(&state->lock);

    nb_record_t *curr = state->records;
    while (curr != NULL) {
        if (strcmp(curr->hostname, name) == 0) {
            // Found it! Inject result directly into BIND packet.
            // TTL = 60s hardcoded for dynamic VPN 
            if (putrr(ptr, "A", 60, curr->ip) == ISC_R_SUCCESS) {
                result = ISC_R_SUCCESS;
            } else {
                result = ISC_R_FAILURE;
            }
            break; 
        }
        curr = curr->next;
    }

    // Release Lock
    pthread_rwlock_unlock(&state->lock);

    return result;
}

/*
 * dlz_version()
 * Required version handshake.
 */
int dlz_version(unsigned int *flags) {
    (void)flags;
    return DLZ_DLOPEN_VERSION;
}
