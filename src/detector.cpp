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
static void addNodeToHeapList(std::list<HeapNode *> *, HeapNode *);
static bool removeNodeFromHeapList(std::list<HeapNode *> *, HeapNode *);
static HeapNode *findNodeInHeapList(std::list<HeapNode *> *, void *);

static int tls_idx;
static std::list<HeapNode *> heapList;

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
    void *address = drwrap_get_retval(wrapcxt);
    if (address == NULL) {
        return;
    }

    size_t size = (size_t) user_data;

    HeapNode *node = (HeapNode *) dr_global_alloc(sizeof(HeapNode));
    if (node == NULL) {
        // Fail to allocate memory
        dr_abort();
    }
    new (node) HeapNode(address, size);

    addNodeToHeapList(&heapList, node);

    //dr_fprintf(STDERR, "[malloc] Address: %p, Size: %ld\n", node->getAddress(), node->size);
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
    DR_ASSERT(node->getAddress() == ptr);

    bool isRemoved = removeNodeFromHeapList(&heapList, node);
    DR_ASSERT(isRemoved);

    //dr_fprintf(STDERR, "[free] ptr: %p\n", node->getAddress());

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
}

static void event_exit(void)
{
    for (auto node : heapList) {
        dr_global_free(node, sizeof(HeapNode));
    }

    drmgr_unregister_tls_field(tls_idx);
    drwrap_exit();
    drsym_exit();
    drmgr_exit();
}

static void event_thread_init(void *drcontext)
{
    ThreadContext *threadContext = (ThreadContext *) dr_thread_alloc(drcontext, sizeof(ThreadContext));
    if (threadContext == NULL) {
        dr_abort();
    }
    new (threadContext) ThreadContext(drcontext);

    //printf("[%d] New Thread with ID %d\n", dr_get_process_id(), threadContext->getThreadId());

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, threadContext);
}

static void event_thread_exit(void *drcontext)
{
    ThreadContext *threadContext = (ThreadContext *) drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);
    
    threadContext->~ThreadContext();
    dr_thread_free(drcontext, threadContext, sizeof(ThreadContext));
}

std::string getSymbolString(app_pc addr)
{
    #define MAX_SYM_RESULT 256
    char name[MAX_SYM_RESULT];
    char file[MAXIMUM_PATH];

    module_data_t *data = dr_lookup_module(addr);
    if (data == NULL) {
        return std::string("? ??:0");
    }

    drsym_info_t sym;
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;

    drsym_error_t symres = drsym_lookup_address(data->full_path, addr - data->start, &sym, DRSYM_DEFAULT_FLAGS);

    std::string symbolString;
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        const char *modname = dr_module_preferred_name(data);
        if (modname == NULL) {
            modname = "<noname>";
        }

        std::stringstream ss;
        ss << "0x" << std::hex << reinterpret_cast<intptr_t>(addr) << " " << modname << ":0x" << std::hex << (addr - data->start) << "!" << sym.name << "+0x" << std::hex << (addr - data->start - sym.start_offs);
        
        symbolString = ss.str();
    } else {
        symbolString = "? ??:0";
    }

    dr_free_module_data(data);

    return symbolString;
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

static void addNodeToHeapList(std::list<HeapNode *> *heapList, HeapNode *node)
{
    DR_ASSERT(heapList != NULL && node != NULL);

    heapList->push_back(node);
}

static bool removeNodeFromHeapList(std::list<HeapNode *> *heapList, HeapNode *node)
{
    DR_ASSERT(heapList != NULL && node != NULL);

    if (std::find(heapList->begin(), heapList->end(), node) != heapList->end()) {
        heapList->remove(node);
        return true;
    }

    return false;
}

static HeapNode *findNodeInHeapList(std::list<HeapNode *> *heapList, void *address)
{
    DR_ASSERT(heapList != NULL);

    for (auto node : *heapList) {
        if (node->getAddress() == address) {
            return node;
        }
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
static void pushNodeToCallStack(std::stack<CallNode *> *callStack, CallNode *node)
{
    DR_ASSERT(callStack != NULL && node != NULL);

    callStack->push(node);
}

/**
 * Pop node from CallStack.
 * 
 * @param[in] callStack The pointer to the CallStack struct.
 * @return Returns NULL if there is nothing to pop, otherwise, return pointer to the CallNode struct. The CallNode needs to be freed by caller.
 * 
 * @pre callStack != NULL
*/
static CallNode *popNodeFromCallStack(std::stack<CallNode *> *callStack)
{
    DR_ASSERT(callStack != NULL);

    if (callStack->empty()) {
        return NULL;
    }

    CallNode *node = callStack->top();
    callStack->pop();

    return node;
}

static bool pushCall(reg_t sp, app_pc pc)
{
    void *drcontext = dr_get_current_drcontext();

    // SP after call
    reg_t next_sp = sp - sizeof(app_pc);

    instr_t instr;
    instr_init(drcontext, &instr);
    app_pc res = decode(drcontext, pc, &instr);
    DR_ASSERT(res != NULL);
    instr_free(drcontext, &instr);

    // Return address
    app_pc return_address = pc + instr_length(drcontext, &instr);

    ThreadContext *threadContext = (ThreadContext *) drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);

    std::stack<CallNode *> *callStack = threadContext->getCallStack();

    CallNode *node = (CallNode *) dr_thread_alloc(drcontext, sizeof(CallNode));
    if (node == NULL) {
        return false;
    }
    new (node) CallNode(next_sp, return_address);

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
    ThreadContext *threadContext = (ThreadContext *) drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);

    std::stack<CallNode *> *callStack = threadContext->getCallStack();

    bool found = false;
    bool hasLongJmp = false;
    CallNode *node;
    while (!found) {
        node = popNodeFromCallStack(callStack);
        if (node == NULL) {
            return EMPTY_CALLSTACK;
        }

        if (node->getSp() > sp) {
            pushNodeToCallStack(callStack, node); // Push node back to callstack
            return SP_NOT_FOUND;
        }

        DR_ASSERT(node != NULL && node->getSp() <= sp);

        if (node->getSp() == sp) {
            found = true;
            continue;
        }

        hasLongJmp = true;
        
        //dr_fprintf(STDERR, "Removing node(sp=0x%lx, value=" PFX "), sp=0x%lx\n", node->sp, node->value, sp);
        dr_thread_free(drcontext, node, sizeof(CallNode));
    }

    DR_ASSERT(node != NULL);

    app_pc return_address = node->getValue();
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

    std::string symbolString = getSymbolString(instr_addr);

    bool hasLongJmp;
    CheckReturnResult res = checkReturn(mc.xsp, target_addr, &hasLongJmp);
    switch (res) {
        case EMPTY_CALLSTACK:
            dr_fprintf(STDERR, "Empty call stack @ %s, SP=" PFX "\n", symbolString.c_str(), mc.xsp);
            break;

        case SP_NOT_FOUND:
            dr_fprintf(STDERR, "Skipping check for instruction @ %s, SP=" PFX "\n", symbolString.c_str(), mc.xsp);
            break;

        case SUCCESS:
            if (hasLongJmp) {
                dr_fprintf(STDERR, "longjmp detected @ %s\n", symbolString.c_str());
            }
            break;
            
        case FAIL:
            dr_fprintf(STDERR, "!!!Stack Overflow Detected @ %s\n", symbolString.c_str());
            dr_abort();
            break;

        default:
            DR_ASSERT(false); // Should not be here
    }
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
