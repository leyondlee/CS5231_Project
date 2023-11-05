#include "cfgsymboledge.h"

CfgSymbolEdge::CfgSymbolEdge(std::string name, std::string library)
{
    _name = name;
    _library = library;
}

std::string CfgSymbolEdge::getName()
{
    return _name;
}

std::string CfgSymbolEdge::getLibrary()
{
    return _library;
}
