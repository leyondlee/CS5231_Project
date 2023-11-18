#include "detector.h"

static int tls_idx;
static std::list<HeapNode *> heapList;
static std::unordered_map<uint64, CfgNode *> cfgMap;

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    if (argc != 2) {
        dr_fprintf(STDERR, "DynamoRIO Client Usage: -c %s <CFG Filename>\n", argv[0]);
        dr_abort();
    }

    const char *cfgFilename = argv[1];
    if (!dr_file_exists(cfgFilename)) {
        dr_fprintf(STDERR, "File does not exist - %s\n", cfgFilename);
        dr_abort();
    }

    file_t file = dr_open_file(cfgFilename, DR_FILE_READ);
    if (file == INVALID_FILE) {
        dr_fprintf(STDERR, "Unable to open file - %s\n", cfgFilename);
        dr_abort();
    }

    std::string data;
    char buffer[BUFFER_SIZE];
    size_t readSize = BUFFER_SIZE - 1;
    size_t size;
    do {
        size = dr_read_file(file, buffer, readSize);
        if (size <= 0) {
            continue;
        }

        DR_ASSERT(size <= readSize);
        buffer[size] = '\0';

        data += buffer;
    } while (size == readSize);
    dr_close_file(file);

    std::vector<std::string> *lines = splitString(data, "\n");
    for (auto line : *lines) {
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> *lineSplit = splitString(trim(line), " ", 1);
        DR_ASSERT(lineSplit->size() == 2);

        std::string offsetStr = lineSplit->at(0);
        std::string edgesStr = lineSplit->at(1);

        char *endptr;
        uint64 offset = strtoull(offsetStr.c_str(), &endptr, 16);
        DR_ASSERT(!(offset == ULONG_MAX && errno == ERANGE));

        DR_ASSERT(cfgMap.find(offset) == cfgMap.end());
        CfgNode *node = new CfgNode(offset);

        std::vector<std::string> *edgesStrSplit = splitString(edgesStr, ",");
        for (auto edgeStr : *edgesStrSplit) {
            std::vector<std::string> *edgeStrSplit = splitString(edgeStr, ":", 1);
            DR_ASSERT(edgeStrSplit->size() == 2);

            std::string type = edgeStrSplit->at(0);
            std::string value = edgeStrSplit->at(1);

            DR_ASSERT(!value.empty());

            if (type == "O") {
                char *endptr;
                uint64 valueOffset = strtoull(value.c_str(), &endptr, 16);
                DR_ASSERT(!(valueOffset == ULONG_MAX && errno == ERANGE));

                node->addOffsetEdge(valueOffset);
            } else if (type == "S") {
                std::vector<std::string> *valueSplit = splitString(value, "::", 1);
                DR_ASSERT(edgeStrSplit->size() <= 2);

                std::string name;
                std::string library;
                if (valueSplit->size() == 2) {
                    name = valueSplit->at(1);
                    library = valueSplit->at(0);
                } else {
                    name = valueSplit->at(0);
                    library = "";
                }
                
                node->addSymbolEdge(name, library);

                delete valueSplit;
            } else {
                // Unknown type
                DR_ASSERT(false);
            }

            delete edgeStrSplit;
        }
        delete edgesStrSplit;
        delete lineSplit;

        cfgMap[offset] = node;
    }
    delete lines;

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
        delete node;
    }

    for (auto pair : cfgMap) {
        delete pair.second;
    }

    drmgr_unregister_tls_field(tls_idx);

    drwrap_exit();
    drsym_exit();
    drmgr_exit();
}

static void event_thread_init(void *drcontext)
{
    ThreadContext *threadContext = new ThreadContext(drcontext);

    //printf("[%d] New Thread with ID %d\n", dr_get_process_id(), threadContext->getThreadId());

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, threadContext);
}

static void event_thread_exit(void *drcontext)
{
    ThreadContext *threadContext = (ThreadContext *) drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);
    
    delete threadContext;
}

static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace, bool translating, void *user_data)
{
    if (instr_is_call_direct(instr)) {
        dr_insert_call_instrumentation(drcontext, bb, instr, (app_pc) at_call);
    } else if (instr_is_call_indirect(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc) at_call_ind, SPILL_SLOT_1);
    } else if (instr_is_return(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc) at_return, SPILL_SLOT_1);
    } else if (instr_is_mbr(instr) && isInstrIndirectJump(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc) at_jump_ind, SPILL_SLOT_1);
    }

    return DR_EMIT_DEFAULT;
}

static void at_call(app_pc instr_addr, app_pc target_addr)
{
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
    
    //dr_fprintf(STDERR, "CALL @ " PFX " to " PFX ", TOS is " PFX "\n", instr_addr, target_addr, mc.xsp);

    if (!pushCall(instr_addr, mc.xsp)) {
        dr_abort();
    }
}

static void at_call_ind(app_pc instr_addr, app_pc target_addr)
{
    dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL /*only need xsp*/ };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);

    //dr_fprintf(STDERR, "CALL INDIRECT @ " PFX " to " PFX "\n", instr_addr, target_addr);
    //dr_fprintf(STDERR, "Indirect call @ %s to %s, checkCfg=%d\n", getSymbolString(instr_addr).c_str(), getSymbolString(target_addr).c_str(), checkCfg(instr_addr, target_addr));

    processIndirectJump(instr_addr, target_addr);
    
    if (!pushCall(instr_addr, mc.xsp)) {
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
                dr_fprintf(STDERR, "Detected longjmp @ %s\n", symbolString.c_str());
            }
            break;
            
        case FAIL:
            dr_fprintf(STDERR, "!!!Stack Overflow Detected @ %s\n", symbolString.c_str());
            printCallTrace();
            dr_abort();
            break;

        default:
            DR_ASSERT(false); // Should not be here
    }
}

static void at_jump_ind(app_pc instr_addr, app_pc target_addr)
{
    //dr_fprintf(STDERR, "Indirect jump @ %s to %s, checkCfg=%d\n", getSymbolString(instr_addr).c_str(), getSymbolString(target_addr).c_str(), checkCfg(instr_addr, target_addr));
    processIndirectJump(instr_addr, target_addr);
}

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

    app_pc calloc_address = (app_pc) dr_get_proc_address(mod->handle, CALLOC_ROUTINE_NAME);
    if (calloc_address != NULL) {
        bool ok = drwrap_wrap(calloc_address, wrap_calloc_pre, wrap_calloc_post);
        if (!ok) {
            dr_fprintf(STDERR,
                        "<FAILED to wrap " CALLOC_ROUTINE_NAME " @" PFX
                        ": already wrapped?\n",
                        calloc_address);
        }
    }

    app_pc realloc_address = (app_pc) dr_get_proc_address(mod->handle, REALLOC_ROUTINE_NAME);
    if (realloc_address != NULL) {
        bool ok = drwrap_wrap(realloc_address, wrap_realloc_pre, wrap_realloc_post);
        if (!ok) {
            dr_fprintf(STDERR,
                        "<FAILED to wrap " REALLOC_ROUTINE_NAME " @" PFX
                        ": already wrapped?\n",
                        realloc_address);
        }
    }

    app_pc reallocarray_address = (app_pc) dr_get_proc_address(mod->handle, REALLOCARRAY_ROUTINE_NAME);
    if (reallocarray_address != NULL) {
        bool ok = drwrap_wrap(reallocarray_address, wrap_reallocarray_pre, wrap_reallocarray_post);
        if (!ok) {
            dr_fprintf(STDERR,
                        "<FAILED to wrap " REALLOCARRAY_ROUTINE_NAME " @" PFX
                        ": already wrapped?\n",
                        reallocarray_address);
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

    HeapNode *node = new HeapNode(address, size);
    addNodeToHeapList(&heapList, node);

    //dr_fprintf(STDERR, "[malloc] Address: %p, Size: %ld\n", address, size);
}

static void wrap_calloc_pre(void *wrapcxt, OUT void **user_data)
{
    size_t nmemb = (size_t) drwrap_get_arg(wrapcxt, 0);
    size_t size = (size_t) drwrap_get_arg(wrapcxt, 1);

    CallocArguments *callocArguments = (CallocArguments *) dr_thread_alloc(dr_get_current_drcontext(), sizeof(CallocArguments));
    DR_ASSERT(callocArguments != NULL);

    callocArguments->nmemb = nmemb;
    callocArguments->size = size;

    *user_data = (void *) callocArguments;
}

static void wrap_calloc_post(void *wrapcxt, void *user_data)
{
    void *address = drwrap_get_retval(wrapcxt);
    if (address == NULL) {
        return;
    }

    CallocArguments *callocArguments = (CallocArguments *) user_data;
    DR_ASSERT(callocArguments != NULL);

    size_t nmemb = callocArguments->nmemb;
    size_t size = callocArguments->size;
    dr_thread_free(dr_get_current_drcontext(), callocArguments, sizeof(CallocArguments));

    HeapNode *node = new HeapNode(address, nmemb * size);
    addNodeToHeapList(&heapList, node);

    //dr_fprintf(STDERR, "[calloc] Address: %p, nmemb = %ld, size = %ld\n", address, nmemb, size);
}

static void wrap_realloc_pre(void *wrapcxt, OUT void **user_data)
{
    void *ptr = (void *) drwrap_get_arg(wrapcxt, 0);
    size_t size = (size_t) drwrap_get_arg(wrapcxt, 1);

    if (ptr != NULL && findNodeInHeapList(&heapList, ptr) == nullptr) {
        dr_fprintf(STDERR, "Using reallocarray on unallocated memory: %p\n", ptr);
        printCallTrace();
        dr_abort();
    }

    ReallocArguments *reallocArguments = (ReallocArguments *) dr_thread_alloc(dr_get_current_drcontext(), sizeof(ReallocArguments));
    DR_ASSERT(reallocArguments != NULL);

    reallocArguments->ptr = ptr;
    reallocArguments->size = size;

    *user_data = (void *) reallocArguments;
}

static void wrap_realloc_post(void *wrapcxt, void *user_data)
{
    void *address = drwrap_get_retval(wrapcxt);
    if (address == NULL) {
        return;
    }

    ReallocArguments *reallocArguments = (ReallocArguments *) user_data;
    DR_ASSERT(reallocArguments != NULL);

    void *ptr = reallocArguments->ptr;
    size_t size = reallocArguments->size;
    dr_thread_free(dr_get_current_drcontext(), reallocArguments, sizeof(ReallocArguments));

    if (ptr != NULL) {
        HeapNode *node = findNodeInHeapList(&heapList, ptr);
        DR_ASSERT(node != nullptr);

        bool isRemoved = removeNodeFromHeapList(&heapList, node);
        DR_ASSERT(isRemoved);

        delete node;
    }

    HeapNode *node = new HeapNode(address, size);
    addNodeToHeapList(&heapList, node);

    //dr_fprintf(STDERR, "[realloc] Address: %p, ptr = %p, size = %ld\n", address, ptr, size);
}

static void wrap_reallocarray_pre(void *wrapcxt, OUT void **user_data)
{
    void *ptr = (void *) drwrap_get_arg(wrapcxt, 0);
    size_t nmemb = (size_t) drwrap_get_arg(wrapcxt, 1);
    size_t size = (size_t) drwrap_get_arg(wrapcxt, 2);

    if (ptr != NULL && findNodeInHeapList(&heapList, ptr) == nullptr) {
        dr_fprintf(STDERR, "Using reallocarray on unallocated memory: %p\n", ptr);
        printCallTrace();
        dr_abort();
    }

    ReallocarrayArguments *reallocarrayArguments = (ReallocarrayArguments *) dr_thread_alloc(dr_get_current_drcontext(), sizeof(ReallocarrayArguments));
    DR_ASSERT(reallocarrayArguments != NULL);

    reallocarrayArguments->ptr = ptr;
    reallocarrayArguments->nmemb = nmemb;
    reallocarrayArguments->size = size;

    *user_data = (void *) reallocarrayArguments;
}

static void wrap_reallocarray_post(void *wrapcxt, void *user_data)
{
    void *address = drwrap_get_retval(wrapcxt);
    if (address == NULL) {
        return;
    }

    ReallocarrayArguments *reallocarrayArguments = (ReallocarrayArguments *) user_data;
    DR_ASSERT(reallocarrayArguments != NULL);

    void *ptr = reallocarrayArguments->ptr;
    size_t nmemb = reallocarrayArguments->nmemb;
    size_t size = reallocarrayArguments->size;
    dr_thread_free(dr_get_current_drcontext(), reallocarrayArguments, sizeof(ReallocarrayArguments));

    if (ptr != NULL) {
        HeapNode *node = findNodeInHeapList(&heapList, ptr);
        DR_ASSERT(node != nullptr);

        bool isRemoved = removeNodeFromHeapList(&heapList, node);
        DR_ASSERT(isRemoved);

        delete node;
    }

    HeapNode *node = new HeapNode(address, nmemb * size);
    addNodeToHeapList(&heapList, node);

    //dr_fprintf(STDERR, "[reallocarray] Address: %p, ptr = %p, nmemb = %ld, size = %ld\n", address, ptr, nmemb, size);
}

static void wrap_free_pre(void *wrapcxt, OUT void **user_data)
{
    void *ptr = drwrap_get_arg(wrapcxt, 0);
    if (ptr == NULL) {
        return;
    }

    HeapNode *node = findNodeInHeapList(&heapList, ptr);
    if (node == nullptr) {
        dr_fprintf(STDERR, "Freeing unallocated memory: " PFX "\n", ptr);
        printCallTrace();
        dr_abort();
    }
    DR_ASSERT(node->getAddress() == ptr);

    bool isRemoved = removeNodeFromHeapList(&heapList, node);
    DR_ASSERT(isRemoved);

    //dr_fprintf(STDERR, "[free] ptr: %p\n", ptr);

    delete node;
}

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
    
    return nullptr;
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
 * @return Returns nullptr if there is nothing to pop, otherwise, return pointer to the CallNode struct. The CallNode needs to be freed by caller.
 * 
 * @pre callStack != NULL
*/
static CallNode *popNodeFromCallStack(std::stack<CallNode *> *callStack)
{
    DR_ASSERT(callStack != NULL);

    if (callStack->empty()) {
        return nullptr;
    }

    CallNode *node = callStack->top();
    callStack->pop();

    return node;
}

static bool pushCall(app_pc pc, reg_t sp)
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

    CallNode *node = new CallNode(pc, next_sp, return_address);
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
        if (node == nullptr) {
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
        delete node;
    }

    DR_ASSERT(node != NULL && node->getSp() == sp);

    app_pc return_address = node->getValue();

    if (return_address == target_addr) {
        delete node;
        *hasLongJmpPtr = hasLongJmp;
        return SUCCESS;
    }

    pushNodeToCallStack(callStack, node); // Push node back to callstack

    return FAIL;
}

static CheckCfgResult checkCfg(app_pc instr_addr, app_pc target_addr)
{
    SymbolInfo *symbolInfo = getSymbolInfo(instr_addr);
    if (symbolInfo == nullptr) {
        return UNKNOWN_MODULE;
    }

    std::string moduleName = symbolInfo->getModuleName();
    if (moduleName.empty()) {
        delete symbolInfo;
        return UNKNOWN_MODULE;
    }

    std::string appname = std::string(dr_get_application_name());
    if (moduleName != appname) {
        delete symbolInfo;
        return DIFFERENT_MODULE;
    }

    std::unordered_map<uint64, CfgNode *>::const_iterator it = cfgMap.find(symbolInfo->getModuleRelativeOffset());
    if (it == cfgMap.end()) {
        delete symbolInfo;
        return CFGNODE_NOT_FOUND;
    }

    DR_ASSERT(it->first == symbolInfo->getModuleRelativeOffset());
    CfgNode *node = it->second;

    SymbolInfo *targetSymbolInfo = getSymbolInfo(target_addr);
    if (targetSymbolInfo == nullptr) {
        delete symbolInfo;
        return UNKNOWN_TARGET;
    }

    std::string targetModuleName = targetSymbolInfo->getModuleName();
    if (targetModuleName.empty()) {
        delete targetSymbolInfo;
        delete symbolInfo;
        return UNKNOWN_TARGET;
    }

    CheckCfgResult res = CFGEDGE_NOT_FOUND;
    if (targetModuleName == moduleName) {
        // Target within same binary
        if (node->hasOffsetEdge(targetSymbolInfo->getModuleRelativeOffset())) {
            res = CFGEDGE_FOUND;
        }
    } else {
        // External target
        if (targetSymbolInfo->getSymbolRelativeOffset() == 0) {
            // Start of function
            if (node->hasSymbolEdge(targetSymbolInfo->getSymbolName(), targetModuleName, true)) {
                // Found similar name
                res = CFGEDGE_FOUND;
            }
        } else {
            // Jumping to middle of function (possibly ROP)
            res = NOT_BEGINNING;
        }
    }

    delete targetSymbolInfo;
    delete symbolInfo;

    return res;
}

static void processIndirectJump(app_pc instr_addr, app_pc target_addr)
{
    CheckCfgResult res = checkCfg(instr_addr, target_addr);
    switch (res) {
        case UNKNOWN_MODULE: // Cannot determine instr_addr module
            // Fallthrough

        case DIFFERENT_MODULE: // instr_addr is not within same binary (eg. Shared library)
            // Fallthrough

        case UNKNOWN_TARGET: // Cannot determine target_addr module (eg. Jumping into private mmap region)
            // Fallthrough

        case CFGNODE_NOT_FOUND: // Static analysis did find any edges for instr_addr
            return; // Pass for now until we find a better way to handle

        case NOT_BEGINNING:
            // Fallthrough

        case CFGEDGE_NOT_FOUND: // target_addr did not match any valid edges
            dr_fprintf(STDERR, "Invalid edge detect @ %s to %s\n", getSymbolString(instr_addr).c_str(), getSymbolString(target_addr).c_str());
            printCallTrace();
            dr_abort();

        case CFGEDGE_FOUND: // target_addr match a valid edge
            return;
    }
}

static void printCallTrace()
{
    void *drcontext = dr_get_current_drcontext();
    ThreadContext *threadContext = (ThreadContext *) drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(threadContext != NULL);

    std::stack<CallNode *> *callStack = threadContext->getCallStack();

    dr_fprintf(STDERR, "Call Trace:\n");

    CallNode *node;
    while ((node = popNodeFromCallStack(callStack)) != nullptr) {
        dr_fprintf(STDERR, "#%ld  %s\n", callStack->size(), getSymbolString(node->getPc()).c_str());

        delete node;
    }
}

static SymbolInfo *getSymbolInfo(app_pc addr)
{
    #define MAX_SYM_RESULT 256
    char name[MAX_SYM_RESULT];
    char file[MAXIMUM_PATH];

    module_data_t *data = dr_lookup_module(addr);
    if (data == NULL) {
        return nullptr;
    }

    drsym_info_t sym;
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;

    drsym_error_t symres = drsym_lookup_address(data->full_path, addr - data->start, &sym, DRSYM_DEFAULT_FLAGS);

    std::string moduleName = "";
    const char *modname = dr_module_preferred_name(data);
    if (modname != NULL) {
        moduleName = std::string(modname);
    }

    uint64 moduleRelativeOffset = addr - data->start;

    std::string symbolName = "";
    uint64 symbolRelativeOffset = -1;
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        symbolName = std::string(sym.name);
        symbolRelativeOffset = addr - data->start - sym.start_offs;
    }

    dr_free_module_data(data);

    SymbolInfo *symbolInfo = new SymbolInfo(moduleName, moduleRelativeOffset, symbolName, symbolRelativeOffset);

    return symbolInfo;
}

static std::string getSymbolString(app_pc addr)
{
    std::stringstream ss;

    SymbolInfo *symbolInfo = getSymbolInfo(addr);
    if (symbolInfo == nullptr) {
        ss << "0x" << std::hex << reinterpret_cast<intptr_t>(addr) << " ??:0";

        return ss.str();
    }

    std::string moduleName = symbolInfo->getModuleName();
    if (moduleName.empty()) {
        moduleName = "<noname>";
    }

    std::string symbolName = symbolInfo->getSymbolName();
    if (symbolName.empty()) {
        ss << "0x" << std::hex << reinterpret_cast<intptr_t>(addr) << " " << moduleName << ":0x" << std::hex << symbolInfo->getModuleRelativeOffset();
    } else {
        ss << "0x" << std::hex << reinterpret_cast<intptr_t>(addr) << " " << moduleName << ":0x" << std::hex << symbolInfo->getModuleRelativeOffset() << "!" << symbolName << "+0x" << std::hex << symbolInfo->getSymbolRelativeOffset();
    }

    delete symbolInfo;

    return ss.str();
}

static bool isInstrIndirectJump(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    if (opcode == OP_jmp_ind || opcode == OP_jmp_far_ind) {
        return true;
    }

    return false;
}

static std::vector<std::string> *splitString(std::string data, std::string delim, std::size_t count)
{
    std::vector<std::string> *vector = new std::vector<std::string>();
    
    size_t delimSize = delim.size();

    size_t i = 0;
    std::string dataLeft = data;
    while (!dataLeft.empty()) {
        if (i >= count) {
            vector->push_back(dataLeft);
            dataLeft = "";
            continue;
        }

        std::size_t pos = dataLeft.find(delim);
        std::string str = dataLeft.substr(0, pos);
        vector->push_back(str);
        i += 1;

        if (pos == std::string::npos) {
            dataLeft = "";
            continue;
        }

        dataLeft = dataLeft.substr(pos + delimSize);
        if (dataLeft.empty()) {
            vector->push_back("");
        }
    }

    return vector;
}
 
static std::string ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}
 
static std::string rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static std::string trim(const std::string &s)
{
    return ltrim(rtrim(s));
}
