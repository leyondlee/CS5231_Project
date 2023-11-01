#include "heapnode.h"

HeapNode::HeapNode(void *address, size_t size)
{
    _address = address;
    _size = size;
}

void *HeapNode::getAddress()
{
    return _address;
}

size_t HeapNode::getSize()
{
    return _size;
}
