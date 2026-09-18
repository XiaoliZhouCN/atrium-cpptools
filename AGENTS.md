# AGENTS.md

本文件是 `AtriumCppTools` 仓库级 Agent 规则入口；子目录中的 `AGENTS.md` 优先级更高。

仓库级规范以 `README.md`（v2.1）为唯一权威来源。本文件只承载**协作边界与权限**，不重复规范条文；出现冲突时以 `README.md` 为准。

## 适用范围

- 适用于 `D:\Repositories\Manager\AtriumCppTools\` 整个仓库。
- 若某个子目录存在自己的 `AGENTS.md`，则该子目录规则覆盖本文件的局部内容，并继承本文件其余约束。

## 角色与权限

- **架构师 Agent**：负责目录规划、接口骨架、架构文档与必要函数框架；允许创建分支、提交、推送；不允许开 PR。
- **模块开发 Agent**：负责具体模块功能实现；仅允许修改本地文件，不允许建分支、提交、推送、开 PR。
- **测试 + 文档管理 Agent**：负责测试补充、验证执行、文档整理与同步；仅允许修改本地文件，不允许建分支、提交、推送、开 PR。

## 先读后改

改动任何目录前，必须先读：

1. `README.md` 的对应章节（尤其 §1.2 分支隔离、§2 设计原则、§5 生命周期契约、§10 红线）。
2. 目标目录的局部 `AGENTS.md`（若存在）。
3. 目标工具的 `Docs/ARCHITECTURE_DESIGN.md`（若已存在）。

## 当前模块边界

- `common/`：工具间共享。**准入条件**：必须有第二个消费者，**接口本身也算**——没有消费者的接口不许进 `common/`（README §2.3）。当前只放 `text` / `error` / `log` / `paths` / `platform`。
- `tools/`：具体工具。每个工具一个独立 CMake target，禁止互相 link。当前为空，首个工具为 `Loomery`（尚未开工，见 README §12.1）。
- `third_party/`：第三方依赖，版本必须锁定；**禁止直接修改第三方源码**。
- `docs/`：仓库级文档（构建、依赖清单、协议同步记录）。

## 禁止事项（高频违规点）

- 禁止在 `main` 上直接提交；禁止在工具分支修改 `common/`、顶层 CMake 与 presets（框架改动只走 `develop`，README §1.2）。
- 禁止在工具分支复制框架源码以绕开依赖（红线 C13）。
- 禁止工具 `#include` 或 link `AtriumSteward` 源码（红线 C1）。
- 禁止依赖 `Projects/` 下任何项目（红线 C14）。
- 禁止把第三方实现与资源（`libav*.h` / `pdfium.h` / 图渲染后端 / `mermaid.min.js` 等 JS 资源）放进 `common/`（红线 C15、目录红线 D8）。
- **禁止自行实现 Mermaid 的解析 / 布局 / 绘制**，必须调用现成实现（红线 C17、README §2.2.2）。
- 禁止在工具名上加 `Atrium` 前缀（红线 C16、README §4.3）。
- 禁止为未通过 README §11 准入评估的候选需求建目录（红线 C6）。
- 禁止提交构建产物、`__pycache__`、IDE 工程文件；`build/` 必须保持被忽略（目录红线 D6）。
- **在框架分支（`develop` / `dev/basic`）提交时禁止 `git add -A`，一律使用显式路径。** 各分支共用同一个工作区，可能残留只在工具分支存在的未跟踪产物；而**只存在于工具分支的 `.gitignore` 在框架分支上并不存在**，那些产物会因此不再被忽略、被一并提交。此坑已真实发生过一次：`checkout develop` 删掉了探针目录里的 `.gitignore`，随后 `git add -A` 把 WebView2 SDK 的解包文件与 9 MB 的 nupkg 一起提交进了 `develop`（违反红线 C7 / 目录红线 D6）。
- 禁止修改 `tools/CMakeLists.txt`：工具通过自动发现注册，不需要（也不允许）改注册表（README §1.2 第 7 条）。
- 禁止提交乱码文本；所有文本文件为 UTF-8 无 BOM（README §7.4）。
- 禁止手工修改生成文件；如需变更应通过脚本或生成流程重建。

## 最小验证要求

任何改动提交前必须完成（README §9.2）：

1. **干净构建通过**（非增量）：`build.bat windows-ninja-debug`
2. **契约测试通过**：`build.bat windows-ninja-debug test`
3. 若改动涉及工具进程：手动执行一次 `启动 → 就绪 → 收到 shutdown → 退出码 0`，并确认宿主退出后无残留进程。
4. 若改动涉及接口或契约：同步更新对应 `Docs/ARCHITECTURE_DESIGN.md` 与 README 的版本号。

> **Agent / 自动化会话注意**：Ninja 预设在该类会话中可能出现"构建完成但进程不退出"（会话环境问题，非工程缺陷，详见 `docs/BUILD.md`）。此时改用 `windows-vs2026-debug` 预设，并**直接调用 `cmake` / `ctest`**，不经 `build.bat`。普通交互式终端不受影响。

**禁止的"验证"**（README §9.3）：只看编译通过即宣称完成；只测 `--no-steward` 路径而从未验证协议路径；手工杀进程后不检查孤儿进程。

## 当前未决项（不得擅自决定）

以下事项需人工决策，Agent 不得自行拍板：

- `README.md` §13.2 的 Q1~Q16；
- `Loomery` 的 WebView2 宿主成本实测与正式定案（README §12.1.4、Q9、Q13）；
- Markdown 解析放在显示侧还是索引侧（README §12.1.1 相邻项，尚无定论）；
- `tools` 分支的派生来源与工具分支获取 `common/` 的方式（Q15）；
- `dev/basic` → `develop` 的合入时机（Q16）；
- 与 `AtriumSteward` 的协议字段命名冻结（§5.2）；
- 工作区 `Docs/` vs `docs/` 大小写统一（Q3）。

以下事项**已定案**，Agent 可直接依此实施，无需再确认：D-14 Markdown 方言范围、D-15 MVP 视图边界（文档 + 全库图）、D-16 C++↔JS 桥（内置 JSON 通道 + 版本化信封）、D-17 第三方 JS 资源提交进 `third_party/` 并记录 sha256。

## 详细规则入口

- 仓库级规范：`README.md`
- 工具清单与新增工具检查表：`tools/README.md`
- 共享库准入与禁止项：`common/`（见 `CMakeLists.txt` 头部注释）
