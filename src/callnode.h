#include "dr_defines.h"

#ifndef CALLNODE_H
#define CALLNODE_H

class CallNode {
private:
    app_pc _pc;
    reg_t _bp;
    reg_t _sp;
    app_pc _return_address;

public:
    CallNode(app_pc pc, reg_t sp, reg_t bp, app_pc return_address);
    app_pc getPc();
    reg_t getSp();
    reg_t getBp();
    app_pc getReturnAddress();
};

#endif
