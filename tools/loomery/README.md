# Loomery

**本地 Markdown 图文笔记阅读器**：直接读取磁盘上的 `.md` 文件，内嵌 Mermaid 把笔记里的结构可视化，并提供全库引用图与全文检索。

它解决的真实问题：笔记以 Markdown 存放时**在编辑器里读起来是纯文本**，图（Mermaid）、引用关系（`[[wiki 链接]]`）与全库检索这三样东西没有统一的地方看。Loomery 只做"看"——内容的唯一真源始终是磁盘上的 Markdown 文件。

---

## 当前状态

| 项 | 值 |
| :-- | :-- |
| 状态 | **规划中（尚未开工，无源码）** |
| 版本 | `0.1.0`（未发布） |
| 稳定性等级 | `experimental` |
| 分支 | `tool/loomery` |
| `common/` 兼容基线 | `0d8c995` |

**当前仓库里只有文档**。下面的构建命令在骨架落地前不会成功，保留在此是为了明确目标形态。

---

## 构建与运行

> ⚠ 以下命令需要先完成 `CMakeLists.txt` 与源码骨架（见"已知限制"）。

```powershell
# 从仓库根目录
build.bat windows-ninja-debug

# 产物
build\windows-ninja-debug\bin\loomery.exe
```

运行：

```powershell
# 独立运行（不接宿主），指定笔记库
loomery.exe --library "D:\Notes" --no-steward

# 仅建索引，不开窗口（脚本与测试用）
loomery.exe --library "D:\Notes" --no-window --reindex
```

命令行参数与退出码的完整定义见 `Docs/ARCHITECTURE_DESIGN.md` §4.3 / §4.6。

---

## 与 Steward 的接入方式

| 项 | 值 |
| :-- | :-- |
| manifest | `Docs/tool.manifest.json` |
| `tool_id` | `loomery` |
| `transport` | `named-pipe` |
| 管道路径 | `\\.\pipe\atrium.steward.loomery` |
| 角色 | 本工具是**服务端**，`Steward` 是客户端 |
| 单实例 | `singleton = true`；冲突时以退出码 `4` 退出 |
| 协议版本 | `protocol_version = 1` |

**独立可运行是硬要求**（仓库规范 §7.2）：脱离 `Steward` 时功能**不削减也不加强**，接入只是增量能力（可被唤醒、可被观测、可被统一关闭）。

---

## 依赖与前置条件

| 依赖 | 用途 | 形态 |
| :-- | :-- | :-- |
| Windows 10/11 x64 | 目标平台（MVP 仅 Windows） | 系统 |
| WebView2 Runtime | 承载文档渲染面 | 系统自带（Win10/11） |
| SQLite | 索引与全文检索 | amalgamation，`third_party/sqlite/<version>/` |
| WebView2 SDK | C++ 宿主 | `third_party/`，锁定版本 |
| Mermaid / markdown-it / KaTeX | 页面侧渲染 | `third_party/`，**提交进仓库并记录 sha256**（决策 D-17） |

依赖清单与许可证记录在仓库根 `docs/DEPENDENCIES.md`。

**不在构建期下载依赖、不在运行期走 CDN**——工具必须离线可用。

---

## 已知限制与 TODO

### 已知限制

1. **尚未开工**：无任何源码，本文档描述的是目标形态。
2. **仅 Windows**：命名管道与 WebView2 均为 Windows 专有；macOS 需等价实现（`AF_UNIX` + 其他 web 载体），不在本期范围。
3. 不做视频与 PDF 承载（MVP 零媒体）。
4. 不做编辑：本工具是阅读器，不修改笔记内容。
5. 不做数据库视图与单篇子图（决策 D-15）。

### TODO（按顺序）

1. **阻塞项**：完成 WebView2 最小宿主实测（冷启动 / 常驻内存 / **退出后无 `msedgewebview2.exe` 残留**）——该实测通过前架构不视为定案。
2. 在 `develop` 分支落地 `third_party/` 与 `docs/DEPENDENCIES.md`（框架侧，见架构文档 §6.2 注）。
3. 建立工具骨架：`CMakeLists.txt`、`main.cpp`、`contract/`（manifest + 帧编解码）。
4. 实现 `mdscan/` + `index/`，跑通 10,000 文件全量索引与检索。
5. 实现 `shell/` + `bridge/` + `resources/page/`，跑通"读到一篇笔记含 Mermaid"。
6. 实现 `graph/` 与图视图。
7. 补齐 §7 的四类测试，尤其是**孤儿进程检测**。

---

## 文档

| 文档 | 内容 |
| :-- | :-- |
| [`Docs/ARCHITECTURE_DESIGN.md`](Docs/ARCHITECTURE_DESIGN.md) | **架构设计**：职责边界、分层与模块、数据模型、外部接口、生命周期、构建与依赖、测试策略、性能预算、风险与未决项 |
| `Docs/tool.manifest.json` | Steward 工具清单（待创建） |
| `Docs/BUILD.md` | 工具级构建说明（待创建） |
| 仓库根 `README.md` | 仓库级规范（**v2.2**），本文档服从其 §2 / §5 / §7 / §10 |
