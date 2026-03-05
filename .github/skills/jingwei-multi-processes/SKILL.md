---
name: jingwei-multi-processes
description: JingWei(精卫) 的多进程设计方案和详细定义。
---

JingWei 采用多进程来实现多客户端支持，通信方式主要基于 Unix 域套接字 (Unix Domain Sockets)。

多进程通信采用自定义协议，消息格式如下：

| JW_MSG_TYPE U8 | JW_MSG_CMD U8 | JW_MSG_LEN U16 | JW_MSG_ID U16 | JW_MSG_CHECKSUM U8 | Payload (variable) |
|----------------|---------------|----------------|---------------|--------------------|--------------------|
| 1 byte         | 1 byte        | 2 bytes        | 2 bytes       | 1 byte             | N bytes (0<=N)     |


- `JW_MSG_TYPE`：消息类型，定义了消息的类别，如命令、事件、响应等。
- `JW_MSG_CMD`：消息命令，具体的操作指令，如创建窗口、销毁窗口、更新图层等。
- `JW_MSG_LEN`：消息长度，表示消息总长度，包含 消息头、Payload 和 CheckSum。 * 注意：消息长度必须大于等于7字节且小于等于65535字节（最大消息长度）。*
- `JW_MSG_ID`：消息 ID，用于唯一标识一条消息。
- `JW_MSG_CHECKSUM`：消息校验码，用于确保消息在传输过程中未被篡改或损坏。
- `Payload`：消息的实际内容，根据消息类型和命令的不同而变化。 其长度>=0，可以为空。

> CheckSum 的计算方法：将 MSG_ID 对 (Payload长度%7) 再取余，得到 Payload中的一个字节，再与前6字节（MSG_TYPE、MSG_CMD、MSG_LEN、MSG_ID）进行累加运算，得到最终的 CheckSum。

```c
bool validate_checksum(const uint8_t *msg, size_t len) {
    if (NULL == msg ) return false; // 消息不能为空
    if (len < 7) return false; // 消息长度必须至少为7字节
    uint8_t checksum = 0;
    int payload_len = len - 7; // Payload长度 = 总长度 - 消息头(6字节) - CheckSum(1字节)
    uint8_t payload_checksum = payload_len > 0 ? msg[7 + (msg[4] % (payload_len%7))] : 0; // Payload中的一个字节
    for (size_t i = 0; i < 6; i++) {
        checksum += msg[i];
    }
    checksum += payload_checksum;
    return checksum == msg[len - 1];
}
```

