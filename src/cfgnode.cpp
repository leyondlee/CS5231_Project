#include "cfgnode.h"

CfgNode::CfgNode(char *binaryName, uint64 offset)
{
    _binaryName = binaryName;
    _offset = offset;
}

void CfgNode::addOffsetEdge(uint64 offset)
{
    _offsetEdges.insert(offset);
}

void CfgNode::addSymbolEdge(std::string symbol)
{
    _symbolEdges.insert(symbol);
}

bool CfgNode::hasOffsetEdge(uint64 offset)
{
    if (_offsetEdges.find(offset) == _offsetEdges.end()) {
        return false;
    }

    return true;
}

bool CfgNode::hasSymbolEdge(std::string symbol)
{
    if (_symbolEdges.find(symbol) == _symbolEdges.end()) {
        return false;
    }

    return true;
}
