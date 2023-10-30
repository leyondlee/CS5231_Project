#include "dr_defines.h"

struct CallNode {
    reg_t sp;
    app_pc value;
    struct CallNode *next;
};
typedef struct CallNode CallNode;

typedef struct {
    CallNode *head;
    uint64 size;
} CallStack;

struct HeapNode {
    void *address;
    size_t size;
    struct HeapNode *prev;
    struct HeapNode *next;
};
typedef struct HeapNode HeapNode;

typedef struct {
    HeapNode *head;
    HeapNode *tail;
    uint64 size;
} HeapList;

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

typedef struct {
    char *data;
    size_t length;
} SymbolString;
