#include <string>
#include <unordered_set>

#include "dr_defines.h"
#include "dr_api.h"

#include "cfgsymboledge.h"

#ifndef CFGNODE_H
#define CFGNODE_H

class CfgNode {
private:
    uint64 _offset;
    std::unordered_set<uint64> _offsetEdges;
    std::unordered_set<CfgSymbolEdge *> _symbolEdges;

public:
    CfgNode(uint64 offset);
    ~CfgNode();
    void addOffsetEdge(uint64 offset);
    void addSymbolEdge(std::string name, std::string library);
    bool hasOffsetEdge(uint64 offset);
    bool hasSymbolEdge(std::string name, std::string library, bool findSimilarName);
};

#endif
