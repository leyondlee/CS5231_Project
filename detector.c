#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"

#include "detector.h"

static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr, bool for_trace, bool translating, void *user_data);
static int tls_idx;

DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'Detector'", "");
    drmgr_init();

    dr_fprintf(STDERR, "Client Detector is running\n");

    dr_register_exit_event(event_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);

    if (drsym_init(0) != DRSYM_SUCCESS) {
        dr_log(NULL, DR_LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n");
    }

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx > -1);
}

static void event_exit(void)
{
    drmgr_unregister_tls_field(tls_idx);
    drmgr_exit();
}

static void event_thread_init(void *drcontext)
{
    CallStack *callStack = dr_thread_alloc(drcontext, sizeof(CallStack));
    if (callStack == NULL) {
        dr_abort();
    }

    callStack->head = NULL;

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, callStack);
}

static void event_thread_exit(void *drcontext)
{
    CallStack *callStack = drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(callStack != NULL);

    CallStackItem *item = callStack->head;
    while (item != NULL) {
        CallStackItem *next = item->next;
        dr_thread_free(drcontext, item, sizeof(CallStackItem));
        item = next;
    }

    dr_thread_free(drcontext, callStack, sizeof(CallStack));
}

/**
 * Push item to CallStack.
 * 
 * @param[in] callStack The pointer to the CallStack struct.
 * @param[in] item The pointer to the CallStackItem struct.
 * 
 * @pre callStack != NULL && item != NULL
*/
static void pushItemToCallStack(CallStack *callStack, CallStackItem *item)
{
    DR_ASSERT(callStack != NULL && item != NULL);

    item->next = callStack->head;
    callStack->head = item;
}

/**
 * Returns item at the top of CallStack.
 * 
 * @param[in] callStack The pointer to the CallStack struct.
 * @return Returns NULL if there is nothing on the CallStack, otherwise, return pointer to the CallStackItem struct.
 * 
 * @pre callStack != NULL
*/
static CallStackItem *peekCallStack(CallStack *callStack)
{
    DR_ASSERT(callStack != NULL);

    CallStackItem *item = callStack->head;
    if (item == NULL) {
        return NULL;
    }

    return item;
}

/**
 * Pop item from CallStack.
 * 
 * @param[in] callStack The pointer to the CallStack struct.
 * @return Returns NULL if there is nothing to pop, otherwise, return pointer to the CallStackItem struct. The CallStackItem needs to be freed by caller.
 * 
 * @pre callStack != NULL
*/
static CallStackItem *popCallStack(CallStack *callStack)
{
    DR_ASSERT(callStack != NULL);

    CallStackItem *item = callStack->head;
    if (item == NULL) {
        return NULL;
    }

    callStack->head = item->next;
    return item;
}

static bool pushCall(reg_t sp, app_pc pc)
{
    void *drcontext = dr_get_current_drcontext();

    CallStack *callStack = drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(callStack != NULL);

    CallStackItem *item = dr_thread_alloc(drcontext, sizeof(CallStackItem));
    if (item == NULL) {
        return false;
    }

    // SP after call
    reg_t next_sp = sp - sizeof(app_pc);

    instr_t instr;
    instr_init(drcontext, &instr);
    app_pc res = decode(drcontext, pc, &instr);
    DR_ASSERT(res != NULL);

    // Return address
    app_pc return_address = pc + instr_length(drcontext, &instr);

    item->sp = next_sp;
    item->value = return_address;
    
    pushItemToCallStack(callStack, item);

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
    CallStack *callStack = drmgr_get_tls_field(drcontext, tls_idx);
    DR_ASSERT(callStack != NULL);

    bool found = false;
    bool hasLongJmp = false;
    CallStackItem *item;
    CallStackItem *prevPeekItem = NULL;
    while (!found) {
        CallStackItem *peekItem = peekCallStack(callStack);
        if (peekItem == NULL || peekItem == prevPeekItem) {
            // No items left or already peeked before
            return SP_NOT_FOUND;
        }

        if (peekItem->sp > sp) {
            // Next item and after are for previous call frames. Note: The stack grows to lower address.
            //dr_fprintf(STDERR, "peekItem->sp > sp: peekItem(sp=0x%lx, value=" PFX "), sp=0x%lx\n", peekItem->sp, peekItem->value, sp);
            return SP_NOT_FOUND;
        }

        item = popCallStack(callStack);
        DR_ASSERT(item != NULL && item == peekItem && item->sp <= sp);

        if (item->sp == sp) {
            found = true;
            continue;
        }

        hasLongJmp = true;
        
        //dr_fprintf(STDERR, "Removing item(sp=0x%lx, value=" PFX "), sp=0x%lx\n", item->sp, item->value, sp);
        dr_thread_free(drcontext, item, sizeof(CallStackItem));
    }

    DR_ASSERT(item != NULL);

    app_pc return_address = item->value;
    dr_thread_free(drcontext, item, sizeof(CallStackItem));

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

    bool hasLongJmp;
    CheckReturnResult res = checkReturn(mc.xsp, target_addr, &hasLongJmp);
    switch (res) {
        case SP_NOT_FOUND:
            dr_fprintf(STDERR, "Skipping check for instruction @ " PFX ", SP=" PFX "\n", instr_addr, mc.xsp);
            // Fallthrough

        case SUCCESS:
            if (hasLongJmp) {
                dr_fprintf(STDERR, "longjmp detected @ " PFX "\n", instr_addr);
            }
            break;

        case DECODE_FAIL:
            // Fallthrough
        case FAIL:
            dr_fprintf(STDERR, "!!!Buffer Overflow Detected @ " PFX "!!!\n", instr_addr);
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
