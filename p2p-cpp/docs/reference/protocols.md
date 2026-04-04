---
title: 协议规范
description: PeerLink协议规范，包括Simple Relay、STUN、DCUtR和Circuit Relay v2
tags:
  - protocol
  - specification
  - stun
  - relay
  - dcutr
---

# 协议规范

本页面描述PeerLink使用的网络协议。

## Simple Relay Protocol

简单的文本协议，用于relay-tunnel工具。

### 消息格式

所有消息为文本行，以`\n`结尾：

```
COMMAND [arguments]\n
```

### 消息类型

#### REGISTER

注册设备ID并等待连接。

```
REGISTER <device-id>\n
```

**响应：**

```
OK <registered-id>\n
ERROR <error-message>\n
```

**示例：**

```bash
# 客户端发送
REGISTER office-001

# 服务器响应
OK office-001
```

---

#### PING

心跳保活。

```
PING\n
```

**响应：**

```
PONG\n
```

---

#### CONNECT

连接到已注册的设备。

```
CONNECT <target-id>\n
```

**响应：**

```
OK <target-id>\n
ERROR <error-message>\n
```

**示例：**

```bash
# 客户端发送
CONNECT office-001

# 服务器响应
OK office-001
```

---

#### 数据转发

注册/连接成功后，所有后续数据直接转发：

```
<binary-data>
```

数据流为双向透明转发。

### 连接流程

```
Client A                    Relay Server                 Client B
   |                              |                            |
   | REGISTER device-a            |                            |
   |---------------------------->|                            |
   | OK device-a                 |                            |
   |<----------------------------|                            |
   |                              |                            |
   |                              | REGISTER device-b          |
   |                              |<---------------------------|
   |                              | OK device-b                |
   |                              |--------------------------->|
   |                              |                            |
   | CONNECT device-b            |                            |
   |---------------------------->|                            |
   | OK device-b                 |                            |
   |<----------------------------|                            |
   |                              | OK device-a                |
   |                              |--------------------------->|
   |                              |                            |
   | <--- data relayed ----->| <--- data relayed ----->|
```

## STUN Protocol

基于RFC 5389的STUN协议，用于NAT穿透。

### 消息类型

| 类型 | 值 | 说明 |
|------|-----|------|
| Binding Request | `0x0001` | 绑定请求 |
| Binding Response | `0x0101` | 绑定响应 |
| Binding Error Response | `0x0111` | 错误响应 |

### 消息格式

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0 0|     STUN Message Type     |         Message Length        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Magic Cookie                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                     Transaction ID (96 bits)                  |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                             Attributes                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 属性类型

| 类型 | 值 | 说明 |
|------|-----|------|
| MAPPED-ADDRESS | `0x0001` | 映射地址 |
| XOR-MAPPED-ADDRESS | `0x0020` | XOR映射地址 |
| ERROR-CODE | `0x0009` | 错误代码 |
| SOFTWARE | `0x8022` | 软件信息 |

### C++实现

```cpp
namespace p2p::protocol {

// STUN Magic Cookie
constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

// 消息类型
enum class StunMessageType : uint16_t {
    BindingRequest = 0x0001,
    BindingResponse = 0x0101,
    BindingErrorResponse = 0x0111,
};

// 创建绑定请求
StunMessage CreateBindingRequest() {
    TransactionId tid = GenerateTransactionId();
    return StunMessage(StunMessageType::BindingRequest, tid);
}

// 解析XOR-MAPPED-ADDRESS
std::optional<std::pair<std::string, uint16_t>> ParseXorMappedAddress(
    const std::vector<uint8_t>& data,
    const TransactionId& tid
) {
    // 解析逻辑...
}

}
```

## DCUtR Protocol

libp2p的Direct Connection Upgrade through Relay协议。

### 消息类型

#### CONNECT

发起方发送，包含候选地址列表。

```
struct ConnectMessage {
    repeated bytes addrs;      // 候选地址（multiaddr格式）
    int64 timestamp_ns;       // 时间戳（纳秒）
}
```

#### SYNC

响应方发送，回显候选地址和时间戳。

```
struct SyncMessage {
    repeated bytes addrs;           // 候选地址
    int64 echo_timestamp_ns;        // 回显时间戳
    int64 timestamp_ns;             // 当前时间戳
}
```

### 打洞协调

使用RTT计算打洞时间：

```
Initiator                  Responder
    |                           |
    | CONNECT (t1)              |
    |-------------------------->|
    |                           |
    |           SYNC (t2, t3)   |
    |<--------------------------|
    |                           |
    | Calculate punch time:      |
    | t4 + RTT/2 + buffer       |
    |                           |
    | [Both punch at T]          |
    |                           |
    |<-------- UDP ------------>|
```

### C++实现

```cpp
namespace p2p::protocol {

// DCUtR协议ID
constexpr const char* DCUTR_PROTOCOL_ID = "/libp2p/dcutr";

// 打洞缓冲时间
constexpr int64_t PUNCH_BUFFER_MS = 100;

// 计算RTT
int64_t MeasureRTT(
    int64_t t1_send,
    int64_t t4_receive,
    int64_t t2_receive,
    int64_t t3_send
) {
    return (t4_receive - t1_send) - (t3_send - t2_receive);
}

// 计算发起方打洞时间
PunchSchedule CalculateInitiatorSchedule(
    int64_t t4_receive,
    int64_t rtt_ns,
    const std::vector<Address>& target_addrs
) {
    int64_t punch_time = t4_receive + rtt_ns / 2 + PUNCH_BUFFER_MS * 1000000;
    return {punch_time, target_addrs, rtt_ns};
}

}
```

## Circuit Relay v2 Protocol

libp2p的Circuit Relay v2协议，用于通过中继节点建立连接。

### 消息类型

| 类型 | 说明 |
|------|------|
| RESERVE | 预留中继槽位 |
| CONNECT | 连接到对等端 |
| STATUS | 状态响应 |

### RESERVE消息

发起方向中继节点发送：

```
message CircuitRelay {
    oneof message {
        Reservation reserve = 1;
        Peer peer = 2;
        Status status = 3;
    }
}

message Reservation {
    // 无额外字段
}
```

**响应：**

```
message Status {
    RelayStatusCode code = 1;
    string message = 2;
    ReservationInfo reservation = 3;
}

message ReservationInfo {
    uint64 expire = 1;        // 过期时间戳
    bytes addr = 2;           // 中继节点地址
    bytes voucher = 3;        // 签名凭证
}
```

### CONNECT消息

```
message Peer {
    bytes id = 1;                           // 对等端ID
    repeated bytes addrs = 2;               // 地址列表
}
```

### 状态码

| 代码 | 名称 | 说明 |
|------|------|------|
| 0 | OK | 成功 |
| 1 | RESERVATION_REFUSED | 预留被拒绝 |
| 2 | RESOURCE_LIMIT_EXCEEDED | 资源限制 |
| 3 | PERMISSION_DENIED | 权限拒绝 |
| 4 | CONNECTION_FAILED | 连接失败 |
| 5 | NO_RESERVATION | 无预留 |
| 6 | MALFORMED_MESSAGE | 消息格式错误 |

### C++实现

```cpp
namespace p2p::protocol {

// 创建RESERVE消息
RelayMessageWrapper CreateReserve() {
    return RelayMessageWrapper::CreateReserve();
}

// 创建CONNECT消息
RelayMessageWrapper CreateConnect(const PeerInfo& peer) {
    return RelayMessageWrapper::CreateConnect(peer);
}

// 创建STATUS消息
RelayMessageWrapper CreateStatus(
    RelayStatusCode code,
    const std::string& text,
    const std::optional<ReservationInfo>& reservation
) {
    return RelayMessageWrapper::CreateStatus(code, text, reservation);
}

}
```

## P2P Message Protocol

P2P客户端之间的消息协议。

### 消息类型

| 类型 | 值 | 说明 |
|------|-----|------|
| HANDSHAKE | `0x01` | 握手消息 |
| HANDSHAKE_ACK | `0x02` | 握手确认 |
| KEEPALIVE | `0x03` | 保活消息 |
| CHANNEL_DATA | `0x04` | 通道数据 |
| CHANNEL_OPEN | `0x05` | 打开通道 |
| CHANNEL_CLOSE | `0x06` | 关闭通道 |
| DISCONNECT | `0x07` | 断开连接 |
| ERROR | `0x08` | 错误消息 |

### 消息格式

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Message Type  |     Channel ID (optional)      |         ...
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Message ID (16 bytes)                       |
+---------------------------------------------------------------+
|                    Sender DID (variable)                       |
+---------------------------------------------------------------+
|                   Receiver DID (variable)                      |
+---------------------------------------------------------------+
|                    Timestamp (8 bytes)                         |
+---------------------------------------------------------------+
|                    Payload Length (4 bytes)                    |
+---------------------------------------------------------------+
|                    Payload (variable)                          |
+---------------------------------------------------------------+
```

### C++实现

```cpp
namespace p2p::protocol {

class Message {
public:
    Message(MessageType type,
            const std::string& sender_did,
            const std::string& receiver_did);

    // 编码为字节流
    std::vector<uint8_t> Encode() const;

    // 从字节流解码
    static std::unique_ptr<Message> Decode(const std::vector<uint8_t>& data);

    // Getters
    MessageType type() const { return type_; }
    const std::string& sender_did() const { return sender_did_; }
    const std::string& receiver_did() const { return receiver_did_; }
    const std::vector<uint8_t>& payload() const { return payload_; }
};

class HandshakeMessage : public Message {
public:
    void SetPublicAddress(const std::string& ip, uint16_t port);
    void SetLocalAddress(const std::string& ip, uint16_t port);
    void SetNATType(const std::string& nat_type);
};

class ChannelDataMessage : public Message {
public:
    ChannelDataMessage(const std::string& sender_did,
                       const std::string& receiver_did,
                       int channel_id,
                       const std::vector<uint8_t>& data);
};

}
```

## 协议协商

### 版本协商

```
struct ProtocolVersion {
    string protocol_id;     // e.g., "/libp2p/dcutr"
    uint32 major;
    uint32 minor;
    uint32 patch;
};
```

### 协商流程

```
Alice                          Bob
  |                              |
  | Supported Protocols          |
  |----------------------------->|
  |                              |
  |           Select Best         |
  |<-----------------------------|
  |                              |
  |     Agreed Protocol Version   |
  |----------------------------->|
  |                              |
  | Use negotiated protocol      |
  |<============================>|
```

### 支持的协议

| 协议ID | 版本 | 说明 |
|--------|------|------|
| `/libp2p/dcutr` | 1.0.0 | Direct Connection Upgrade |
| `/libp2p/circuit/relay/0.2.0/hop` | 0.2.0 | Circuit Relay Hop |
| `/libp2p/circuit/relay/0.2.0/stop` | 0.2.0 | Circuit Relay Stop |
| `/ipfs/id/1.0.0` | 1.0.0 | Identify |
