#include "threadcontext.h"

ThreadContext::ThreadContext(void *drcontext)
{
    _drcontext = drcontext;
    _threadId = dr_get_thread_id(drcontext);
    _callStack = (std::stack<CallNode *> *) dr_thread_alloc(_drcontext, sizeof(std::stack<CallNode *>));
    if (_callStack == NULL) {
        dr_abort();
    }

    new (_callStack) std::stack<CallNode *>();
}

ThreadContext::~ThreadContext()
{
    while (!_callStack->empty()) {
        CallNode *node = _callStack->top();
        _callStack->pop();

        dr_thread_free(_drcontext, node, sizeof(CallNode));
    }

    dr_thread_free(_drcontext, _callStack, sizeof(std::stack<CallNode *>));
}

thread_id_t ThreadContext::getThreadId()
{
    return _threadId;
}

std::stack<CallNode *> *ThreadContext::getCallStack()
{
    return _callStack;
}
