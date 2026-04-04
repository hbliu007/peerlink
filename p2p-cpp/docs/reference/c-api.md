---
title: C API 参考
description: PeerLink C语言API完整参考，包括函数签名、参数说明和代码示例
tags:
  - api
  - c
  - reference
  - bindings
---

# C API 参考

本页面提供PeerLink C语言API的完整参考。

## 概述

C API提供语言无关的接口，支持Python、Java、Swift、JavaScript等多语言绑定。

### 头文件

```c
#include "p2p_c_api.h"
```

### 链接

```bash
# Linux
gcc -o myapp myapp.c -lp2p_client -lboost_system -lpthread

# macOS
clang -o myapp myapp.c -lp2p_client -lboost_system -lpthread
```

## 数据类型

### Opaque Handle

```c
typedef void* p2p_client_t;
typedef void* p2p_config_t;
```

不透明的句柄类型，用于封装C++对象。

### 错误码

```c
typedef enum {
    P2P_OK = 0,
    P2P_ERROR_INVALID_PARAM = 1,
    P2P_ERROR_NOT_INITIALIZED = 2,
    P2P_ERROR_ALREADY_CONNECTED = 3,
    P2P_ERROR_CONNECTION_FAILED = 4,
    P2P_ERROR_TIMEOUT = 5,
    P2P_ERROR_NAT_DETECTION_FAILED = 6,
    P2P_ERROR_SEND_FAILED = 7,
    P2P_ERROR_UNKNOWN = 99
} p2p_error_t;
```

### 连接状态

```c
typedef enum {
    P2P_STATE_DISCONNECTED = 0,
    P2P_STATE_CONNECTING = 1,
    P2P_STATE_HANDSHAKE = 2,
    P2P_STATE_CONNECTED_P2P = 3,
    P2P_STATE_CONNECTED_RELAY = 4,
    P2P_STATE_FAILED = 5
} p2p_connection_state_t;
```

### 回调类型

```c
// 连接成功回调
typedef void (*p2p_connected_callback_t)(void* user_data);

// 断开连接回调
typedef void (*p2p_disconnected_callback_t)(void* user_data);

// 数据接收回调
typedef void (*p2p_data_callback_t)(
    int channel_id,
    const uint8_t* data,
    size_t data_len,
    void* user_data
);

// 错误回调
typedef void (*p2p_error_callback_t)(
    p2p_error_t error,
    const char* message,
    void* user_data
);

// 完成回调
typedef void (*p2p_completion_callback_t)(
    p2p_error_t error,
    void* user_data
);
```

## 配置函数

### p2p_config_create

```c
p2p_config_t p2p_config_create(void);
```

创建P2P客户端配置。

**返回值：** 配置句柄，失败返回NULL

**示例：**

```c
p2p_config_t config = p2p_config_create();
if (!config) {
    fprintf(stderr, "Failed to create config\n");
    return 1;
}
```

---

### p2p_config_destroy

```c
void p2p_config_destroy(p2p_config_t config);
```

销毁P2P客户端配置。

**参数：**
- `config` - 配置句柄

---

### p2p_config_set_signaling_server

```c
void p2p_config_set_signaling_server(
    p2p_config_t config,
    const char* server,
    uint16_t port
);
```

设置信令服务器地址。

**参数：**
- `config` - 配置句柄
- `server` - 服务器主机名或IP
- `port` - 服务器端口

---

### p2p_config_set_stun_server

```c
void p2p_config_set_stun_server(
    p2p_config_t config,
    const char* server,
    uint16_t port
);
```

设置STUN服务器地址。

**参数：**
- `config` - 配置句柄
- `server` - STUN服务器地址
- `port` - STUN端口（通常3478）

---

### p2p_config_set_relay_server

```c
void p2p_config_set_relay_server(
    p2p_config_t config,
    const char* server,
    uint16_t port
);
```

设置中继服务器地址。

**参数：**
- `config` - 配置句柄
- `server` - 中继服务器地址
- `port` - 中继端口

---

### p2p_config_set_local_port

```c
void p2p_config_set_local_port(
    p2p_config_t config,
    uint16_t port
);
```

设置本地UDP端口。

**参数：**
- `config` - 配置句柄
- `port` - 本地端口（0为自动分配）

## 生命周期函数

### p2p_client_create

```c
p2p_client_t p2p_client_create(
    const char* did,
    p2p_config_t config
);
```

创建P2P客户端。

**参数：**
- `did` - 设备ID（NULL结尾的字符串）
- `config` - 配置句柄（可为NULL使用默认配置）

**返回值：** 客户端句柄，失败返回NULL

**示例：**

```c
p2p_client_t client = p2p_client_create("device-001", config);
if (!client) {
    fprintf(stderr, "Failed to create client\n");
    return 1;
}
```

---

### p2p_client_destroy

```c
void p2p_client_destroy(p2p_client_t client);
```

销毁P2P客户端。

**参数：**
- `client` - 客户端句柄

---

### p2p_client_initialize

```c
p2p_error_t p2p_client_initialize(
    p2p_client_t client,
    p2p_completion_callback_t callback,
    void* user_data
);
```

初始化P2P客户端。

**参数：**
- `client` - 客户端句柄
- `callback` - 完成回调
- `user_data` - 用户数据

**返回值：** P2P_OK表示成功

---

### p2p_client_run

```c
p2p_error_t p2p_client_run(p2p_client_t client);
```

运行事件循环（阻塞）。

**参数：**
- `client` - 客户端句柄

**返回值：** P2P_OK表示成功

---

### p2p_client_stop

```c
void p2p_client_stop(p2p_client_t client);
```

停止事件循环。

**参数：**
- `client` - 客户端句柄

## 连接管理

### p2p_client_connect

```c
p2p_error_t p2p_client_connect(
    p2p_client_t client,
    const char* peer_did,
    p2p_completion_callback_t callback,
    void* user_data
);
```

连接到对等端。

**参数：**
- `client` - 客户端句柄
- `peer_did` - 对等端设备ID
- `callback` - 完成回调
- `user_data` - 用户数据

**返回值：** P2P_OK表示成功

**示例：**

```c
void on_connect_complete(p2p_error_t error, void* user_data) {
    if (error == P2P_OK) {
        printf("Connected successfully\n");
    } else {
        printf("Connection failed: %d\n", error);
    }
}

p2p_client_connect(client, "device-002", on_connect_complete, NULL);
```

---

### p2p_client_close

```c
void p2p_client_close(p2p_client_t client);
```

关闭连接。

**参数：**
- `client` - 客户端句柄

## 数据传输

### p2p_client_create_channel

```c
int p2p_client_create_channel(p2p_client_t client);
```

创建数据通道。

**参数：**
- `client` - 客户端句柄

**返回值：** 通道ID，失败返回-1

---

### p2p_client_close_channel

```c
void p2p_client_close_channel(
    p2p_client_t client,
    int channel_id
);
```

关闭数据通道。

**参数：**
- `client` - 客户端句柄
- `channel_id` - 通道ID

---

### p2p_client_send_data

```c
p2p_error_t p2p_client_send_data(
    p2p_client_t client,
    int channel_id,
    const uint8_t* data,
    size_t data_len,
    p2p_completion_callback_t callback,
    void* user_data
);
```

发送数据。

**参数：**
- `client` - 客户端句柄
- `channel_id` - 通道ID
- `data` - 数据缓冲区
- `data_len` - 数据长度
- `callback` - 完成回调
- `user_data` - 用户数据

**返回值：** P2P_OK表示成功

**示例：**

```c
const char* message = "Hello, peer!";
p2p_client_send_data(
    client,
    0,
    (const uint8_t*)message,
    strlen(message),
    NULL,
    NULL
);
```

## 回调设置

### p2p_client_set_connected_callback

```c
void p2p_client_set_connected_callback(
    p2p_client_t client,
    p2p_connected_callback_t callback,
    void* user_data
);
```

设置连接成功回调。

**参数：**
- `client` - 客户端句柄
- `callback` - 回调函数
- `user_data` - 用户数据

---

### p2p_client_set_disconnected_callback

```c
void p2p_client_set_disconnected_callback(
    p2p_client_t client,
    p2p_disconnected_callback_t callback,
    void* user_data
);
```

设置断开连接回调。

**参数：**
- `client` - 客户端句柄
- `callback` - 回调函数
- `user_data` - 用户数据

---

### p2p_client_set_data_callback

```c
void p2p_client_set_data_callback(
    p2p_client_t client,
    p2p_data_callback_t callback,
    void* user_data
);
```

设置数据接收回调。

**参数：**
- `client` - 客户端句柄
- `callback` - 回调函数
- `user_data` - 用户数据

**示例：**

```c
void on_data_received(
    int channel_id,
    const uint8_t* data,
    size_t data_len,
    void* user_data
) {
    printf("Received %zu bytes on channel %d\n", data_len, channel_id);
    fwrite(data, 1, data_len, stdout);
    printf("\n");
}

p2p_client_set_data_callback(client, on_data_received, NULL);
```

---

### p2p_client_set_error_callback

```c
void p2p_client_set_error_callback(
    p2p_client_t client,
    p2p_error_callback_t callback,
    void* user_data
);
```

设置错误回调。

**参数：**
- `client` - 客户端句柄
- `callback` - 回调函数
- `user_data` - 用户数据

## 状态查询

### p2p_client_get_state

```c
p2p_connection_state_t p2p_client_get_state(p2p_client_t client);
```

获取连接状态。

**参数：**
- `client` - 客户端句柄

**返回值：** 连接状态

---

### p2p_client_is_connected

```c
int p2p_client_is_connected(p2p_client_t client);
```

检查是否已连接。

**参数：**
- `client` - 客户端句柄

**返回值：** 1表示已连接，0表示未连接

---

### p2p_client_is_p2p

```c
int p2p_client_is_p2p(p2p_client_t client);
```

检查是否为P2P连接（非中继）。

**参数：**
- `client` - 客户端句柄

**返回值：** 1表示P2P，0表示中继

---

### p2p_client_get_did

```c
const char* p2p_client_get_did(p2p_client_t client);
```

获取设备ID。

**参数：**
- `client` - 客户端句柄

**返回值：** 设备ID字符串（不要释放）

## 完整示例

```c
#include "p2p_c_api.h"
#include <stdio.h>
#include <string.h>

static void on_connected(void* user_data) {
    printf("Connected!\n");
}

static void on_disconnected(void* user_data) {
    printf("Disconnected\n");
}

static void on_data(int channel_id, const uint8_t* data, size_t len, void* user_data) {
    printf("Received: %.*s\n", (int)len, data);
}

static void on_error(p2p_error_t error, const char* msg, void* user_data) {
    fprintf(stderr, "Error: %s\n", msg);
}

int main() {
    // 创建配置
    p2p_config_t config = p2p_config_create();
    p2p_config_set_signaling_server(config, "127.0.0.1", 8080);
    p2p_config_set_stun_server(config, "127.0.0.1", 3478);

    // 创建客户端
    p2p_client_t client = p2p_client_create("device-001", config);
    if (!client) {
        fprintf(stderr, "Failed to create client\n");
        return 1;
    }

    // 设置回调
    p2p_client_set_connected_callback(client, on_connected, NULL);
    p2p_client_set_disconnected_callback(client, on_disconnected, NULL);
    p2p_client_set_data_callback(client, on_data, NULL);
    p2p_client_set_error_callback(client, on_error, NULL);

    // 初始化
    p2p_client_initialize(client, NULL, NULL);

    // 连接到对等端
    p2p_client_connect(client, "device-002", NULL, NULL);

    // 运行事件循环
    p2p_client_run(client);

    // 清理
    p2p_client_destroy(client);
    p2p_config_destroy(config);

    return 0;
}
```

## 编译

```bash
# Linux
gcc -o myapp myapp.c -I/path/to/include -L/path/to/lib -lp2p_client -lboost_system -lpthread

# macOS
clang -o myapp myapp.c -I/path/to/include -L/path/to/lib -lp2p_client -lboost_system -lpthread

# 运行
LD_LIBRARY_PATH=/path/to/lib ./myapp
```
