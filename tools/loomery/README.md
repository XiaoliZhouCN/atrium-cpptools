# Loomery

**本地 Markdown 图文笔记阅读器**：读取磁盘上的 `.md` 文件，把正文与其中的 Mermaid 图渲染出来。

## 当前状态

| 项 | 值 |
| :-- | :-- |
| 状态 | **骨架**——只有一个空窗口，没有文档、没有渲染面、没有索引 |
| 版本 | `0.1.0`（未发布） |
| 目录 | `tools/loomery/` |
| 分支 | `tool/loomery` |
| 渲染面 | **Qt WebEngine**（已定，尚未接入） |

## 构建与运行

需要 Qt 6（`Widgets`；接入渲染面后还需要 `WebEngineWidgets`）。

```powershell
cmake --preset windows-vs2026-debug
cmake --build --preset build-windows-vs2026-debug
.\build\windows-vs2026-debug\bin\loomery.exe
```

## 与宿主（Steward）的接入

**尚未接入。** 计划中的接入参数：

| 项 | 值 |
| :-- | :-- |
| `tool_id` | `loomery` |
| `transport` | `named-pipe` |
| 管道路径 | `\\.\pipe\atrium.steward.loomery` |
| 角色 | 本工具是服务端，宿主是客户端 |

接入是**增量能力**：脱离宿主时功能不削减（工具是完整产品）。

## 依赖

| 依赖 | 用途 | 状态 |
| :-- | :-- | :-- |
| Qt 6 `Widgets` | 窗口与控件 | 已用 |
| Qt 6 `WebEngineWidgets` | 文档渲染面 | 待接入 |
| Markdown / Mermaid 渲染器 | 页面侧 JS | 待引入 |

**Markdown 与 Mermaid 都不自己实现**——两者都交给页面侧的现成生态。

## 已知限制与 TODO

**限制**

1. 目前只有一个空窗口，不能打开任何文件。
2. 仅规划 Windows（Qt 本身跨平台，但宿主与打包暂只考虑 Windows）。

**TODO（按顺序）**

1. 接入 Qt WebEngine 视图，把一篇 `.md` 渲染出来（含 Mermaid）。
2. 定下架构与目录组织（模块边界、依赖方向）。
3. 再谈索引、检索与图视图。

## 文档

| 文档 | 内容 |
| :-- | :-- |
| `Docs/ARCHITECTURE_DESIGN.md` | 架构设计（待编写，等架构定案） |
| 仓库根 `README.md` | 仓库级规范（**当前与代码结构已不一致，待重写**） |
