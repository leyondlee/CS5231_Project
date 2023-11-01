#include "callnode.h"

CallNode::CallNode(reg_t sp, app_pc value)
{
    _sp = sp;
    _value = value;
}

reg_t CallNode::getSp()
{
    return _sp;
}

app_pc CallNode::getValue()
{
    return _value;
}
