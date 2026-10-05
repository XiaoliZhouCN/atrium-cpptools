# 命名与接口规范

> **来源**：从 `README.md` §4、§7 拆分而来（2026-09-19 文档瘦身）

## 1. 三元一致原则

同一个工具在以下三处必须使用**同一个**标识：

| 位置 | 形式 | 示例 |
| :-- | :-- | :-- |
| 仓库目录 | `PascalCase` | `Loomery/` |
| Git 分支 | `tool/<kebab-case>` | `tool/loomery` |
| `tool_id`（manifest / Steward 注册表） | `kebab-case` | `loomery` |

## 2. 可执行文件命名

- 产物名统一为 `kebab-case`，与 `tool_id` 一致：`loomery.exe`
- 禁止使用空格、中文、大小写混排作为产物名
- 调试与发布产物同名，仅以构建目录区分

## 3. 工具名命名规范

工具名 = **一个单义名词，不加 `Atrium` 前缀**。

| 规则 | 说明 |
| :-- | :-- |
| **不加 `Atrium` 前缀** | 前缀属于**工作区家族**（`AtriumSteward` / `AtriumPyTools` / `AtriumCppTools`），不属于单个工具。`AtriumPyTools` 下的工具本来就是 `colorpicker` / `filechecker`，本仓库与之对齐 |
| 禁止形态词 | 不带 `Canvas` / `App` / `Tool` / `Editor` / `Studio`——形态会变，名字不该承载形态 |
| 禁止与仓库角色撞名 | 不得用 `Note`（内容仓库为 `AtriumNote`），不得用语言标记（`Py` / `Cpp`） |
| 必须单义 | 名字应指向**能力**而非实现手段；同时能被一句话解释 |
| 长度 | 建议 ≤ 16 字符，因为要同时派生目录名、分支名、`tool_id`、管道名 |
| 名称冲突 | 定名前必须检索一次，避开同领域知名产品名——重名会在搜索、分发、包管理上持续撞车 |
| 名不符实是缺陷 | 名字暗示了不具备的能力（如叫 `Canvas` 却不支持自由摆放）会持续误导实现方向，按文档缺陷处理 |

### 3.1 命名决策记录

首个工具 `AtriumCanvasNote` → `AtriumLoom` → **`Loomery`**（v2.0）。

1. `AtriumCanvasNote`：`Note` 与内容仓库 `AtriumNote` 撞名，与"应用与内容必须分离"的意图冲突；`Canvas` 暗示自由画布，而定位本质是图不是画布。
2. `AtriumLoom`：隐喻正确——把文字、图、引用**织**在一起；但 `Atrium` 前缀与 `AtriumPyTools` 的既有实践不一致。
3. **`Loomery`**（织造工坊，`loom` 的派生词）：保留 loom 词根与"织"的隐喻；避开 `Loom`（Loom.com 录屏工具，同领域重名）与 `Atlas`（MongoDB Atlas 等大量撞名）；无 `Atrium` 前缀。

> **`namespace atrium` 不随工具名变化**：`atrium::common` 里的 `atrium` 指**工作区家族**，与"工具名不加前缀"是两件事。去掉工具名前缀**不影响** `namespace atrium`。

## 4. CLI 参数（除协议外的第二条稳定接口）

- 必须支持 `--help`、`--version`
- 必须支持 `--no-steward`（或等价开关）用于脱离宿主独立调试
- 参数解析失败时返回退出码 `2`，并输出可读原因
- 禁止在参数名上使用宿主实现细节（如窗口类名）

## 5. 独立可运行原则

工具必须在**没有安装 / 没有运行** `Steward` 的环境下独立启动并完成**全部功能**——是"完整产品"，不是"完整产品的降级版"。违规设计直接否决，这是防止宿主膨胀与保证可测试性的底线。

| # | 要求 | 反例（禁止） |
| :- | :-- | :-- |
| 1 | 所有功能在无 `Steward` 时可用 | "查看笔记库概览需要 Steward 提供上下文" |
| 2 | 不要求额外加强，但也不允许削减 | "脱离 Steward 时只能只读打开" |
| 3 | `Steward` 不托管工具窗口、不代管工具状态、不代跑工具业务 | 工具把视图状态寄存在宿主进程 |
| 4 | 工具的本地配置与状态必须自洽于自身 | 配置只能从宿主下发 |
| 5 | 接入 `Steward` 只是**增量能力**（可被唤醒、可被观测、可被统一关闭） | 接入后功能变多、不接入则功能缺失 |

## 6. 语言与工程标准

- 语言标准：**C++20**（与 `NexusRenderer` / `ChromaCMS` 基线对齐；如需 C++23 需在架构文档说明理由）
- 编译器：MSVC x64（主力），允许 MinGW/Clang 作为交叉验证；平台差异必须在架构文档中声明
- 构建：CMake + `CMakePresets.json`，预设命名沿用 `windows-ninja-debug` / `windows-vs2026-debug` 形态
- 依赖：优先系统依赖与 header-only；引入重型第三方库必须在架构文档中记录理由与体积代价
- 线程与异步：UI 线程禁止阻塞 IO；长任务必须有取消路径
- 内存与资源：RAII 为默认；禁止裸 `new/delete`；跨 DLL 边界不得传递 STL 对象所有权（除非 ABI 明确约定）

## 7. 编码与文本

- 所有文本文件统一 UTF-8（无 BOM），换行按 `.gitattributes` 归一
- 禁止在源码与文档中提交乱码或错误路径标记的文本

> **已知待修**：`AtriumPyTools/tools/colorpicker/__init__.py` 与 `AtriumSteward` 多处源码注释仍带旧 `ChestPyTools` / `ChestSteward` 路径标记，应在各自仓库修复。

## 8. 稳定性分级

工具必须在 manifest 中声明稳定性等级，宿主据此决定是否默认启用：

| 等级 | 含义 | 宿主行为 |
| :-- | :-- | :-- |
| `experimental` | 接口可能变 | 默认隐藏，手动开启 |
| `beta` | 功能可用，契约稳定 | 默认展示，标注 |
| `stable` | 生产可用 | 默认启用 |
