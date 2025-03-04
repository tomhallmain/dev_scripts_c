#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>

// Opaque type for the hashmap
typedef struct hashmap hashmap;

// Function pointer type for cleanup callbacks
typedef void (*hashmap_cleanup_fn)(void* key, void* value);

// Create a new hashmap with initial capacity
// Returns NULL if allocation fails
hashmap* hashmap_create(size_t initial_capacity);

// Free the hashmap and all its entries
// cleanup_fn can be NULL if no special cleanup is needed
void hashmap_free(hashmap* map, hashmap_cleanup_fn cleanup_fn);

// Insert or update a key-value pair
// The key is copied, but the value is stored as-is
// Returns false if memory allocation fails
bool hashmap_put(hashmap* map, const void* key, size_t key_size, void* value);

// Get a value by key
// Returns false if key not found, true if found (value is set)
bool hashmap_get(const hashmap* map, const void* key, size_t key_size, void** value);

// Remove an entry from the map
// Returns false if key not found, true if removed
// cleanup_fn can be NULL if no special cleanup is needed
bool hashmap_remove(hashmap* map, const void* key, size_t key_size, hashmap_cleanup_fn cleanup_fn);

// Get the number of entries in the map
size_t hashmap_size(const hashmap* map);

// Convenience functions for string keys
bool hashmap_put_string(hashmap* map, const char* key, void* value);
bool hashmap_get_string(const hashmap* map, const char* key, void** value);
bool hashmap_remove_string(hashmap* map, const char* key, hashmap_cleanup_fn cleanup_fn);

// Iterator function type
typedef void (*hashmap_iter_fn)(const void* key, size_t key_size, void* value, void* user_data);

// Iterate over all entries in the map
void hashmap_iterate(const hashmap* map, hashmap_iter_fn iter_fn, void* user_data);

#endif // HASHMAP_H 