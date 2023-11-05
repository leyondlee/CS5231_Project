#include <string>

#ifndef CFGSYMBOLEDGE_H
#define CFGSYMBOLEDGE_H

class CfgSymbolEdge {
private:
    std::string _name;
    std::string _library;

public:
    CfgSymbolEdge(std::string name, std::string library);
    std::string getName();
    std::string getLibrary();
};

#endif
