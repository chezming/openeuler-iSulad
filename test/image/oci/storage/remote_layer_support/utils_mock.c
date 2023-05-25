#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>

#include "mock_flags.h"
#include "map/map.h"

void *util_common_calloc_s(size_t size)
{
    struct util_call_flags *flags = get_flags();
    if (flags->alloc_flag) {
        return NULL;
    }

    const char *symbol = "util_common_calloc_s";
    void *(*original)(size_t) = dlsym(RTLD_NEXT, symbol);
    return original(size);
}

char *util_path_join(const char *dir, const char *file)
{
    struct util_call_flags *flags = get_flags();
    if (flags->path_join_flag) {
        return NULL;
    }

    const char *symbol = "util_path_join";
    char *(*original)(const char *, const char *) = dlsym(RTLD_NEXT, symbol);
    return original(dir, file);
}

char *util_clean_path(const char *path, char *realpath, size_t realpath_len)
{
    struct util_call_flags *flags = get_flags();
    if (flags->clean_path_flag) {
        return NULL;
    }

    const char *symbol = "util_clean_path";
    char *(*original)(const char *, char *, size_t) = dlsym(RTLD_NEXT, symbol);
    return original(path, realpath, realpath_len);

}

int util_path_remove(const char *path)
{
    struct util_call_flags *flags = get_flags();
    if (flags->path_remove_flag) {
        return -1;
    }

    const char *symbol = "util_path_remove";
    int (*original)(const char *) = dlsym(RTLD_NEXT, symbol);
    return original(path);
}

bool map_insert(map_t *map, void *key, void *value)
{
    struct util_call_flags *flags = get_flags();
    if (flags->map_insert_flag) {
        return false;
    }

    const char *symbol = "map_insert";
    bool (*original)(map_t *, void *, void *) = dlsym(RTLD_NEXT, symbol);
    return original(map, key, value);
}
