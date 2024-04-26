//
// Created by Alex Brodsky on 2024-03-10.
//
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include "cache.h"

#define num_sets 2
#define lines_per_set 4
#define block_size 32

unsigned int offset_bits = 5;
unsigned int index_bits = 1;

typedef struct {
    unsigned int valid;
    unsigned int tag;
    char data[block_size];
} CacheLine;

typedef struct {
    CacheLine lines[lines_per_set];
} CacheSet;

typedef struct {
    int initialized;
    CacheSet sets[num_sets];
} Cache;

static void initialize_cache_set(CacheSet *set) {
    for (size_t i = 0; i < lines_per_set; i++) {
        set->lines[i].valid = false;
        set->lines[i].tag = UINT_MAX;
        memset(set->lines[i].data, 0, block_size);
    }
}

static void initialize_cache(Cache *cache) {
    for (size_t i = 0; i < num_sets; i++) {
        initialize_cache_set(&cache->sets[i]);
    }
    cache->initialized = true;
}

static int find_line(CacheSet *set, unsigned int tag, size_t *line_index) {
    for (size_t i = 0; i < lines_per_set; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            *line_index = i;
            return 1; // Cache hit
        }
    }
    return 0; // Cache miss
}

static size_t choose_victim_line(CacheSet *set) {
    for (size_t i = 0; i < lines_per_set; i++) {
        if (!set->lines[i].valid) {
            return i;
        }
    }
    // Simple eviction strategy: replace the first line
    return 0;
}

static int load_data_from_memory(unsigned long address, char *data) {
    if (memget(address, data, block_size) != block_size) {
        return 0; // Failed to load from main memory
    }
    return 1; // Data loaded successfully
}

extern int cache_get(unsigned long address, unsigned long *value) {
    Cache *cache = (Cache *) c_info.F_memory;

    if (!cache->initialized) {
        initialize_cache(cache);
    }

    unsigned int tag = address >> (offset_bits + index_bits);
    unsigned int index = (address >> offset_bits) & ((1 << index_bits) - 1);
    unsigned int offset = address & ((1 << offset_bits) - 1);

    CacheSet *set = &cache->sets[index];

    size_t line_index;
    if (find_line(set, tag, &line_index)) {
        // Cache hit
        *value = *((unsigned long *) (set->lines[line_index].data + offset));
        return 1;
    }

    // Cache miss
    line_index = choose_victim_line(set);
    set->lines[line_index].valid = true;
    set->lines[line_index].tag = tag;

    if (!load_data_from_memory(address - offset, set->lines[line_index].data)) {
        return 0; // Failed to load from main memory
    }

    // Data loaded successfully
    *value = *((unsigned long *) (set->lines[line_index].data + offset));
    return 1;
}
