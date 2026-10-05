# 决策与待确认事项

> **来源**：从 `README.md` §13 拆分而来（2026-09-19 文档瘦身）
> 事项关闭后应移入 §1 并更新本文件日期，不得只改正文。

## 1. 已确认（不再讨论，除非复盘）

| 编号 | 决策 | 落地位置 |
| :-- | :-- | :-- |
| D-1 | `tools/*` 只合入 `tools`，不合入 `develop`；框架与工具隔离，辅以纪律条款 | `BRANCH_MODEL.md` §2 |
| D-2 | 首个立项工具为 **`Loomery`**（Markdown 图文笔记阅读器 + 内嵌 Mermaid 可视化） | `README.md` §10 |
| D-3 | 传输通道为 Windows 命名管道，帧格式与单实例语义见契约 | `STEWARD_CONTRACT.md` §4.2 |
| D-4 | 与 `Projects/` 项目只允许「参考 / 搬运」，禁止构建期依赖 | `README.md` §1 |
| D-5 | 共享契约不共享实现；媒体等能力先定窄接口，实现留在工具内 | `README.md` §1 |
| D-6 | `Loomery` 不自行实现**图渲染（Mermaid）**、视频解码与 PDF 渲染；"媒体/PDF 索引器"不作为并列的第二工具 | `README.md` §1 |
| D-7 | `common/` 准入条件：**必须有第二个消费者，且包括接口本身**；当前只放 `text` / `error` / `log` / `paths` / `platform` | `README.md` §1 |
| D-8 | 工具是完整产品；`Steward` 只做生命周期控制，不承载运行内容；所有 C++ 工具都必须可独立运行 | `NAMING_AND_INTERFACES.md` §5 |
| D-9 | `Loomery` 规模对标 **10,000 个 Markdown 文件**；启动与帧率同等重要；MVP 零媒体，接口按第 4 档预留 | `README.md` §10.1.2 |
| D-10 | 命名按能力顺序而非愿望顺序；**工具名不加 `Atrium` 前缀**，首个工具定名 `Loomery` | `NAMING_AND_INTERFACES.md` §3 |
| D-11 | 不使用自有块存储，改为「**Markdown 文件为唯一真源**」，块 / 索引只是可重建的派生物 | `README.md` §10.1.1 |
| D-12 | 文档渲染面采用 **web 渲染**（暂定 WebView2），C++ 承载外壳、全库索引与文件 IO；`mermaid.min.js` 等 JS 资源进 `third_party/` 并**锁定版本** | `README.md` §10.1.3、§10.1.4 |
| D-13 | **禁止自行实现 Mermaid 的解析 / 布局 / 绘制**，必须调用现成实现 | 红线 C17 |
| D-14 | `Loomery` 的 **Markdown 方言范围**：GFM（表格 / 任务列表 / 删除线 / 自动链接）+ YAML frontmatter + `[[wiki 链接]]` + 脚注 + 提示块 + **数学公式**（KaTeX） | `README.md` §10.1.2 |
| D-15 | `Loomery` 的 **MVP 视图边界**：**文档阅读 + 全库图视图**；单篇子图与数据库视图不在 MVP | `README.md` §10.1.2 |
| D-16 | **C++ ↔ JS 桥**采用 WebView2 内置 JSON 消息通道，并外套**版本化信封**（`version` / `type` / `id` / `payload`），与契约协议同构，维护消息类型表 | `README.md` §10.1.2 |
| D-17 | 第三方 JS 资源（`mermaid.min.js` 等）**提交进仓库** `third_party/`（带版本号目录），并在依赖文档记录**来源与 sha256**；不在构建期下载、不在运行期走 CDN | 目录红线 D5 |
| D-18 | **分支布局修正**：汇总分支保持 `tools`，工具分支改为 `tool/<kebab-case>`（原 `tools/<kebab-case>` 与 `tools` 在 git 中不可共存）；派生链 `develop` → `tools` → `tool/<tool>` | `BRANCH_MODEL.md` §1、红线 C18 |

## 2. 待确认事项

| 编号 | 事项 | 影响 |
| :-- | :-- | :-- |
| Q1 | 框架（`common/`）与工具的版本锁定方式：单仓同步版本 vs 工具声明兼容范围 | 出现第二、第三个工具后会立刻遇到；当前由"记录兼容基线"临时兜底 |
| Q2 | `AtriumContracts/` 是否落地及其归属仓库 | 当前契约以本仓库文档为权威，长期需要单一权威 |
| Q3 | 工作区 `Docs/` vs `docs/` 大小写统一（当前本仓库用 `docs/`、`AtriumSteward` 用 `Docs/`） | 文档路径与脚本引用一致性 |
| Q4 | `Loomery` 与 `AtriumPyTools/tools/filechecker`、`colorpicker` 的功能边界 | 避免跨仓库重复实现 |
| Q5 | `Steward` 是否需要用 C++ 重写宿主（影响本仓库是否要提供宿主侧 SDK） | 决定 `common/` 是否需要 `host-sdk` 模块 |
| Q6 | 命名管道的协议版本协商时机（连接后首帧协商 vs 启动参数携带） | 决定工具启动路径与错误处理分支 |
| Q7 | ~~`common/platform` 的色标 POD 是否与 `ChromaCMS` / `NexusRenderer` 保持字段兼容~~ | **已作废**：随 `common/media/` 一并移除。若未来出现真实的媒体消费者，此问题随接口一起重新评估 |
| Q8 | ~~Markdown 方言范围~~ | **已关闭 → D-14** |
| Q9 | UI 技术栈 | **暂定 WebView2**（实测支持）；待宿主成本实测后正式定案 |
| Q10 | ~~视图的优先级与 MVP 边界~~ | **已关闭 → D-15** |
| Q11 | ~~块格式规范由谁维护~~ | **已作废**：改回 Markdown 为唯一真源，不存在自有块格式 |
| Q12 | ~~C++ ↔ JS 桥的接口形态与版本策略~~ | **已关闭 → D-16** |
| Q13 | WebView2 的释放流程与孤儿进程检测方式 | 直接关系禁止项与生命周期契约；宿主成本实测第三项的落地 |
| Q14 | ~~`mermaid.min.js` 等 JS 资源的版本锁定与升级流程~~ | **已关闭 → D-17** |
| Q15 | ~~`tools` 分支的派生来源与工具分支获取 `common/` 的方式~~ | **已关闭 → D-18**：派生链 `develop` → `tools` → `tool/<tool>` |
| Q16 | `dev/basic` → `develop` 的合入时机 | 框架已定案并验证通过，但尚未提交到远端分支 |

## 3. 阻止正式定案的风险项

`Loomery` 的 WebView2 **宿主成本**尚未实测，直接决定生命周期契约是否成立。三项必须测：

| 待测项 | 为什么必须测 | 对应约束 |
| :-- | :-- | :-- |
| 冷启动（创建 environment → 首次导航完成） | WebView2 会拉起 `msedgewebview2.exe` 及子进程，最可能击穿 ≤1500 ms | 启动预算 |
| 常驻内存 | Chromium 多进程通常加 80–250 MB | 内存预算 ≤400 MB |
| **杀进程后是否残留 `msedgewebview2.exe`** | 未正确释放 environment 会留下孤儿进程 | 明确禁止的行为；生命周期契约 |

第三项是选 WebView2 最大的集成风险，也是最容易漏掉的一项。它必须成为 shutdown 流程里的**显式步骤**和**一条测试用例**（Q13）。

## 4. 与其他文档的关系

| 文档 | 关系 |
| :-- | :-- |
| `AtriumSteward/Docs/ARCHITECTURE_DESIGN.md` | 上位架构依据 |
| `AtriumSteward/Docs/WORKSPACE_SPECIFICATION.md` | 工作区级规范（目录红线、文档要求、Agent 权限） |
| `AtriumSteward/README_BRANCHES.md` | 工作区分支总览，本仓库分支变更需同步 |
| `Projects/ChromaCMS/Docs/ARCHITECTURE_DESIGN.md` | **复用参考**：三层结构与红线 CI 化范本；FFmpeg 隔离、Qt 原生视口路径、Skia pin 策略 |
| `Projects/NexusRenderer/Docs/ARCHITECTURE_DESIGN.md` | **复用参考**：RHI 后端隔离、单向依赖；红线 CI 化的可机械判定写法见 `Projects/NexusRenderer/Tools/ci/static_checks.py` |
| `Projects/ChromaCMS/Docs/Agents/AGENTS.md` | 局部 `AGENTS.md` 的组织范式 |

**外部实测资料**（第三方，非本工作区文档）：宿主成本实测所依据的图渲染对比资料见 `README.md` §10.1.3。本机复测的可执行探针在**工作区临时目录 `.probe-merman/`，不属于本仓库，可随时删除**。
