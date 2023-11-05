#include "symbolinfo.h"

SymbolInfo::SymbolInfo(std::string moduleName, uint64 moduleRelativeOffset, std::string symbolName, uint64 symbolRelativeOffset)
{
    _moduleName = moduleName;
    _moduleRelativeOffset = moduleRelativeOffset;
    _symbolName = symbolName;
    _symbolRelativeOffset = symbolRelativeOffset;
}

std::string SymbolInfo::getModuleName()
{
    return _moduleName;
}

uint64 SymbolInfo::getModuleRelativeOffset()
{
    return _moduleRelativeOffset;
}

std::string SymbolInfo::getSymbolName()
{
    return _symbolName;
}

uint64 SymbolInfo::getSymbolRelativeOffset()
{
    return _symbolRelativeOffset;
}
