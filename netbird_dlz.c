#include <urcu.h>
#include <urcu/wfcqueue.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <jansson.h>
#include <errno.h>
#include <stdarg.h>

/* BIND 9.18+ DLZ headers */
#include <dns/dlz_dlopen.h>
#include <dns/sdlz.h>
#include <isc/result.h>

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
} nb_state_t;

/******************************************************************************
 * LOGGING HELPER
 ******************************************************************************/
#define NB_LOG_INFO    0
#define NB_LOG_WARNING 1
#define NB_LOG_ERROR   2

static void nb_log(nb_state_t *state, int level, const char *fmt, ...) {
    (void)state;  // unused
    (void)level;  // unused for now
    FILE *fp = fopen("/tmp/dlz.log", "a");
    if (fp) {
        va_list args;
        va_start(args, fmt);
        fprintf(fp, "[Netbird-DLZ] ");
        vfprintf(fp, fmt, args);
        fprintf(fp, "\n");
        va_end(args);
        fclose(fp);
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
        nb_log(state, NB_LOG_ERROR, "Netbird DLZ: Curl init failed");
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
        nb_log(state, NB_LOG_ERROR, "Netbird DLZ: Curl perform failed: %s", curl_easy_strerror(res));
        // Error Handling: API down? Keep old cache (do nothing).
        goto cleanup;
    }

    // DEBUG: Dump JSON to /tmp/netbird_debug.json for inspection
    FILE *debug_fp = fopen("/tmp/netbird_debug.json", "w");
    if (debug_fp) {
        if (chunk.ptr) {
            fputs(chunk.ptr, debug_fp);
        }
        fclose(debug_fp);
        nb_log(state, NB_LOG_INFO, "Netbird DLZ: Debug dump written to /tmp/netbird_debug.json");
    }
    
    // Parse JSON
    json_error_t error;
    json_t *root = json_loads(chunk.ptr, 0, &error);
    
    if (!root) {
        nb_log(state, NB_LOG_ERROR, "Netbird DLZ: JSON parse error on line %d: %s", error.line, error.text);
        goto cleanup;
    }

    if (!json_is_array(root)) {
        nb_log(state, NB_LOG_ERROR, "Netbird DLZ: JSON root is not an array");
        json_decref(root);
        goto cleanup;
    }

    nb_log(state, NB_LOG_INFO, "Jansson Consts: OBJ=%d ARR=%d STR=%d INT=%d", 
           JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_INTEGER);

    size_t index;
    json_t *peer;
    size_t array_size = json_array_size(root);
    nb_log(state, NB_LOG_INFO, "Netbird DLZ: JSON array size: %zu", array_size);

    nb_record_t *new_list_head = NULL;

    nb_log(state, NB_LOG_INFO, "Debug: sizeof(json_t)=%zu, sizeof(json_type)=%zu", sizeof(json_t), sizeof(json_type));

    json_array_foreach(root, index, peer) {
        nb_log(state, NB_LOG_INFO, "Debug: Processing peer %zu", index);
        
        int type = json_typeof(peer);
        nb_log(state, NB_LOG_INFO, "Debug: Peer type=%d (Expected %d)", type, JSON_OBJECT);

        if (!json_is_object(peer)) {
            nb_log(state, NB_LOG_ERROR, "Debug: Peer is not an object! Skipping.");
            continue;
        }

        /* DIRECT KEY LOOKUP TEST */
        char *host_str = NULL;
        char *ip_str = NULL;
        json_t *chk_host = json_object_get(peer, "hostname");
        json_t *chk_ip = json_object_get(peer, "ip");
        nb_log(state, NB_LOG_INFO, "Debug: Direct Lookup - hostname=%p, ip=%p", (void*)chk_host, (void*)chk_ip);

        if (chk_host) {
            nb_log(state, NB_LOG_INFO, "Debug: hostname type=%d, value='%s'", 
                json_typeof(chk_host), json_string_value(chk_host));
        }

        /* ITERATOR TEST */
        void *iter = json_object_iter(peer);
        nb_log(state, NB_LOG_INFO, "Debug: iter=%p", iter);

        while (iter) {
            const char *key = json_object_iter_key(iter);
            json_t *value = json_object_iter_value(iter);
            
            nb_log(state, NB_LOG_INFO, "Debug: Key='%s'", key); // Log every key

            if (strcmp(key, "hostname") == 0) {
                if (json_is_string(value)) {
                    host_str = strdup(json_string_value(value));
                    nb_log(state, NB_LOG_INFO, "Debug: Found hostname via iter: %s", host_str);
                }
            } else if (strcmp(key, "ip") == 0) {
                if (json_is_string(value)) {
                    // Check IPv4 vs IPv6 logic if needed, simplifed here
                    const char *val_s = json_string_value(value);
                    if (strchr(val_s, ':')) {
                        // IPv6 - skip or handle
                    } else {
                        ip_str = strdup(val_s); 
                        nb_log(state, NB_LOG_INFO, "Debug: Found IP via iter: %s", ip_str);
                    }
                }
            }

            iter = json_object_iter_next(peer, iter);
        }

        while(iter) {
            const char *key = json_object_iter_key(iter);
            json_t *value = json_object_iter_value(iter);
            
            // nb_log(state, NB_LOG_INFO, "Debug: Key='%s'", key);

             if (strcmp(key, "hostname") == 0 || strcmp(key, "name") == 0) {
                // nb_log(state, NB_LOG_INFO, "Debug: Found key '%s'", key);
                if (!host_str) {
                    if (json_is_string(value)) {
                        host_str = strdup(json_string_value(value));
                    } else {
                        // Fallback dump
                        char *s = json_dumps(value, JSON_ENCODE_ANY);
                        if (s && s[0] == '"') {
                             size_t len = strlen(s);
                             if (len >= 2 && s[len-1] == '"') {
                                 s[len-1] = '\0';
                                 host_str = strdup(s+1);
                             }
                        }
                        if (s) free(s);
                    }
                }
            }
            if (strcmp(key, "ip") == 0) {
                if (json_is_string(value)) {
                    ip_str = strdup(json_string_value(value));
                } else {
                    char *s = json_dumps(value, JSON_ENCODE_ANY);
                    if (s && s[0] == '"') {
                         size_t len = strlen(s);
                         if (len >= 2 && s[len-1] == '"') {
                             s[len-1] = '\0';
                             ip_str = strdup(s+1);
                         }
                    }
                    if (s) free(s);
                }
            }

            iter = json_object_iter_next(peer, iter);
        }

        /* End of manual iteration */

        if (host_str && ip_str) {
            nb_record_t *node = malloc(sizeof(nb_record_t));
            if (!node) {
                free(host_str);
                free(ip_str);
                continue;
            }
            
            node->hostname = host_str;
            char *dot = strchr(node->hostname, '.');
            if (dot) *dot = '\0';

            // Sanitize
            for (char *p = node->hostname; *p; p++) {
                if (*p == ' ') *p = '-';
            }
            node->ip = ip_str;

            nb_log(state, NB_LOG_INFO, "Netbird DLZ: Loaded record name='%s' ip='%s'", node->hostname, node->ip);
            
            node->next = new_list_head;
            new_list_head = node;
        } else {
            if (host_str) free(host_str);
            if (ip_str) free(ip_str);
        }
    }

    json_decref(root);

    // Atomic Swap
    // "Traffic Light": Ensure BIND isn't reading while we swap pointer
    pthread_rwlock_wrlock(&state->lock);
    
    nb_record_t *old_list = state->records;
    state->records = new_list_head;
    
    pthread_rwlock_unlock(&state->lock);
    
    nb_log(state, NB_LOG_INFO, "Netbird DLZ: Cache updated successfully");

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
 * Called by BIND to ask "Do you handle this zone?"
 * BIND 9.18+ signature with clientinfo parameters.
 */
isc_result_t dlz_findzonedb(void *dbdata, const char *name,
                            dns_clientinfomethods_t *methods,
                            dns_clientinfo_t *clientinfo) {
    nb_state_t *state = (nb_state_t *)dbdata;
    
    // Unused parameters
    (void)methods;
    (void)clientinfo;

    // Default to not found
    isc_result_t result = ISC_R_NOTFOUND;
    
    // Log the check (DEBUG)
    // nb_log(state, NB_LOG_INFO, "FindZone: Checking '%s' against '%s'", name, state->zone_name);

    if (strcasecmp(name, state->zone_name) == 0) {
        result = ISC_R_SUCCESS;
    } else {
        // Trailing dot check
        size_t nlen = strlen(name);
        size_t zlen = strlen(state->zone_name);
        
        // If name has dot (e.g. "bird.xnet.ngo.") and zone does not ("bird.xnet.ngo")
        if (nlen == zlen + 1 && name[zlen] == '.' && strncasecmp(name, state->zone_name, zlen) == 0) {
             result = ISC_R_SUCCESS;
        }
        // If zone has dot ("bird.xnet.ngo.") and name does not ("bird.xnet.ngo")
        else if (zlen == nlen + 1 && state->zone_name[nlen] == '.' && strncasecmp(name, state->zone_name, nlen) == 0) {
             result = ISC_R_SUCCESS;
        }
    }
    
    return result;
}

/*
 * dlz_lookup()
 * The "Hot Path". Thread-safe, non-blocking memory lookup.
 * BIND 9.18+ signature with dns_sdlzlookup_t pointer.
 */
isc_result_t dlz_lookup(const char *zone, const char *name, void *dbdata,
                        dns_sdlzlookup_t *lookup, 
                        dns_clientinfomethods_t *methods,
                        dns_clientinfo_t *clientinfo) {
    (void)methods;
    (void)clientinfo;
    nb_state_t *state = (nb_state_t *)dbdata;
    isc_result_t result = ISC_R_NOTFOUND;

    // Loose zone matching (handle trailing dot mismatch)
    size_t zlen = strlen(zone);
    size_t slen = strlen(state->zone_name);
    int match = 0;
    
    if (strcasecmp(zone, state->zone_name) == 0) match = 1;
    else if (zlen > slen && zlen == slen + 1 && zone[slen] == '.' && strncasecmp(zone, state->zone_name, slen) == 0) match = 1;
    else if (slen > zlen && slen == zlen + 1 && state->zone_name[zlen] == '.' && strncasecmp(zone, state->zone_name, zlen) == 0) match = 1;

    if (!match) {
        nb_log(state, NB_LOG_WARNING, "Lookup zone mismatch: query='%s', configured='%s'", zone, state->zone_name);
        return ISC_R_NOTFOUND;
    }
    
    nb_log(state, NB_LOG_INFO, "Lookup: zone='%s' name='%s' lookup=%p", zone, name, (void*)lookup);

    // Handle zone apex queries (SOA, NS, etc.) - just return NOTFOUND
    // BIND will handle the apex records itself for DLZ zones with 'search yes'
    if (strcmp(name, "@") == 0 || strcasecmp(name, zone) == 0 || 
        (strlen(name) == strlen(zone) + 1 && name[strlen(zone)] == '.' && strncasecmp(name, zone, strlen(zone)) == 0)) {
        nb_log(state, NB_LOG_INFO, "Zone apex query for '%s' - returning NOTFOUND", name);
        return ISC_R_NOTFOUND;
    }

    // Acquire Read Lock (allows concurrent lookups)
    pthread_rwlock_rdlock(&state->lock);

    nb_record_t *curr = state->records;
    int record_count = 0;
    while (curr != NULL) {
        record_count++;
        nb_log(state, NB_LOG_INFO, "Debug compare: '%s' vs '%s'", curr->hostname, name);
        if (strcasecmp(curr->hostname, name) == 0) {
            // Found it! Inject result directly into BIND packet.
            nb_log(state, NB_LOG_INFO, "Match found! hostname='%s' ip='%s'", curr->hostname, curr->ip);
            
            // Check for null/invalid data
            if (curr->ip == NULL) {
                nb_log(state, NB_LOG_ERROR, "IP is NULL!");
                pthread_rwlock_unlock(&state->lock);
                return ISC_R_FAILURE;
            }
            if (lookup == NULL) {
                nb_log(state, NB_LOG_ERROR, "lookup handle is NULL!");
                pthread_rwlock_unlock(&state->lock);
                return ISC_R_FAILURE;
            }
            
            // TTL = 60s hardcoded for dynamic VPN 
            nb_log(state, NB_LOG_INFO, "Calling dns_sdlz_putrr: lookup=%p type='A' ttl=60 data='%s'", (void*)lookup, curr->ip);
            isc_result_t rr_result = dns_sdlz_putrr(lookup, "A", 60, curr->ip);
            nb_log(state, NB_LOG_INFO, "dns_sdlz_putrr returned: %d", (int)rr_result);
            
            if (rr_result == ISC_R_SUCCESS) {
                nb_log(state, NB_LOG_INFO, "Success: '%s' -> '%s'", name, curr->ip);
                result = ISC_R_SUCCESS;
            } else {
                nb_log(state, NB_LOG_ERROR, "dns_sdlz_putrr failed for %s", name);
                result = ISC_R_FAILURE;
            }
            break; 
        }
        curr = curr->next;
    }
    
    if (result == ISC_R_NOTFOUND) {
         nb_log(state, NB_LOG_INFO, "Lookup failed: '%s' not found in %d records", name, record_count);
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
