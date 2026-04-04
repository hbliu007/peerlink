# 贡献指南

欢迎参与 PeerLink 开源项目！本文档帮助你了解如何为项目做出贡献。

## 如何贡献

### 报告问题

在提交 Issue 之前，请先：

1. 搜索 [已有 Issues](https://github.com/your-org/peerlink/issues) 确认问题未被报告
2. 收集必要的调试信息：
   - PeerLink 版本（`peerlink --version`）
   - 操作系统和版本
   - 网络环境（NAT 类型、防火墙策略）
   - 相关日志输出

**Bug 报告模板：**

```markdown
## Bug 描述
简要描述问题

## 复现步骤
1. ...
2. ...

## 期望行为
描述你期望发生什么

## 实际行为
描述实际发生了什么

## 环境信息
- PeerLink 版本:
- 操作系统:
- 网络环境:

## 日志
```
粘贴相关日志（去除敏感信息）
```
```

**功能请求模板：**

```markdown
## 需求描述
你希望实现什么功能？

## 使用场景
描述你遇到的问题或使用场景

## 建议方案
如果有想法，描述可能的实现方案
```

### 提交 Pull Request

1. **Fork 仓库** → 点击 GitHub 页面右上角 Fork 按钮
2. **创建分支** → `git checkout -b feat/your-feature-name`
3. **编写代码** → 遵循项目代码风格和规范
4. **编写测试** → 确保新功能有对应的测试用例
5. **提交代码** → 使用 Conventional Commits 格式：

```
feat: 添加 XX 功能
fix: 修复 XX 问题
refactor: 重构 XX 模块
docs: 更新 XX 文档
test: 添加 XX 测试
chore: 更新构建配置
```

6. **推送并创建 PR** → `git push origin feat/your-feature-name`

### PR 审查流程

1. 所有 PR 需要至少一位维护者审查
2. CI 检查必须通过（编译 + 测试）
3. 代码风格符合项目规范
4. 新功能必须包含测试用例
5. 破坏性变更需在 PR 描述中明确说明

## 开发环境搭建

参见 [开发指南](development.md) 了解如何搭建开发环境。

## 代码规范

- C++ 代码遵循 C++20 标准
- 使用 `clang-format` 格式化代码
- 公共 API 添加 Doxygen 注释
- 函数体不超过 50 行
- 文件不超过 800 行

## 行为准则

- 尊重每一位贡献者
- 建设性的讨论和反馈
- 专注于问题本身，不针对个人
- 欢迎不同水平的开发者参与

## 贡献类型

我们欢迎以下类型的贡献：

- **代码** — 新功能、Bug 修复、性能优化
- **文档** — 教程、API 文档、翻译
- **测试** — 单元测试、集成测试、E2E 测试
- **反馈** — Bug 报告、功能请求、用户体验反馈

## 联系方式

- **GitHub Issues** — Bug 报告和功能请求
- **GitHub Discussions** — 问题和讨论
- **邮件** — peerlink-dev@googlegroups.com
