#include "threadcontext.h"

ThreadContext::ThreadContext(void *drcontext)
{
    _drcontext = drcontext;
    _threadId = dr_get_thread_id(drcontext);
}

ThreadContext::~ThreadContext()
{
    while (!_callStack.empty()) {
        CallNode *node = _callStack.top();
        _callStack.pop();

        delete node;
    }
}

thread_id_t ThreadContext::getThreadId()
{
    return _threadId;
}

std::stack<CallNode *> *ThreadContext::getCallStack()
{
    return &_callStack;
}
