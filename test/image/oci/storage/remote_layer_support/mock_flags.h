#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct util_call_flags {
    bool alloc_flag;
    bool path_join_flag;
    bool clean_path_flag;
    bool path_remove_flag;
    bool map_insert_flag;
    int snprintf_cnt;
};

struct util_call_flags * get_flags(void);

#ifdef __cplusplus
}
#endif
