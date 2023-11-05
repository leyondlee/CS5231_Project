#include <string>

#include "dr_defines.h"

#ifndef SYMBOLINFO_H
#define SYMBOLINFO_H

class SymbolInfo {
private:
    std::string _moduleName;
    uint64 _moduleRelativeOffset;
    std::string _symbolName;
    uint64 _symbolRelativeOffset;

public:
    SymbolInfo(std::string moduleName, uint64 moduleRelativeOffset, std::string symbolName, uint64 symbolRelativeOffset);
    std::string getModuleName();
    uint64 getModuleRelativeOffset();
    std::string getSymbolName();
    uint64 getSymbolRelativeOffset();
};

#endif
