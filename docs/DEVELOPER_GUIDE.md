# PeerLink 开发者贡献指南

> 欢迎参与 PeerLink 项目开发！

---

## 目录

1. [开发环境搭建](#开发环境搭建)
2. [代码风格规范](#代码风格规范)
3. [提交代码流程](#提交代码流程)
4. [测试指南](#测试指南)
5. [架构概述](#架构概述)
6. [开发工作流](#开发工作流)
7. [常见开发任务](#常见开发任务)
8. [发布流程](#发布流程)

---

## 开发环境搭建

### 前置要求

- **Python**: 3.11+
- **C++**: C++20 编译器 (GCC 11+, Clang 14+, MSVC 2022+)
- **CMake**: 3.20+
- **Docker**: 20.10+ (可选，用于容器化开发)

### 开发工具安装

#### 1. 安装依赖管理工具

```bash
# Python 开发工具
pip install poetry pre-commit

# C++ 开发工具
brew install cmake clang-format cppcheck  # macOS
sudo apt install cmake clang-format cppcheck  # Ubuntu
```

#### 2. 克隆项目

```bash
# 使用 SSH (推荐，已配置 SSH 密钥)
git clone git@github.com:hbliu007/peerlink.git
cd peerlink

# 或使用 HTTPS
git clone https://github.com/hbliu007/peerlink.git
cd peerlink
```

#### 3. 安装 Python 开发依赖

```bash
# 使用 Poetry 安装
cd client_sdk
poetry install

# 或使用 pip
pip install -e ".[dev]"

# 安装 pre-commit hooks
pre-commit install
```

#### 4. 编译 C++ 组件

```bash
cd p2p-cpp
mkdir build && cd build

# Debug 模式（开发）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON

# Release 模式（发布）
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

# 编译
make -j$(nproc)

# 运行测试
ctest --output-on-failure
```

#### 5. 启动开发服务器

```bash
# 使用 Docker Compose
cd ..
docker-compose -f docker-compose.dev.yml up -d

# 或直接运行 Python 服务
python -m p2p_engine.servers.stun
python -m p2p_engine.servers.relay
python -m p2p_engine.servers.signaling
python -m p2p_engine.servers.did
```

### IDE 配置

#### VS Code

安装推荐扩展：

```bash
# Python
code --install-extension ms-python.python
code --install-extension ms-python.pylint
code --install-extension ms-python.vscode-pylance

# C++
code --install-extension ms-vscode.cpptools

# Docker
code --install-extension ms-azuretools.vscode-docker

# Git
code --install-extension eamodio.gitlens
```

创建 `.vscode/settings.json`:

```json
{
    "python.linting.enabled": true,
    "python.linting.pylintEnabled": true,
    "python.linting.flake8Enabled": true,
    "python.linting.mypyEnabled": true,
    "python.formatting.provider": "black",
    "editor.formatOnSave": true,
    "editor.codeActionsOnSave": {
        "source.organizeImports": true
    },
    "files.trimTrailingWhitespace": true,
    "files.insertFinalNewline": true,
    "C_Cpp.default.cppStandard": "c++20",
    "C_Cpp.default.cStandard": "c17"
}
```

#### PyCharm

1. 打开项目目录
2. Settings → Project → Python Interpreter → 选择 Poetry 虚拟环境
3. Settings → Tools → Black → 启用
4. Settings → Tools → File Watchers → 添加 isort

---

## 代码风格规范

### Python 代码规范

遵循 **PEP 8** 标准，使用以下工具确保代码质量：

#### 格式化工具

```bash
# Black - 代码格式化
black src/ tests/

# isort - 导入排序
isort src/ tests/

# 同时运行
black src/ tests/ && isort src/ tests/
```

#### 代码检查

```bash
# Flake8 - 代码风格检查
flake8 src/ tests/ --max-line-length=100

# Pylint - 代码质量检查
pylint src/

# Mypy - 类型检查
mypy src/
```

#### 代码风格示例

```python
"""
模块文档字符串

简要描述模块功能。
"""

from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import AsyncIterator

# 常量使用 UPPER_CASE
MAX_CONNECTIONS = 100
DEFAULT_TIMEOUT = 30.0


class ConnectionManager:
    """连接管理器类。

    负责 P2P 连接的建立、维护和关闭。
    """

    def __init__(self, max_connections: int = MAX_CONNECTIONS) -> None:
        """初始化连接管理器。

        Args:
            max_connections: 最大连接数
        """
        self.max_connections = max_connections
        self._connections: dict[str, asyncio.Task] = {}

    async def connect(self, peer_id: str, timeout: float = DEFAULT_TIMEOUT) -> bool:
        """连接到对等设备。

        Args:
            peer_id: 对等设备 ID
            timeout: 超时时间（秒）

        Returns:
            是否连接成功

        Raises:
            ConnectionError: 连接失败
        """
        if peer_id in self._connections:
            return True

        try:
            task = asyncio.create_task(self._do_connect(peer_id))
            await asyncio.wait_for(task, timeout=timeout)
            self._connections[peer_id] = task
            return True
        except asyncio.TimeoutError as exc:
            raise ConnectionError(f"Connect to {peer_id} timeout") from exc

    async def _do_connect(self, peer_id: str) -> None:
        """执行连接。"""
        # 实现连接逻辑
        pass


async def main() -> None:
    """主函数入口。"""
    manager = ConnectionManager()
    if await manager.connect("peer-001"):
        print("Connected!")


if __name__ == "__main__":
    asyncio.run(main())
```

### C++ 代码规范

遵循 **C++ Core Guidelines** 和 **Google C++ Style Guide**：

#### 格式化工具

```bash
# clang-format - 代码格式化
clang-format -i src/*.cpp include/*.hpp

# 检查格式
clang-format --dry-run --Werror src/*.cpp
```

#### 代码风格示例

```cpp
// 文件头注释
/**
 * @file connection.hpp
 * @brief P2P 连接管理
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace p2p {

// 常量使用 kCamelCase
constexpr size_t kMaxConnections = 100;
constexpr double kDefaultTimeout = 30.0;

// 类名使用 PascalCase
class ConnectionManager {
public:
    // 构造函数
    explicit ConnectionManager(size_t max_connections = kMaxConnections);

    // 析构函数
    ~ConnectionManager();

    // 禁用拷贝
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 启用移动
    ConnectionManager(ConnectionManager&&) noexcept;
    ConnectionManager& operator=(ConnectionManager&&) noexcept;

    /**
     * @brief 连接到对等设备
     *
     * @param peer_id 对等设备 ID
     * @param timeout_ms 超时时间（毫秒）
     * @return true 连接成功
     * @return false 连接失败
     */
    bool Connect(const std::string& peer_id, int timeout_ms = kDefaultTimeout * 1000);

    /**
     * @brief 断开连接
     *
     * @param peer_id 对等设备 ID
     */
    void Disconnect(const std::string& peer_id);

private:
    // 成员变量使用 camel_case_ 带下划线后缀
    size_t max_connections_;
    std::unordered_map<std::string, std::unique_ptr<Connection>> connections_;

    // 私有方法使用 PascalCase
    bool DoConnect(const std::string& peer_id);
};

}  // namespace p2p
```

---

## 提交代码流程

### Git 工作流

#### 1. 创建功能分支

```bash
# 更新主分支
git checkout main
git pull origin main

# 创建功能分支
git checkout -b feature/your-feature-name

# 或修复分支
git checkout -b fix/issue-description
```

分支命名规范：
- `feature/功能描述` - 新功能
- `fix/问题描述` - Bug 修复
- `refactor/重构内容` - 代码重构
- `docs/文档内容` - 文档更新
- `test/测试内容` - 测试相关
- `perf/性能优化` - 性能优化

#### 2. 开发和测试

```bash
# 查看修改
git status
git diff

# 暂存文件
git add path/to/file

# 提交（遵循提交规范）
git commit -m "feat(connection): add reconnect logic with exponential backoff"
```

#### 3. 同步远程分支

```bash
# 拉取最新代码
git fetch origin

# 如果 main 有更新，变基到最新
git rebase origin/main

# 解决冲突（如有）
# 编辑冲突文件
git add conflicted_files
git rebase --continue
```

#### 4. 推送到远程

```bash
# 推送分支
git push origin feature/your-feature-name

# 或设置上游分支
git push -u origin feature/your-feature-name
```

#### 5. 创建 Pull Request

在 GitHub 上创建 Pull Request：
1. 填写 PR 模板
2. 关联相关 Issue
3. 请求代码审查
4. 等待 CI 检查通过

### 提交信息规范

遵循 **Conventional Commits** 规范：

```
<type>: <description>

[optional body]

[optional footer]
```

#### 类型 (type)

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat: add WebRTC data channel support` |
| `fix` | Bug 修复 | `fix: resolve memory leak in connection pool` |
| `refactor` | 重构 | `refactor: simplify event handling logic` |
| `docs` | 文档 | `docs: update API documentation` |
| `test` | 测试 | `test: add integration tests for relay server` |
| `chore` | 构建/工具 | `chore: upgrade to CMake 3.25` |
| `perf` | 性能优化 | `perf: reduce connection latency by 20%` |
| `style` | 代码风格 | `style: format code with black` |

#### 示例

```
feat(connection): implement automatic reconnection with exponential backoff

Add automatic reconnection when connection is lost. The reconnection
delay follows exponential backoff strategy starting from 1s and
maxing out at 60s.

Features:
- Exponential backoff (1s, 2s, 4s, ... 60s)
- Max retry attempts: 10
- Event notifications for connection state changes

Closes #123
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

### Code Review 检查清单

提交 PR 前请确认：

- [ ] 代码符合项目风格规范
- [ ] 所有函数有文档字符串
- [ ] 测试覆盖率 >= 80%
- [ ] 无硬编码配置值
- [ ] 错误处理完整
- [ ] 无安全漏洞
- [ ] 更新了相关文档
- [ ] 添加了测试用例
- [ ] CI 检查全部通过

---

## 测试指南

### 测试类型

#### 1. 单元测试

测试单个函数或类的方法：

```python
# tests/unit/test_connection.py
import pytest
from p2p_engine.connection import ConnectionManager

@pytest.mark.asyncio
async def test_connection_initialization():
    """测试连接管理器初始化"""
    manager = ConnectionManager(max_connections=10)
    assert manager.max_connections == 10
    assert len(manager.connections) == 0

@pytest.mark.asyncio
async def test_connect_success():
    """测试成功连接"""
    manager = ConnectionManager()
    result = await manager.connect("peer-001")
    assert result is True
    assert "peer-001" in manager.connections

@pytest.mark.asyncio
async def test_connect_timeout():
    """测试连接超时"""
    manager = ConnectionManager()
    with pytest.raises(ConnectionError):
        await manager.connect("peer-001", timeout=0.1)
```

#### 2. 集成测试

测试多个组件协同工作：

```python
# tests/integration/test_p2p_connection.py
import pytest
from p2p_engine import P2PClient

@pytest.mark.integration
@pytest.mark.asyncio
async def test_end_to_end_connection():
    """测试端到端 P2P 连接"""
    client1 = P2PClient(did="device-1")
    client2 = P2PClient(did="device-2")

    try:
        await asyncio.gather(
            client1.initialize(),
            client2.initialize()
        )

        # 建立连接
        assert await client1.connect("device-2")

        # 传输数据
        await client1.send_data(b"Hello!")
        data = await client2.recv_data(timeout=5)
        assert data == b"Hello!"

    finally:
        await asyncio.gather(
            client1.close(),
            client2.close()
        )
```

#### 3. 压力测试

测试系统在高负载下的表现：

```python
# tests/stress/test_concurrent_connections.py
import pytest
from p2p_engine import P2PClient

@pytest.mark.stress
@pytest.mark.asyncio
async def test_100_concurrent_connections():
    """测试 100 个并发连接"""
    clients = [P2PClient(did=f"device-{i}") for i in range(100)]

    try:
        # 并发初始化
        start_time = time.time()
        await asyncio.gather(*[c.initialize() for c in clients])
        init_time = time.time() - start_time

        assert init_time < 30  # 30秒内完成
        assert all(c.is_initialized for c in clients)

    finally:
        await asyncio.gather(*[c.close() for c in clients])
```

### 运行测试

```bash
# 运行所有测试
pytest

# 运行特定文件
pytest tests/unit/test_connection.py

# 运行特定标记
pytest -m unit
pytest -m "not stress"

# 生成覆盖率报告
pytest --cov=p2p_engine --cov-report=html

# 并行运行测试
pytest -n auto
```

### 测试最佳实践

1. **隔离性**: 每个测试独立运行，不依赖其他测试
2. **确定性**: 避免随机性，使用固定种子
3. **快速**: 单元测试应在秒级完成
4. **清晰**: 使用描述性的测试名称
5. **覆盖率**: 目标 80% 以上

---

## 架构概述

### 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         应用层                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  Client SDK  │  │   Examples   │  │   Tests      │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│                         P2P 引擎层                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │    Core      │  │   Protocol   │  │   Transport  │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│  ┌──────────────┐  ┌──────────────┐                           │
│  │     NAT      │  │   Security   │                           │
│  └──────────────┘  └──────────────┘                           │
├─────────────────────────────────────────────────────────────────┤
│                         服务器层                                │
│  ┌──────┐  ┌──────┐  ┌──────────┐  ┌──────┐                   │
│  │ STUN │  │Relay │  │Signaling │  │ DID  │                   │
│  └──────┘  └──────┘  └──────────┘  └──────┘                   │
└─────────────────────────────────────────────────────────────────┘
```

### 目录结构

```
peerlink/
├── client_sdk/           # 客户端 SDK
│   ├── src/             # 源代码
│   ├── tests/           # 测试
│   └── docs/            # 文档
├── p2p-cpp/             # C++ 引擎
│   ├── include/p2p/     # 头文件
│   ├── src/             # 源代码
│   ├── tests/           # 测试
│   └── docs/            # 文档
├── p2p_engine/          # Python 引擎
│   ├── core/            # 核心模块
│   ├── protocol/        # 协议实现
│   ├── transport/       # 传输层
│   ├── nat/             # NAT 穿透
│   └── security/        # 安全模块
├── servers/             # 服务器
│   ├── stun/            # STUN 服务器
│   ├── relay/           # Relay 服务器
│   ├── signaling/       # 信令服务器
│   └── did/             # DID 服务
├── tests/               # 集成测试
│   ├── unit/            # 单元测试
│   ├── integration/     # 集成测试
│   └── stress/          # 压力测试
├── docs/                # 文档
├── scripts/             # 脚本
└── docker/              # Docker 配置
```

### 核心模块说明

#### Core 模块

核心引擎，提供基础的 P2P 功能：

- `Engine`: 主引擎类，管理所有 P2P 连接
- `Connection`: 连接类，表示一个 P2P 连接
- `EventBus`: 事件总线，处理异步事件

#### Protocol 模块

协议实现，兼容 libp2p：

- `Handshake`: 握手协议
- `Channel`: 通道协议
- `DCUtR`: 直连中继转换协议
- `CircuitRelay`: 电路中继协议

#### Transport 模块

传输层实现：

- `TCP`: TCP 传输
- `UDP`: UDP 传输
- `QUIC`: QUIC 传输
- `WebRTC`: WebRTC 传输

#### NAT 模块

NAT 穿透功能：

- `STUNClient`: STUN 客户端
- `Puncher`: NAT 打孔器
- `Detector`: NAT 类型检测器

---

## 开发工作流

### TDD 开发流程

1. **编写测试**（先写测试，了解需求）
2. **运行测试**（确认测试失败）
3. **实现功能**（最小化实现）
4. **运行测试**（确认测试通过）
5. **重构代码**（优化实现）
6. **重复**

```bash
# 1. 创建测试文件
touch tests/unit/test_new_feature.py

# 2. 编写测试（RED）
vim tests/unit/test_new_feature.py

# 3. 运行测试（失败）
pytest tests/unit/test_new_feature.py -v

# 4. 实现功能（GREEN）
vim src/new_feature.py

# 5. 运行测试（通过）
pytest tests/unit/test_new_feature.py -v

# 6. 重构（IMPROVE）
vim src/new_feature.py
pytest tests/unit/test_new_feature.py -v

# 7. 检查覆盖率
pytest --cov=src --cov-report=html
```

### 调试技巧

#### Python 调试

```bash
# 使用 pdb
python -m pdb script.py

# 在测试中使用 pdb
pytest --pdb

# 使用 ipdb（更友好）
pip install ipdb
export PYTHONBREAKPOINT=ipdb.set_trace
```

#### C++ 调试

```bash
# 使用 GDB
gdb ./build/tests/unit_test

# 常用命令
(gdb) run                    # 运行程序
(gdb) bt                     # 查看堆栈
(gdb) frame 0                # 切换帧
(gdb) print variable         # 打印变量
(gdb) break main             # 设置断点
```

#### 日志调试

```python
import logging

# 配置日志
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

# 使用日志
logger = logging.getLogger(__name__)
logger.debug("Debug message")
logger.info("Info message")
logger.error("Error message", exc_info=True)
```

---

## 常见开发任务

### 添加新的协议支持

1. 在 `p2p_engine/protocol/` 创建新模块
2. 定义协议接口
3. 实现协议逻辑
4. 编写单元测试
5. 更新文档

```python
# p2p_engine/protocol/new_protocol.py
from .base import BaseProtocol

class NewProtocol(BaseProtocol):
    """新协议实现"""

    async def handshake(self, peer_id: str) -> bool:
        """握手"""
        pass

    async def send(self, data: bytes) -> bool:
        """发送数据"""
        pass

    async def recv(self) -> bytes:
        """接收数据"""
        pass
```

### 添加新的传输方式

1. 在 `p2p_engine/transport/` 创建新模块
2. 实现传输接口
3. 注册到传输管理器
4. 编写测试

### 添加新的服务器

1. 在 `servers/` 创建新目录
2. 实现服务器逻辑
3. 添加 Docker 配置
4. 更新 docker-compose.yml

### 性能优化

1. 使用 profiler 分析性能
2. 识别瓶颈
3. 优化代码
4. 验证改进

```python
# 使用 cProfile
python -m cProfile -o profile.stats script.py

# 分析结果
python -c "
import pstats
p = pstats.Stats('profile.stats')
p.sort_stats('cumtime')
p.print_stats(20)
"
```

---

## 发布流程

### 版本号规范

遵循 **Semantic Versioning**:

```
MAJOR.MINOR.PATCH

MAJOR: 不兼容的 API 变更
MINOR: 向后兼容的功能新增
PATCH: 向后兼容的 Bug 修复
```

### 发布步骤

1. **更新版本号**

```bash
# 更新版本
vim client_sdk/pyproject.toml
vim p2p-cpp/CMakeLists.txt
```

2. **更新 CHANGELOG**

```bash
vim CHANGELOG.md

# 添加新版本条目
## [1.1.0] - 2026-03-18

### Added
- New feature 1
- New feature 2

### Fixed
- Bug fix 1
- Bug fix 2

### Changed
- Change 1
```

3. **运行完整测试**

```bash
# 单元测试
pytest tests/unit/ -v

# 集成测试
pytest tests/integration/ -v

# 压力测试
pytest tests/stress/ -v
```

4. **构建发布包**

```bash
# Python 包
cd client_sdk
poetry build

# C++ 库
cd ../p2p-cpp/build
make package

# Docker 镜像
cd ../..
docker-compose build
```

5. **创建 Git 标签**

```bash
git tag -a v1.1.0 -m "Release v1.1.0"
git push origin v1.1.0
```

6. **发布到 PyPI**

```bash
cd client_sdk
poetry publish
```

7. **发布 Docker 镜像**

```bash
docker tag peerlink:latest peerlink:1.1.0
docker push peerlink:latest
docker push peerlink:1.1.0
```

---

## 获取帮助

- **文档**: [docs/](./)
- **Issues**: [GitHub Issues](https://github.com/hbliu007/peerlink/issues)
- **Discussions**: [GitHub Discussions](https://github.com/hbliu007/peerlink/discussions)
- **邮件**: dev@peerlink-project.io

---

## 许可证

贡献的代码将使用 MIT 许可证发布。

---

**文档版本**: 1.0
**最后更新**: 2026-03-18
