#include "dr_defines.h"

#ifndef CALLNODE_H
#define CALLNODE_H

class CallNode {
private:
    app_pc _pc;
    reg_t _sp;
    app_pc _value;

public:
    CallNode(app_pc pc, reg_t sp, app_pc value);
    app_pc getPc();
    reg_t getSp();
    app_pc getValue();
};

#endif
