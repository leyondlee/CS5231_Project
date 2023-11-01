#include <stddef.h>

#ifndef HEAPNODE_H
#define HEAPNODE_H

class HeapNode {
private:
    void *_address;
    size_t _size;

public:
    HeapNode(void *, size_t);

    void *getAddress();
    size_t getSize();
};

#endif
