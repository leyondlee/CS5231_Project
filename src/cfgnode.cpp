#include "cfgnode.h"

CfgNode::CfgNode(uint64 offset)
{
    _offset = offset;
}

CfgNode::~CfgNode()
{
    for (auto edge : _symbolEdges) {
        delete edge;
    }
}

void CfgNode::addOffsetEdge(uint64 offset)
{
    _offsetEdges.insert(offset);
}

void CfgNode::addSymbolEdge(std::string name, std::string library)
{
    CfgSymbolEdge *edge = new CfgSymbolEdge(name, library);
    _symbolEdges.insert(edge);
}

bool CfgNode::hasOffsetEdge(uint64 offset)
{
    if (_offsetEdges.find(offset) == _offsetEdges.end()) {
        return false;
    }

    return true;
}

bool CfgNode::hasSymbolEdge(std::string name, std::string library, bool findSimilarName)
{
    for (auto edge : _symbolEdges) {
        std::string edgeLibrary = edge->getLibrary();
        if (!edgeLibrary.empty() && edgeLibrary != library) {
            continue;
        }

        if (name == edge->getName()) {
            return true;
        }

        if (findSimilarName) {
            // Check if edge name is a substring of name
            if (name.find(edge->getName()) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}
