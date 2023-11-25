#include <stack>
#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drwrap.h"
#include "dr_defines.h"
#include "dr_ir_opcodes.h"

#include "heapnode.h"
#include "threadcontext.h"
#include "cfgnode.h"
#include "symbolinfo.h"

#ifndef DETECTOR_H
#define DETECTOR_H

#define MALLOC_ROUTINE_NAME "malloc"
#define CALLOC_ROUTINE_NAME "calloc"
#define REALLOC_ROUTINE_NAME "realloc"
#define REALLOCARRAY_ROUTINE_NAME "reallocarray"
#define FREE_ROUTINE_NAME "free"

#define BUFFER_SIZE 1024
#define WHITESPACE " \n\r\t\f\v"

typedef enum {
    EMPTY_CALLSTACK,
    SP_NOT_FOUND,
    SUCCESS,
    FAIL
} CheckReturnResult;

typedef enum {
    UNKNOWN_MODULE,
    DIFFERENT_MODULE,
    UNKNOWN_TARGET,
    NOT_BEGINNING,
    CFGNODE_NOT_FOUND,
    CFGEDGE_NOT_FOUND,
    CFGEDGE_FOUND
} CheckCfgResult;

typedef struct {
    size_t nmemb;
    size_t size;
} CallocArguments;

typedef struct {
    void *ptr;
    size_t size;
} ReallocArguments;

typedef struct {
    void *ptr;
    size_t nmemb;
    size_t size;
} ReallocarrayArguments;

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]);
static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace, bool translating, void *user_data);

static void at_call(app_pc instr_addr, app_pc target_addr);
static void at_call_ind(app_pc instr_addr, app_pc target_addr);
static void at_return(app_pc instr_addr, app_pc target_addr);
static void at_jump_ind(app_pc instr_addr, app_pc target_addr);

static void module_load_event(void *drcontext, const module_data_t *mod, bool loaded);
static void wrap_malloc_pre(void *wrapcxt, OUT void **user_data);
static void wrap_malloc_post(void *wrapcxt, void *user_data);
static void wrap_calloc_pre(void *wrapcxt, OUT void **user_data);
static void wrap_calloc_post(void *wrapcxt, void *user_data);
static void wrap_realloc_pre(void *wrapcxt, OUT void **user_data);
static void wrap_realloc_post(void *wrapcxt, void *user_data);
static void wrap_reallocarray_pre(void *wrapcxt, OUT void **user_data);
static void wrap_reallocarray_post(void *wrapcxt, void *user_data);
static void wrap_free_pre(void *wrapcxt, OUT void **user_data);

static void addNodeToHeapList(std::list<HeapNode *> *heapList, HeapNode *node);
static bool removeNodeFromHeapList(std::list<HeapNode *> *heapList, HeapNode *node);
static HeapNode *findNodeInHeapList(std::list<HeapNode *> *heapList, void *address);

static void pushNodeToCallStack(std::stack<CallNode *> *callStack, CallNode *node);
static CallNode *popNodeFromCallStack(std::stack<CallNode *> *callStack);

static void saveCall(app_pc pc, reg_t bp, reg_t sp);
static CheckReturnResult checkReturn(reg_t sp, reg_t bp, app_pc target_addr, bool *hasLongJmpPtr);
static CheckCfgResult checkCfg(app_pc instr_addr, app_pc target_addr);
static void processIndirectJump(app_pc instr_addr, app_pc target_addr);
static void printCallTrace();

static SymbolInfo *getSymbolInfo(app_pc addr);
static std::string getSymbolString(app_pc addr);
static bool isInstrIndirectJump(instr_t *instr);
static std::vector<std::string> *splitString(std::string data, std::string delim, std::size_t count = -1);
static std::string ltrim(const std::string &s);
static std::string rtrim(const std::string &s);
static std::string trim(const std::string &s);

#endif
