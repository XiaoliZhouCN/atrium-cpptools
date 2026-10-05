# 分支模型与工具隔离

> **来源**：从 `README.md` §1 拆分而来（2026-09-19 文档瘦身）
> **当前实测状态**：见 `AtriumSteward/README_BRANCHES.md`

## 1. 分支布局

- `main`：主分支，`develop` + `tools` 的汇总，只做合入，不直接开发。
- `develop`：框架开发分支，承载 `common/`、构建系统、契约基线。
- `dev/basic`：框架开发分支，合入 `develop`。
- `tools`：工具汇总分支，所有工具合入本分支。
- `tool/<kebab-case>`：具体工具开发分支，合入 `tools`。

> **为什么工具分支是 `tool/` 而不是 `tools/`**（v2.2 修正）：git 无法同时存在 `refs/heads/tools` 与 `refs/heads/tools/<x>`——前者是一个文件，后者要求它是目录。实测（git 2.55.0）：
>
> ```
> $ git branch tools            # 成功
> $ git branch tools/loomery
> fatal: cannot lock ref 'refs/heads/tools/loomery':
>        'refs/heads/tools' exists; cannot create 'refs/heads/tools/loomery'
> ```
>
> 这是 git ref 存储模型的硬约束，无法绕过（红线 C18）。v1.x 的 §1.1 同时定义了 `tools` 与 `tools/xxx`，**该条文不可实施**。
>
> **派生链**：`develop` →（建立）`tools` →（建立）`tool/<tool>`。工具分支从 `tools` 派生，因此天然包含 `common/`，可独立构建；框架升级按 §2 第 5 条显式同步。

## 2. 框架与工具的隔离模型（已确认）

**决策**：`tools/*` 只合入 `tools`，**不合入** `develop`。框架（`develop`）与工具（`tools`）保持隔离。

这个模型是有效的，但必须配一组纪律，否则会退化成"框架无法反哺 + 分支永久漂移"。以下规则是对该模型的补充，不是替代：

1. **框架改动一律走 `develop`**。任何 `common/`、CMake、`CMakePresets.json`、构建脚本、契约文档的改动，都必须在 `develop` 上完成并合入 `dev/basic`，禁止在工具分支上改框架。
2. **禁止在工具分支上"就地打补丁修框架"**。发现框架缺陷时，正确路径是 `develop` 修复 → 工具分支同步；若框架修复尚未合入，工具分支应等待，而不是各自临时绕过。
3. **禁止把框架源码复制进工具分支**以绕开依赖——这会把一次修复变成多份副本，是隔离模型最容易退化的路径。
4. **每个工具分支必须记录兼容基线**：在 `Docs/ARCHITECTURE_DESIGN.md` 中记录其 `common/` 兼容基线（commit SHA 或版本号），格式统一，便于宿主与 CI 校验。
5. **框架升级是显式同步动作**：工具分支升级兼容基线的唯一方式是显式 merge/rebase 并更新基线记录，不得靠自动合并静默漂移。
6. **边界判定**：`common/`、顶层 CMake、presets、构建脚本、契约文档属于**框架**；`tools/<ToolName>/` 下的一切属于**工具**。一旦两个工具都需要同一段代码，该代码必须下沉到 `common/`，禁止互相复制。
7. **工具注册是自动的，工具分支永远不改框架文件**：`tools/CMakeLists.txt` 以自动发现方式遍历其直属子目录（含 `CMakeLists.txt` 者即视为一个工具）。因此**新增工具只需把自己的目录放进分支**，不必回到 `develop` 改注册表；多个工具分支合入 `tools` 时也不会在注册文件上冲突。这条同时消掉了此前"`tools/CMakeLists.txt` 算框架还是工具"的归属争议——它是框架文件，且不需要被工具分支修改。

代价说明（明示，避免误读）：此模型下框架改进**不会**自动流入未同步的工具分支。这是有意接受的成本，由上述第 4、5 条控制，而不是靠"到时候再说"。

## 3. 分支操作规则

1. 禁止在 `main` 上直接提交。
2. 每个 `tool/<tool-id>` 分支必须包含所开发工具的 `README.md`；分支根目录保留仓库级 `README.md`。
3. 每个工具目录必须包含 `Docs/`，其中至少有 `ARCHITECTURE_DESIGN.md` 与 `README.md`（模板见 `DOC_STANDARDS.md`）。
4. 分支命名、工具目录名、`tool_id` 三者必须一致（见 `NAMING_AND_INTERFACES.md`）。
5. 删除分支前确认其内容已合入 `tools`。
6. 工具分支不得改动 `common/`、顶层 CMake 与 presets（见 §2 第 1 条）。

## 4. 高频违规点（真实发生过）

- **在框架分支（`develop` / `dev/basic`）提交时禁止 `git add -A`，一律使用显式路径。** 各分支共用同一个工作区，可能残留只在工具分支存在的未跟踪产物；而**只存在于工具分支的 `.gitignore` 在框架分支上并不存在**，那些产物会因此不再被忽略、被一并提交。此坑已真实发生过一次：`checkout develop` 删掉了探针目录里的 `.gitignore`，随后 `git add -A` 把 WebView2 SDK 的解包文件与 9 MB 的 nupkg 一起提交进了 `develop`（违反红线 C7 / 目录红线 D6）。
