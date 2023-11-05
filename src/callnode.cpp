#include "callnode.h"

CallNode::CallNode(app_pc pc, reg_t sp, app_pc value)
{
    _pc = pc;
    _sp = sp;
    _value = value;
}

app_pc CallNode::getPc()
{
    return _pc;
}

reg_t CallNode::getSp()
{
    return _sp;
}

app_pc CallNode::getValue()
{
    return _value;
}
