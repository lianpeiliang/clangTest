#include <cstdint>

#pragma pack(push, 1)

struct MsgHeader
{
    uint16_t msgId;
    uint8_t msgType;
    uint32_t dataLength;
};

#pragma pack(pop)
