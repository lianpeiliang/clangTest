#include <cstdint>

// 保存原有打包设置，将成员对齐上限设为 1 字节，避免成员之间的填充。
#pragma pack(push, 1)

struct MsgHeader
{
    uint16_t msgId;
    uint8_t msgType;
    uint32_t dataLength;
};

// 恢复之前的打包设置，避免影响后续声明的结构体。
#pragma pack(pop)
