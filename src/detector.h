#include <stack>
#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <algorithm>
#include <unordered_map>

#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drwrap.h"
#include "dr_defines.h"

#include "heapnode.h"
#include "threadcontext.h"
#include "cfgnode.h"

#ifndef DETECTOR_H
#define DETECTOR_H

typedef enum {
    EMPTY_CALLSTACK,
    SP_NOT_FOUND,
    SUCCESS,
    FAIL
} CheckReturnResult;

DR_EXPORT void dr_client_main(client_id_t, int, const char *[]);
static void event_exit(void);
static void event_thread_init(void *);
static void event_thread_exit(void *);
static dr_emit_flags_t event_app_instruction(void *, void *, instrlist_t *, instr_t *, bool , bool , void *);

static bool pushCall(reg_t, app_pc);
static void at_call(app_pc, app_pc);
static void at_call_ind(app_pc, app_pc);
static void at_return(app_pc, app_pc);

static void module_load_event(void *, const module_data_t *, bool);
static void wrap_malloc_pre(void *, OUT void **);
static void wrap_malloc_post(void *, void *);
static void wrap_free_pre(void *, OUT void **);

static void addNodeToHeapList(std::list<HeapNode *> *, HeapNode *);
static bool removeNodeFromHeapList(std::list<HeapNode *> *, HeapNode *);
static HeapNode *findNodeInHeapList(std::list<HeapNode *> *, void *);

static void pushNodeToCallStack(std::stack<CallNode *> *, CallNode *);
static CallNode *popNodeFromCallStack(std::stack<CallNode *> *);

static CheckReturnResult checkReturn(reg_t, app_pc, bool *);
std::string getSymbolString(app_pc);

#endif
