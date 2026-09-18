# 构建说明

> 本文档描述 `AtriumCppTools` 的构建方式。规范依据见仓库根 `README.md` §8。
> 状态：骨架阶段（尚无工具）。最后更新：2026-09-18。

## 前置条件

| 项 | 要求 | 说明 |
| :-- | :-- | :-- |
| CMake | ≥ 3.25 | 预设文件版本为 6 |
| 编译器 | MSVC x64 | 语言标准 C++20（`README.md` §7.3） |
| Visual Studio | 2022 或 2026，含 "使用 C++ 的桌面开发" 工作负载 | `build.bat` 通过 `vswhere` 自动探测，无写死路径 |
| Ninja | 可选 | 仅 `windows-ninja-*` 预设需要；见下方"已知问题" |

## 快速开始

```bat
:: Visual Studio 生成器（当前环境下唯一可用的验证路径）
build.bat windows-vs2026-debug test

:: 仅构建
build.bat windows-vs2026-debug

:: Ninja 预设（若本机 Ninja 能正常退出，则增量更快；见下方"已知问题"）
build.bat windows-ninja-debug test
```

`build.bat` 的行为：

1. 校验 `cmake` 可用。
2. 若预设不是 `windows-vs*`，则用 `vswhere` 找到 MSVC 并通过 `vcvars64.bat` 导入环境。
3. `cmake --preset <preset>`，随后 `cmake --build --preset build-<preset>`。
4. 第二个参数为 `test` 时额外执行 `ctest --preset test-<preset>`。

## 可用的预设

| Configure 预设 | 生成器 | 适用场景 |
| :-- | :-- | :-- |
| `windows-ninja-debug` | Ninja | 日常增量构建（见"已知问题"） |
| `windows-ninja-release` | Ninja | 发布构建与性能测量 |
| `windows-vs2022-debug` | Visual Studio 17 2022 | VS2022 / 完整 IDE 调试 |
| `windows-vs2026-debug` | Visual Studio 18 2026 | VS2026 |

每个 configure 预设都有配对的 `build-<preset>` 与 `test-<preset>` 预设（覆盖 Ninja 与 VS 生成器、Debug 与 Release），因此 `build.bat <preset> test` 对**全部**预设都成立。VS 生成器是多配置的，其 test 预设显式带 `configuration: Debug`。

预设是**唯一**构建描述来源：不在 IDE 内另存配置，不提交 `.vcxproj` / `.sln`（`README.md` §8.1）。
本机特有的路径覆盖请写入 `CMakeUserPresets.json`（已被 `.gitignore` 忽略）。

## 产物位置

```
build/<preset>/bin/          # 可执行文件
build/<preset>/lib/          # 静态库
```

`build/` 整体被忽略，禁止提交任何构建产物（`README.md` §8.4、目录红线 D6）。

## 常用选项

| 选项 | 默认 | 说明 |
| :-- | :-- | :-- |
| `ATRIUM_BUILD_TESTS` | `ON` | 是否构建单元 / 契约测试（`README.md` §9.1） |

```bat
cmake --preset windows-vs2026-debug -DATRIUM_BUILD_TESTS=OFF
```

## 手动构建（不使用 build.bat）

```powershell
# Visual Studio 生成器不需要 vcvars
cmake --preset windows-vs2026-debug
cmake --build --preset build-windows-vs2026-debug
ctest --test-dir build/windows-vs2026-debug -C Debug --output-on-failure
```

## 已知问题

### Windows 宏名冲突会静默改写函数名

**现象**：`common/` 中一个名为 `GetEnvironmentVariable` 的函数在 MSVC 下**永远链接不上**：

```
error LNK2019: 无法解析的外部符号 "...atrium::common::GetEnvironmentVariable(...)"
```

即使源码中该函数确实存在、也确实编译进了 `.lib`，链接依然失败。

**原因**：`<windows.h>` 把 `GetEnvironmentVariable` 定义为宏（展开为 `GetEnvironmentVariableA` / `GetEnvironmentVariableW`）。任何同名函数在预处理阶段就被改名，于是：

- 定义侧变成 `GetEnvironmentVariableW`（存在于 `Paths.obj` 中）；
- 声明侧若未包含 `<windows.h>`，仍是 `GetEnvironmentVariable`；
- 两侧符号不一致 ⇒ 链接期找不到符号。

用 `dumpbin /SYMBOLS` 能看到这个错位：`.obj` 里只有 `?GetEnvironmentVariableA@common@atrium@@...`。

**规避**：避免使用与 Win32 API 同名的标识符。已知同类危险名（含 `windows.h` 宏或 API）：

| 危险名 | 说明 |
| :-- | :-- |
| `GetEnvironmentVariable` | 宏 → `...A` / `...W` |
| `GetCurrentDirectory` | 宏 |
| `GetTempPath` / `GetTempFileName` | 宏 |
| `CreateFile` / `DeleteFile` | 宏 |
| `FindFirstFile` / `FindNextFile` | 宏 |
| `GetObject` / `LoadImage` / `DrawText` | 宏 |
| `min` / `max` | 宏（应使用 `std::min` / `std::max`，或定义 `NOMINMAX`） |

**当前处置**：该函数已改名为 `GetEnvironmentValue`，并在 `common/include/atrium/common/paths/Paths.hpp` 中留了说明注释，便于通过 grep 追溯到原因。

**建议**：在包含 `<windows.h>` 的翻译单元中优先定义 `WIN32_LEAN_AND_MEAN` 与 `NOMINMAX`，可减少一类冲突；但**不要**依赖 `#undef` 逐个规避——正确做法是公共接口不使用 Win32 风格的名字。

### `.bat` / `.cmd` 脚本必须 ASCII-only

**现象**：`build.bat` 中带中文的 `rem` 注释会让 cmd 报出一串莫名其妙的错误，而报错内容正是注释片段的乱码：

```
'�机路径；Visual' is not recognized as an internal or external command
'or' is not recognized as an internal or external command
'| was unexpected at this time.
```

**原因**：`cmd.exe` 按**当前控制台代码页**逐字节解析 `.bat`，而不是按文件自身的编码。UTF-8 的中文字节序列在别的代码页下被错切，切出的字节会落进 ASCII 元字符区间（`|`、`&`、`<`、`>`、`(`），于是 cmd 把一条注释**当成了带管道/重定向的命令行**——上面那条 `| was unexpected at this time` 就是这么来的。

**规则**：**所有 `.bat` / `.cmd` 一律 ASCII-only，并且不写注释**——说明性内容写进本文件，不写进脚本。`build.bat` 已按此处理（全文无注释）。中文不是"观感问题"，是会让构建直接失败的问题。

**注意**：从 PowerShell 调用时是否触发取决于调用形式（`& .\build.bat ...` 与 `cmd /c "build.bat ..."` 的代码页处理不同），所以"我这边能跑"**不能**作为放行依据。

### Ninja 预设不退出（只出现在自动化 / Agent 会话中）

**现象**：`windows-ninja-*` 预设 configure 阶段停在 `-- Detecting CXX compiler ABI info` 之后长时间无输出；直接运行 `ninja -f build.ninja` 会**正确完成构建**（产物已生成），但进程不退出。

**已确认的事实**：

- 与工具链无关：Visual Studio 自带的 `cmake` + `ninja 1.13.2` 与 PATH 上的 `cmake 4.2.1` + `ninja 1.12.1` 表现一致。
- 与本仓库的 CMake 无关：只含 `project(probe LANGUAGES CXX)` 与 `add_executable` 的最小工程同样复现。
- **在普通交互式终端中 Ninja 预设工作正常**（2026-09-18 人工确认）。该现象**只出现在自动化 / Agent 会话**中，属会话环境的进程退出问题，不是工程缺陷。

**应对**：

| 场景 | 做法 |
| :-- | :-- |
| 日常开发（交互式终端） | 用 `windows-ninja-*`，增量更快 |
| 自动化 / Agent 会话 | 用 `windows-vs2026-debug` 预设，并**直接调用 `cmake` / `ctest`**，不经 `build.bat` |

后者可直接用：

```powershell
cmake --preset windows-vs2026-debug
cmake --build --preset build-windows-vs2026-debug
ctest --preset test-windows-vs2026-debug
```

## 验证要求

任何改动提交前必须完成（`README.md` §9.2、`AGENTS.md`）：

1. 干净构建通过（非增量）。
2. 契约测试通过。
3. 涉及工具进程时，手动验证 `启动 → 就绪 → shutdown → 退出码 0`。
