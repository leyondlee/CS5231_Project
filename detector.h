#include "dr_defines.h"

struct CallStackItem {
    reg_t sp;
    app_pc value;
    struct CallStackItem *next;
};
typedef struct CallStackItem CallStackItem;

typedef struct {
    CallStackItem *head;
    uint64 size;
} CallStack;

typedef enum {
    EMPTY_CALLSTACK,
    SP_NOT_FOUND,
    SUCCESS,
    FAIL
} CheckReturnResult;

typedef struct {
    CallStack *callStack;
    thread_id_t thread_id;
} ThreadContext;
