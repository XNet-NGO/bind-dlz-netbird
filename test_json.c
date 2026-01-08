#include <jansson.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *json_str = "[{\"hostname\":\"testnode\",\"ip\":\"1.2.3.4\"}]";
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    
    if(!root) { printf("Error: %s\n", error.text); return 1; }
    
    if (!json_is_array(root)) { printf("Not array\n"); return 1; }

    printf("Array size: %zu\n", json_array_size(root));

    size_t index;
    json_t *peer;
    
    json_array_foreach(root, index, peer) {
        printf("Peer %zu type=%d size=%zu\n", index, json_typeof(peer), json_object_size(peer));
        
        // Direct get
        json_t *h = json_object_get(peer, "hostname");
        printf("Hostname Ptr: %p\n", (void*)h);
        if(h) {
            printf("Hostname Type: %d\n", json_typeof(h));
            printf("Hostname Val: %s\n", json_string_value(h));
        }

        json_t *ip = json_object_get(peer, "ip");
        printf("IP Ptr: %p\n", (void*)ip);
        if(ip) {
            printf("IP Val: %s\n", json_string_value(ip));
        }

        // Iterator
        void *iter = json_object_iter(peer);
        while(iter) {
            const char *k = json_object_iter_key(iter);
            json_t *v = json_object_iter_value(iter);
            printf("Key: %s, Val: %s\n", k, json_string_value(v));
            iter = json_object_iter_next(peer, iter);
        }
    }
    return 0;
}
