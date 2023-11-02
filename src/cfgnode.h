#include <string>
#include <unordered_set>

#include "dr_defines.h"

#ifndef CFGNODE_H
#define CFGNODE_H

class CfgNode {
private:
    std::string _binaryName;
    uint64 _offset;
    std::unordered_set<uint64> _offsetEdges;
    std::unordered_set<std::string> _symbolEdges;

public:
    CfgNode(char *, uint64);
    void addOffsetEdge(uint64);
    void addSymbolEdge(std::string);
    bool hasOffsetEdge(uint64);
    bool hasSymbolEdge(std::string);
};

#endif
