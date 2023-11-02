#include <stack>

#include "dr_defines.h"
#include "dr_api.h"

#include "callnode.h"

#ifndef THREADCONTEXT_H
#define THREADCONTEXT_H

class ThreadContext {
private:
    void *_drcontext;
    thread_id_t _threadId;
    std::stack<CallNode *> _callStack;

public:
    ThreadContext(void *);
    ~ThreadContext();
    thread_id_t getThreadId();
    std::stack<CallNode *> *getCallStack();
};

#endif
