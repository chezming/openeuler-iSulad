#include "mock_flags.h"

static struct util_call_flags flags;

struct util_call_flags * get_flags(void)
{
    return &flags;
}
