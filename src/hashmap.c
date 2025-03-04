#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

// Initial size must be a power of 2
#define HASHMAP_INITIAL_SIZE 16
#define HASHMAP_LOAD_FACTOR 0.75

// FNV-1a hash function constants
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

typedef struct hashmap_entry {
    void* key;
    size_t key_size;
    void* value;
    struct hashmap_entry* next;
} hashmap_entry;

struct hashmap {
    hashmap_entry** buckets;
    size_t capacity;
    size_t size;
};

// FNV-1a hash function
static uint64_t hash_key(const void* key, size_t length) {
    const unsigned char* data = (const unsigned char*)key;
    uint64_t hash = FNV_OFFSET;
    
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint64_t)data[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

static bool hashmap_resize(hashmap* map, size_t new_capacity) {
    hashmap_entry** new_buckets = calloc(new_capacity, sizeof(hashmap_entry*));
    if (!new_buckets) return false;
    
    // Rehash all entries
    for (size_t i = 0; i < map->capacity; i++) {
        hashmap_entry* entry = map->buckets[i];
        while (entry) {
            hashmap_entry* next = entry->next;
            size_t new_index = hash_key(entry->key, entry->key_size) % new_capacity;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }
    
    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    return true;
}

hashmap* hashmap_create(size_t initial_capacity) {
    if (initial_capacity < HASHMAP_INITIAL_SIZE) {
        initial_capacity = HASHMAP_INITIAL_SIZE;
    }
    
    // Round up to next power of 2
    initial_capacity--;
    initial_capacity |= initial_capacity >> 1;
    initial_capacity |= initial_capacity >> 2;
    initial_capacity |= initial_capacity >> 4;
    initial_capacity |= initial_capacity >> 8;
    initial_capacity |= initial_capacity >> 16;
    initial_capacity |= initial_capacity >> 32;
    initial_capacity++;
    
    hashmap* map = malloc(sizeof(hashmap));
    if (!map) return NULL;
    
    map->buckets = calloc(initial_capacity, sizeof(hashmap_entry*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }
    
    map->capacity = initial_capacity;
    map->size = 0;
    return map;
}

void hashmap_free(hashmap* map, hashmap_cleanup_fn cleanup_fn) {
    if (!map) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        hashmap_entry* entry = map->buckets[i];
        while (entry) {
            hashmap_entry* next = entry->next;
            if (cleanup_fn) {
                cleanup_fn(entry->key, entry->value);
            }
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    
    free(map->buckets);
    free(map);
}

bool hashmap_put(hashmap* map, const void* key, size_t key_size, void* value) {
    if (!map || !key) return false;
    
    // Check if we need to resize
    if (map->size >= map->capacity * HASHMAP_LOAD_FACTOR) {
        if (!hashmap_resize(map, map->capacity * 2)) {
            return false;
        }
    }
    
    size_t index = hash_key(key, key_size) % map->capacity;
    
    // Check if key already exists
    hashmap_entry* entry = map->buckets[index];
    while (entry) {
        if (entry->key_size == key_size && memcmp(entry->key, key, key_size) == 0) {
            entry->value = value;
            return true;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = malloc(sizeof(hashmap_entry));
    if (!entry) return false;
    
    entry->key = malloc(key_size);
    if (!entry->key) {
        free(entry);
        return false;
    }
    
    memcpy(entry->key, key, key_size);
    entry->key_size = key_size;
    entry->value = value;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    map->size++;
    
    return true;
}

bool hashmap_get(const hashmap* map, const void* key, size_t key_size, void** value) {
    if (!map || !key || !value) return false;
    
    size_t index = hash_key(key, key_size) % map->capacity;
    hashmap_entry* entry = map->buckets[index];
    
    while (entry) {
        if (entry->key_size == key_size && memcmp(entry->key, key, key_size) == 0) {
            *value = entry->value;
            return true;
        }
        entry = entry->next;
    }
    
    return false;
}

bool hashmap_remove(hashmap* map, const void* key, size_t key_size, hashmap_cleanup_fn cleanup_fn) {
    if (!map || !key) return false;
    
    size_t index = hash_key(key, key_size) % map->capacity;
    hashmap_entry* entry = map->buckets[index];
    hashmap_entry* prev = NULL;
    
    while (entry) {
        if (entry->key_size == key_size && memcmp(entry->key, key, key_size) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            
            if (cleanup_fn) {
                cleanup_fn(entry->key, entry->value);
            }
            
            free(entry->key);
            free(entry);
            map->size--;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return false;
}

size_t hashmap_size(const hashmap* map) {
    return map ? map->size : 0;
}

bool hashmap_put_string(hashmap* map, const char* key, void* value) {
    return hashmap_put(map, key, strlen(key) + 1, value);
}

bool hashmap_get_string(const hashmap* map, const char* key, void** value) {
    return hashmap_get(map, key, strlen(key) + 1, value);
}

bool hashmap_remove_string(hashmap* map, const char* key, hashmap_cleanup_fn cleanup_fn) {
    return hashmap_remove(map, key, strlen(key) + 1, cleanup_fn);
}

void hashmap_iterate(const hashmap* map, hashmap_iter_fn iter_fn, void* user_data) {
    if (!map || !iter_fn) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        hashmap_entry* entry = map->buckets[i];
        while (entry) {
            iter_fn(entry->key, entry->key_size, entry->value, user_data);
            entry = entry->next;
        }
    }
} 