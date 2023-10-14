#include "dr_defines.h"

struct CallStackItem {
    reg_t sp;
    app_pc value;
    struct CallStackItem *next;
};
typedef struct CallStackItem CallStackItem;

typedef struct {
    CallStackItem *head;
} CallStack;

typedef enum {
    SP_NOT_FOUND,
    SUCCESS,
    FAIL
} CheckReturnResult;
