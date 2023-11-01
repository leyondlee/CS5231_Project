#include <string.h>

#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drwrap.h"

#include "detector.h"

#define MALLOC_ROUTINE_NAME "malloc"
#define FREE_ROUTINE_NAME "free"

static void event_exit(void);
static void event_thread_init(void *);
static void event_thread_exit(void *);
static dr_emit_flags_t event_app_instruction(void *, void *, instrlist_t *, instr_t *, bool , bool , void *);
static void wrap_malloc_pre(void *, OUT void **);
static void wrap_malloc_post(void *, void *);
static void wrap_free_pre(void *, OUT void **);
static void printHeapList(HeapList *);
static void initHeapList(HeapList *);
static void addNodeToHeapList(HeapList *, HeapNode *);
static bool removeNodeFromHeapList(HeapList *, HeapNode *);
static HeapNode *findNodeInHeapList(HeapList *, void *);

static int tls_idx;
static HeapList heapList;

static void module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc malloc_address = (app_pc) dr_get_proc_address(mod->handle, MALLOC_ROUTINE_NAME);
    if (malloc_address != NULL) {
        bool ok = drwrap_wrap(malloc_address, wrap_malloc_pre, wrap_malloc_post);
        if (!ok) {
            dr_fprintf(STDERR,
                        "<FAILED to wrap " MALLOC_ROUTINE_NAME " @" PFX
                        ": already wrapped?\n",
                        malloc_address);
        }
    }

    app_pc free_address = (app_pc) dr_get_proc_address(mod->handle, FREE_ROUTINE_NAME);
    if (free_address != NULL) {
        bool ok = drwrap_wrap(free_address, wrap_free_pre, NULL);
        if (!ok) {
            dr_fprintf(STDERR,
                        "<FAILED to wrap " FREE_ROUTINE_NAME " @" PFX
                        ": already wrapped?\n",
                        free_address);
        }
    }
}

static void wrap_malloc_pre(void *wrapcxt, OUT void **user_data)
{
    size_t size = (size_t) drwrap_get_arg(wrapcxt, 0);
    *user_data = (void *) size;
}

static void wrap_malloc_post(void *wrapcxt, void *user_data)
{
    HeapNode *node = dr_global_alloc(sizeof(HeapNode));
    if (node == NULL) {
        // Fail to allocate memory
        dr_abort();
    }

    void *address = drwrap_get_retval(wrapcxt);
    if (address == NULL) {
        return;
    }

    node->address = address;
    node->size = (size_t) user_data;
    node->prev = NULL;
    node->next = NULL;
    addNodeToHeapList(&heapList, node);

    //dr_fprintf(STDERR, "[malloc] Address: %p, Size: %ld\n", node->address, node->size);
}

static void wrap_free_pre(void *wrapcxt, OUT void **user_data)
{
    void *ptr = drwrap_get_arg(wrapcxt, 0);
    if (ptr == NULL) {
        return;
    }

    HeapNode *node = findNodeInHeapList(&heapList, ptr);
    if (node == NULL) {
        dr_fprintf(STDERR, "Freeing unallocated memory: %p\n", ptr);
        dr_abort();
    }
    DR_ASSERT(node->address == ptr);

    bool isRemoved = removeNodeFromHeapList(&heapList, node);
    DR_ASSERT(isRemoved);

    //dr_fprintf(STDERR, "[free] ptr: %p\n", node->address);

    dr_global_free(node, sizeof(HeapNode));
}

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'Detector'", "");
    drmgr_init();
    if (drsym_init(0) != DRSYM_SUCCESS) {
        dr_log(NULL, DR_LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n");
    }
    drwrap_init();

    dr_fprintf(STDERR, "Client Detector is running\n");

    dr_register_exit_event(event_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_module_load_event(module_load_event);

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx > -1);

    initHeapList(&heapList);
}

static void event_exit(void)
{
    HeapNode *node = heapList.head;
    while (node != NULL) {
        HeapNode *next = node->next;
        dr_global_free(node, sizeof(HeapNode));
        node = next;
    }

    drmgr_unregister_tls_field(tls_idx);
    drwrap_exit();
    drsym_exit();
    drmgr_exit();
}

static void event_thread_init(void *drcontext)
{
    ThreadContext *threadContext = dr_thread_alloc(drcontext, sizeof(ThreadContext));
    if (threadContext == NULL) {
        dr_abort();
    }

    CallStack *callStack = dr_thread_alloc(drcontext, sizeof(CallStack));
    if (callStack == NULL) {
        dr_abort();
    }

    callStack->head = NULL;
    callStack->size = 0;

    threadContext->callStack = callStack;
    threadContext->thread_id = dr_get_thread_id(drcontext);

    //printf("[%d] New Thread with ID %d\n", dr_get_process_id(), threadContext->thread_id);

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, threadContext);
}

static void event_thread_exit(void *drcontext)
{
    ThreadContext *threadContext = drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);

    CallStack *callStack = threadContext->callStack;
    DR_ASSERT(callStack != NULL);

    CallNode *node = callStack->head;
    while (node != NULL) {
        CallNode *next = node->next;
        dr_thread_free(drcontext, node, sizeof(CallNode));
        node = next;
    }

    dr_thread_free(drcontext, callStack, sizeof(CallStack));
    dr_thread_free(drcontext, threadContext, sizeof(ThreadContext));
}

void freeSymbolString(SymbolString *symbolString)
{
    DR_ASSERT(symbolString != NULL);

    void *drcontext = dr_get_current_drcontext();
    char *data = symbolString->data;
    dr_thread_free(drcontext, data, symbolString->length);

    symbolString->data = NULL;
    symbolString->length = 0;
}

bool getDefaultSymbolString(char **stringPtr, size_t *lengthPtr)
{
    char *defaultString = "? ??:0";

    size_t stringLength = strlen(defaultString) + 1;

    void *drcontext = dr_get_current_drcontext();
    char *string = dr_thread_alloc(drcontext, stringLength);
    if (string == NULL) {
        return false;
    }

    dr_snprintf(string, stringLength, "%s", defaultString);
    string[stringLength - 1] = '\0';

    *stringPtr = string;
    *lengthPtr = stringLength;

    return true;
}

bool getSymbolString(app_pc addr, SymbolString *symbolString)
{
    #define MAX_SYM_RESULT 256
    char name[MAX_SYM_RESULT];
    char file[MAXIMUM_PATH];

    module_data_t *data = dr_lookup_module(addr);
    if (data == NULL) {
        char *string;
        size_t length;

        if (!getDefaultSymbolString(&string, &length)) {
            return false;
        }

        symbolString->data = string;
        symbolString->length = length;

        return true;
    }

    drsym_info_t sym;
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;

    drsym_error_t symres = drsym_lookup_address(data->full_path, addr - data->start, &sym, DRSYM_DEFAULT_FLAGS);

    void *drcontext = dr_get_current_drcontext();
    char *string;
    size_t maxStringLength;
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        const char *modname = dr_module_preferred_name(data);
        if (modname == NULL) {
            modname = "<noname>";
        }

        size_t addrSize = sizeof(app_pc) + 2; // 0x...

        // addr, ' ', modname, ':', (addr - data->start), '!', sym.name, '+', (addr - data->start - sym.start_offs), '\0'
        maxStringLength = (3 * addrSize) + strlen(modname) + strlen(sym.name) + addrSize + 5;
        string = dr_thread_alloc(drcontext, maxStringLength);
        if (string == NULL) {
            dr_free_module_data(data);
            return false;
        }

        dr_snprintf(string, maxStringLength, PFX " %s:" PIFX "!%s+" PIFX, addr, modname, (addr - data->start), sym.name, (addr - data->start - sym.start_offs));
        string[maxStringLength - 1] = '\0';
    } else {
        if (!getDefaultSymbolString(&string, &maxStringLength)) {
            dr_free_module_data(data);
            return false;
        }
    }

    dr_free_module_data(data);

    symbolString->data = string;
    symbolString->length = maxStringLength;

    return true;
}

static void initHeapList(HeapList *heapList)
{
    DR_ASSERT(heapList != NULL);

    heapList->head = NULL;
    heapList->tail = NULL;
    heapList->size = 0;
}

// static void printHeapList(HeapList *heapList)
// {
//     DR_ASSERT(heapList != NULL);

//     dr_fprintf(STDERR, "heapList->head = %p\n", heapList->head);
//     dr_fprintf(STDERR, "heapList->tail = %p\n", heapList->tail);
//     dr_fprintf(STDERR, "heapList->size = %ld\n", heapList->size);

//     HeapNode *node = heapList->head;
//     while (node != NULL) {
//         dr_fprintf(STDERR, "%p (%ld)", node->address, node->size);
//         if (node == heapList->tail) {
//             dr_fprintf(STDERR, "\n");
//         } else {
//             dr_fprintf(STDERR, "->");
//         }
//         node = node->next;
//     }
// }

static void addNodeToHeapList(HeapList *heapList, HeapNode *node)
{
    DR_ASSERT(heapList != NULL && node != NULL);

    if (heapList->size == 0) {
        // heapList is empty
        DR_ASSERT(heapList->head == NULL && heapList->tail == NULL);

        heapList->head = node;
        heapList->tail = node;
    } else {
        DR_ASSERT(heapList->head != NULL && heapList->tail != NULL);

        node->prev = heapList->tail;
        node->prev->next = node;
        heapList->tail = node;
    }

    heapList->size += 1;
}

static bool removeNodeFromHeapList(HeapList *heapList, HeapNode *node)
{
    DR_ASSERT(heapList != NULL && node != NULL);

    HeapNode *curNode = heapList->head;
    while (curNode != NULL) {
        if (curNode == node) {
            HeapNode *prevNode = curNode->prev;
            if (prevNode == NULL) {
                // curNode is head
                DR_ASSERT(curNode == heapList->head);
                heapList->head = curNode->next;
            } else {
                DR_ASSERT(prevNode != heapList->head);
                prevNode->next = curNode->next;
            }

            HeapNode *nextNode = curNode->next;
            if (nextNode == NULL) {
                // curNode is tail
                DR_ASSERT(curNode == heapList->tail);
                heapList->tail = curNode->prev;
            } else {
                DR_ASSERT(curNode != heapList->tail);
                nextNode->prev = curNode->prev;
            }

            DR_ASSERT(heapList->size >= 1);
            heapList->size -= 1;

            return true;
        }

        curNode = curNode->next;
    }

    return false;
}

static HeapNode *findNodeInHeapList(HeapList *heapList, void *address)
{
    DR_ASSERT(heapList != NULL);

    HeapNode *node = heapList->head;
    while (node != NULL) {
        if (node->address == address) {
            return node;
        }

        node = node->next;
    }
    
    return NULL;
}

/**
 * Push node to CallStack.
 * 
 * @param[in] callStack The pointer to the CallStack struct.
 * @param[in] node The pointer to the CallNode struct.
 * 
 * @pre callStack != NULL && node != NULL
*/
static void pushNodeToCallStack(CallStack *callStack, CallNode *node)
{
    DR_ASSERT(callStack != NULL && node != NULL);

    node->next = callStack->head;

    callStack->head = node;
    callStack->size += 1;
}

/**
 * Pop node from CallStack.
 * 
 * @param[in] callStack The pointer to the CallStack struct.
 * @return Returns NULL if there is nothing to pop, otherwise, return pointer to the CallNode struct. The CallNode needs to be freed by caller.
 * 
 * @pre callStack != NULL
*/
static CallNode *popNodeFromCallStack(CallStack *callStack)
{
    DR_ASSERT(callStack != NULL);

    CallNode *node = callStack->head;
    if (node == NULL) {
        return NULL;
    }

    callStack->head = node->next;

    DR_ASSERT(callStack->size >= 1);
    callStack->size -= 1;

    return node;
}

static bool pushCall(reg_t sp, app_pc pc)
{
    void *drcontext = dr_get_current_drcontext();
    ThreadContext *threadContext = drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);

    CallStack *callStack = threadContext->callStack;
    DR_ASSERT(callStack != NULL);

    //dr_fprintf(STDERR, "[Thread %u] callStack = %p (size: %ld)\n", threadContext->thread_id, threadContext->callStack, threadContext->callStack->size);

    CallNode *node = dr_thread_alloc(drcontext, sizeof(CallNode));
    if (node == NULL) {
        return false;
    }

    // SP after call
    reg_t next_sp = sp - sizeof(app_pc);

    instr_t instr;
    instr_init(drcontext, &instr);
    app_pc res = decode(drcontext, pc, &instr);
    DR_ASSERT(res != NULL);
    instr_free(drcontext, &instr);

    // Return address
    app_pc return_address = pc + instr_length(drcontext, &instr);

    node->sp = next_sp;
    node->value = return_address;
    node->next = NULL;
    
    pushNodeToCallStack(callStack, node);

    return true;
}

/**
 * Check if return address match the shadow stack.
 * 
 * @param[in] sp The current stack pointer.
 * @param[in] target_addr The target address the instruction will jump to.
 * @param[out] hasLongJmpPtr Pointer to a bool variable indicating if a longjmp is detected.
 * @return A `CheckReturnResult` value.
 * 
 * @post `*hasLongJmpPtr` will contain `true` or `false` if return result is `SUCCESS`, otherwise, `*hasLongJmpPtr` is undefined.
*/
static CheckReturnResult checkReturn(reg_t sp, app_pc target_addr, bool *hasLongJmpPtr)
{
    void *drcontext = dr_get_current_drcontext();
    ThreadContext *threadContext = drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);

    CallStack *callStack = threadContext->callStack;
    DR_ASSERT(callStack != NULL);

    bool found = false;
    bool hasLongJmp = false;
    CallNode *node;
    while (!found) {
        node = popNodeFromCallStack(callStack);
        if (node == NULL) {
            return EMPTY_CALLSTACK;
        }

        if (node->sp > sp) {
            pushNodeToCallStack(callStack, node); // Push node back to callstack
            return SP_NOT_FOUND;
        }

        DR_ASSERT(node != NULL && node->sp <= sp);

        if (node->sp == sp) {
            found = true;
            continue;
        }

        hasLongJmp = true;
        
        //dr_fprintf(STDERR, "Removing node(sp=0x%lx, value=" PFX "), sp=0x%lx\n", node->sp, node->value, sp);
        dr_thread_free(drcontext, node, sizeof(CallNode));
    }

    DR_ASSERT(node != NULL);

    app_pc return_address = node->value;
    dr_thread_free(drcontext, node, sizeof(CallNode));

    if (return_address == target_addr) {
        *hasLongJmpPtr = hasLongJmp;
        return SUCCESS;
    }

    return FAIL;
}

static void at_call(app_pc instr_addr, app_pc target_addr)
{
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
    
    //dr_fprintf(STDERR, "CALL @ " PFX " to " PFX ", TOS is " PFX "\n", instr_addr, target_addr, mc.xsp);

    if (!pushCall(mc.xsp, instr_addr)) {
        dr_abort();
    }
}

static void at_call_ind(app_pc instr_addr, app_pc target_addr)
{
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);

    //dr_fprintf(STDERR, "CALL INDIRECT @ " PFX " to " PFX "\n", instr_addr, target_addr);
    
    if (!pushCall(mc.xsp, instr_addr)) {
        dr_abort();
    }
}

static void at_return(app_pc instr_addr, app_pc target_addr)
{
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);

    //dr_fprintf(STDERR, "RETURN @ " PFX " to " PFX ", TOS is " PFX "\n", instr_addr, target_addr, mc.xsp);

    SymbolString symbolString;
    bool success = getSymbolString(instr_addr, &symbolString);
    if (!success) {
        dr_abort();
    }

    bool hasLongJmp;
    CheckReturnResult res = checkReturn(mc.xsp, target_addr, &hasLongJmp);
    switch (res) {
        case EMPTY_CALLSTACK:
            dr_fprintf(STDERR, "Empty call stack @ %s, SP=" PFX "\n", symbolString.data, mc.xsp);
            break;

        case SP_NOT_FOUND:
            dr_fprintf(STDERR, "Skipping check for instruction @ %s, SP=" PFX "\n", symbolString.data, mc.xsp);
            break;

        case SUCCESS:
            if (hasLongJmp) {
                dr_fprintf(STDERR, "longjmp detected @ %s\n", symbolString.data);
            }
            break;
            
        case FAIL:
            dr_fprintf(STDERR, "!!!Stack Overflow Detected @ %s\n", symbolString.data);
            dr_abort();
            break;

        default:
            DR_ASSERT(false); // Should not be here
    }

    freeSymbolString(&symbolString);
}

static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    /* instrument calls and returns -- ignore far calls/rets */
    if (instr_is_call_direct(instr)) {
        dr_insert_call_instrumentation(drcontext, bb, instr, (app_pc) at_call);
    } else if (instr_is_call_indirect(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc) at_call_ind,
                                      SPILL_SLOT_1);
    } else if (instr_is_return(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc) at_return,
                                      SPILL_SLOT_1);
    }

    return DR_EMIT_DEFAULT;
}
