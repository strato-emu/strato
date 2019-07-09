#pragma once
#include <cstdint>

namespace core::kernel
{
class IpcRequest
{
public:
    IpcRequest(uint8_t* tlsPtr);

    template<typename T>
    T GetValue()
    {
        dataPos += sizeof(T);
        return *reinterpret_cast<T*>(&dataPtr[dataPos-sizeof(T)]);
    }

    uint16_t type, xCount, aCount, bCount, wCount;
    uint32_t dataSize;

private:
    uint8_t* dataPtr;
    uint32_t dataPos;
};

class IpcResponse
{
public:
    IpcResponse() {}
};
}