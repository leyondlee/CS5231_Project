#include "dr_defines.h"

#ifndef CALLNODE_H
#define CALLNODE_H

class CallNode {
private:
    reg_t _sp;
    app_pc _value;

public:
    CallNode(reg_t, app_pc);
    reg_t getSp();
    app_pc getValue();
};

#endif
