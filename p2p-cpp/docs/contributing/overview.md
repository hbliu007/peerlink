# 贡献指南

欢迎为 PeerLink 项目做出贡献！

---

## 如何贡献

我们欢迎各种形式的贡献：

- 🐛 报告 Bug
- 💡 提出新功能
- 📖 改进文档
- 🔧 提交代码
- ✅ 修复 Bug
- 🌍 帮助翻译

---

## 行为准则

### 我们的承诺

为了营造开放和友好的环境，我们承诺：

- 尊重不同的观点和经验
- 使用友好的语言
- 优雅地接受建设性批评
- 关注对社区最有利的事情
- 对其他社区成员表示同理心

### 不可接受的行为

- 使用性化的语言或图像
- 人身攻击或侮辱性评论
- 公开或私下骚扰
- 未经许可发布他人私人信息
- 其他不专业或不适当的行为

---

## 报告 Bug

### 报告前请检查

1. 查看已有的 [Issues](https://github.com/your-org/peerlink/issues)
2. 确认问题未被报告过

### Bug 报告模板

```markdown
### Bug 描述

简洁地描述 Bug。

### 复现步骤

1. 执行 '...'
2. 点击 '....'
3. 滚动到 '....'
4. 看到错误

### 预期行为

描述你期望发生什么。

### 实际行为

描述实际发生了什么。

### 环境信息

- OS: [e.g. Ubuntu 22.04]
- PeerLink 版本: [e.g. v1.0.0]
- 编译器: [e.g. GCC 11.3]

### 日志

```
粘贴相关日志输出
```

### 附加信息

其他有助于理解问题的信息。
```

---

## 功能请求

### 功能请求模板

```markdown
### 功能描述

简洁地描述你想要的功能。

### 问题或需求

这个功能解决什么问题？

### 建议的解决方案

描述你希望这个功能如何工作。

### 替代方案

描述你考虑过的其他解决方案。

### 附加信息

其他相关信息或截图。
```

---

## 开发流程

### 1. Fork 仓库

```bash
# Fork https://github.com/your-org/peerlink
# 然后克隆你的 fork
git clone https://github.com/your-username/peerlink.git
cd peerlink
```

### 2. 创建分支

```bash
# 从 main 分支创建新分支
git checkout -b feature/your-feature-name

# 或修复 Bug
git checkout -b fix/your-bug-fix
```

### 3. 编写代码

遵循项目的[代码规范](development.md)。

### 4. 运行测试

```bash
# 运行所有测试
make test

# 运行特定测试
make test TEST=peerlink_test

# 检查覆盖率
make coverage
```

### 5. 提交代码

```bash
git add .
git commit -m "feat: add XYZ feature"
```

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 格式：

- `feat:` 新功能
- `fix:` Bug 修复
- `docs:` 文档更新
- `style:` 代码格式调整
- `refactor:` 代码重构
- `perf:` 性能优化
- `test:` 测试相关
- `chore:` 构建/工具相关

### 6. 推送到你的 Fork

```bash
git push origin feature/your-feature-name
```

### 7. 创建 Pull Request

访问 GitHub 并创建 Pull Request。

### PR 模板

```markdown
### 描述

简要描述此 PR 的更改。

### 类型

- [ ] Bug 修复
- [ ] 新功能
- [ ] 代码重构
- [ ] 文档更新
- [ ] 性能优化
- [ ] 其他

### 相关 Issue

关闭 #(issue number)

### 更改说明

- 更改 1
- 更改 2

### 测试

描述你如何测试这些更改：
- [ ] 单元测试通过
- [ ] 集成测试通过
- [ ] 手动测试通过

### 检查清单

- [ ] 代码符合项目规范
- [ ] 添加了必要的测试
- [ ] 测试通过
- [ ] 更新了相关文档
- [ ] 提交信息清晰明确
```

---

## 代码规范

### C++ 代码

- 使用 C++20 标准
- 遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- 使用 `clang-format` 格式化代码
- 使用 `clang-tidy` 检查代码质量

### 代码格式化

```bash
# 格式化代码
make format

# 检查格式
make check-format
```

### 代码审查

所有 PR 都需要经过代码审查：

1. 至少一位维护者批准
2. 所有 CI 检查通过
3. 没有重大的审查意见

---

## 测试要求

### 测试覆盖率

- 新代码需要 > 80% 的测试覆盖率
- 关键路径需要 100% 覆盖

### 测试类型

1. **单元测试**: 测试单个函数/类
2. **集成测试**: 测试模块间交互
3. **端到端测试**: 测试完整场景

### 添加测试

```cpp
// tests/unit/example_test.cpp
#include <gtest/gtest.h>
#include <peerlink/example.hpp>

TEST(ExampleTest, BasicTest) {
    peerlink::Example example;
    EXPECT_EQ(example.get_value(), 42);
}
```

---

## 文档

### 文档要求

1. **API 文档**: 所有公共 API 需要注释
2. **用户文档**: 新功能需要用户指南
3. **设计文档**: 重大变更需要设计文档

### 文档风格

- 使用简洁清晰的语言
- 提供代码示例
- 包含使用场景

---

## 社区

### 沟通渠道

- **GitHub Issues**: 报告 Bug 和功能请求
- **GitHub Discussions**: 一般讨论和问答
- **Slack**: 实时交流（[加入链接]）

### 维护者

项目由以下团队维护：

- **核心团队**: 架构设计和方向
- **贡献者**: 代码贡献和审查
- **文档团队**: 文档维护

---

## 许可证

通过贡献代码，你同意你的贡献将按照项目的 [MIT License](https://github.com/your-org/peerlink/blob/main/LICENSE) 进行许可。

---

## 致谢

感谢所有为 PeerLink 做出贡献的人！

---

**下一步**: [开发指南](development.md) · [测试指南](testing.md)
