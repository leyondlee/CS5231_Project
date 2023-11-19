#include "callnode.h"

CallNode::CallNode(app_pc pc, reg_t sp, reg_t bp, app_pc return_address)
{
    _pc = pc;
    _sp = sp;
    _bp = bp;
    _return_address = return_address;
}

app_pc CallNode::getPc()
{
    return _pc;
}

reg_t CallNode::getSp()
{
    return _sp;
}

reg_t CallNode::getBp()
{
    return _bp;
}

app_pc CallNode::getReturnAddress()
{
    return _return_address;
}
