# webview2-probe —— WebView2 宿主成本探针

> **这是一次性探针，不属于 Loomery 产品代码。** 它的唯一目的是关掉架构文档里的
> **风险 R1 / 未决项 P1**：WebView2 宿主的冷启动、常驻内存、以及退出后是否残留
> `msedgewebview2.exe` 孤儿进程。**在该实测通过前，Loomery 的架构不视为定案。**

本目录是一个**独立的 CMake 工程**，不参与仓库主构建，也不触碰 `tools/CMakeLists.txt`
（避免架构文档未决项 P2 的归属争议）。

---

## 为什么必须做这个实测

| 已实测（README §12.1.3） | 未实测（本探针要解决） |
| :-- | :-- |
| Mermaid 热态渲染 12.9–29.9 ms/图 | WebView2 environment 创建耗时 |
| Markdown 解析 21.6 ms / 排版 8.2 ms（2000 块） | 整棵 Chromium 子进程树的常驻内存 |
| `mermaid.min.js` 体积 3.49 MB | **被强杀后是否留下 `msedgewebview2.exe` 孤儿** |

第三项最关键，也最容易漏：**正常关闭不留孤儿 ≠ 被强杀不留孤儿**。而 `Steward` 在超时后
会 `terminate` 再 `kill`（仓库规范 §5.7 第 3 条），所以工具必须扛得住强杀。如果扛不住，
就需要给 Chromium 子进程加 Job Object（`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`）——探针的
`--job-object` 开关就是为了同时回答"有没有问题"和"这个修法管不管用"。

---

## 怎么跑（三步）

```powershell
cd D:\Repositories\Manager\AtriumCppTools\tools\loomery\tests\spike\webview2-probe

# 1) 下载并解包 WebView2 SDK（约 8.8 MB，只需一次）
powershell -ExecutionPolicy Bypass -File .\prepare-sdk.ps1

# 2) 编译
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release

# 3) 跑全部场景并输出汇总表（约 2–4 分钟）
powershell -ExecutionPolicy Bypass -File .\run-probe.ps1
```

> **如果你沿用的是当前这份工作副本，第 1、2 步已经做完了**：`.sdk/1.0.4191.47/` 与
> `build/bin/webview2_probe.exe` 都已就位（两者都被 `.gitignore` 忽略，不会进仓库）。
> 那么你只需要跑第 3 步。`prepare-sdk.ps1` 会检测到已解包的 SDK 并直接跳过。

可选参数：

| 参数 | 作用 |
| :-- | :-- |
| `-DiagramCount N` | 页面内渲染 N 张 Mermaid 图（默认 3）。若把 `mermaid.min.js` 放到 `page/` 目录，就会连图一起量 |
| `-NoShow` | 不显示窗口（**会低估内存**，仅供快速试跑） |
| `-SkipJobObjectScenarios` | 跳过 Job Object 对照场景 |
| `-KillAfterReadyMs N` | 外部强杀场景里，观察到 Chromium 子进程后再等多久开杀 |
| `-ProbeTimeoutMs N` | 单场景等待导航完成的超时（默认 30000） |
| `-UseNavigateToString` | 改用 `NavigateToString` 注入页面，**绕开 `file://` URI 解析**。纯诊断开关 |

> 跑之前请**关掉自己的 Edge 窗口**能减少干扰，但不是必须——探针用 PID 差分识别"自己制造的"
> Chromium 进程，不会把你自己已运行的算进去。
>
> **每个场景使用独立的 profile 目录**（只有 `clean-cold` / `clean-warm` 这一对故意共用，
> 因为要测冷热对比）。共用 profile 会让上一场景仍在运行的 Chromium 子进程占住目录，
> 表现为随机的 controller 创建失败与卡死——这个坑已经踩过一次。

---

## 如果结果仍然是 INCONCLUSIVE

脚本会把无效场景**排除**在孤儿判定之外，并逐条打印诊断。对照下表读：

| 诊断 | 含义 | 下一步 |
| :-- | :-- | :-- |
| `timeout before controller creation completed (stage: environment created at N ms)` | environment 建成了，但 controller 一直没回来 | 最常见的失败。先确认没有任何 Edge/WebView2 实例在跑；再看 `process_failed_kind` |
| `timeout waiting for first NavigationCompleted (... NavigationStarting seen=...)` | controller 建成了。看 `seen=` | `false` = **导航根本没发起**（`Navigate` 的 HRESULT 会一并打印）；`true` = 发起了但浏览器没回 |
| `CreateCoreWebView2Controller failed` + `hresult` | controller 创建被拒 | `0x80070005` = 拒绝访问；`0x80004004` = `E_ABORT` |
| `Navigate failed, hr=...` | URI 被拒 | 检查 `resolved_page_url` 是否是合法的 `file:///` |
| `procFailed: kind=N` | 渲染进程崩溃/被杀 | kind：`0`=浏览器进程, `1`=渲染进程, `2`=GPU, `3`=Utility, `5`=未知；详情看 `process_failed_description` |
| `get_CoreWebView2 hr != 0` | 拿不到 webview 对象 | 基本等同于 controller 无效 |

**兜底**：如果 `file://` 这条路径始终不通，加 `-UseNavigateToString` 重跑一次。
它能区分"WebView2 装坏了"和"只是 `file://` URI 处理有问题"——前者怎么都不行，后者换这条路径就能出数。

---

## 输出怎么读

`run-probe.ps1` 最后打印一张汇总表，同时在 `out/` 下留每个场景的 JSON 原始报告。

| 列 | 含义 |
| :-- | :-- |
| `env_ms` | 进程入口 → **environment 创建完成** |
| `ctrl_ms` | → **controller 创建完成** |
| `nav_ms` | → **首次 NavigationCompleted**（页面加载完成）。**这一列才是"冷启动到可用"的主体** |
| `self_mb` | 宿主进程自身工作集 |
| `children_mb` | 整棵 `msedgewebview2.exe` **子孙**进程工作集之和（Chromium 是多进程，只看宿主会严重低估） |
| `total_mb` | `self_mb + children_mb` |
| `child_procs` | 子孙进程个数 |
| `orphans_now` | 宿主退出并静置后，新出现的 `msedgewebview2.exe` 数量 |
| `orphans_after3s` | 再等 3 秒后的数量。**只要不为 0，就是孤儿** |

**六个场景**：

| 场景 | 问的问题 |
| :-- | :-- |
| `clean-cold` | 全新 profile 下的冷启动成本 |
| `clean-warm` | 复用 profile 下的启动成本（日常情况） |
| `hard-selfkill` | 宿主自己 `TerminateProcess`（不执行任何清理）后是否留孤儿 |
| `kill-external` | **被外部强杀**（模拟 Steward 的 terminate+kill）后是否留孤儿 |
| `clean-jobobject` | 加了 Job Object 后的正常关闭 |
| `kill-external-job` | **加了 Job Object 后被强杀** |

---

## 判定标准

| 检查项 | 通过与否 |
| :-- | :-- |
| 冷启动 | `nav_ms` + 页面内 Mermaid 首次渲染 **≤ 1500 ms**（README §12.1.1）。建议留一倍余量，即目标 ≤ 750 ms |
| 常驻内存 | `total_mb` **≤ 400 MB**（同上） |
| 孤儿进程 | **所有场景**的 `orphans_after3s` 都为 **0** |
| Job Object | 若裸跑有孤儿、加 Job Object 后为 0，则结论是"必须用 Job Object"，并据此写进架构文档的 shutdown 流程 |

**无论结果如何，都要把结论回填到 `Docs/ARCHITECTURE_DESIGN.md`**：通过则关掉 R1/P1 并把
性能预算表里的 ⚠ 去掉；不通过则重新评估文档渲染面技术栈（退回原生 + PNG 位图路线）。

---

## 已知限制

1. **页面加载完成的定义**：`nav_ms` 对应 `NavigationCompleted`。页面用**阻塞式** `<script src="mermaid.min.js">`，
   所以 mermaid 的下载与解析会计入该时间；但 `mermaid.render()` 是异步的，**图渲染完成的时间不包含在内**。
   若需要"图也画完"的总时间，把 `mermaid.min.js` 放进 `page/` 并配合 `-DiagramCount` 观察内存与主观耗时。
2. **内存是工作集（Working Set）**，不是私有提交量。不同工具口径不同，不要与任务管理器之外的数字混比。
3. **仅 Windows x64**。命名管道与 WebView2 均为 Windows 专有。
4. 本探针**不测帧率**。滚动/合成帧率需要真实交互，探针只能给内存与启动。

---

## 已在 DSH 沙箱中观察到的情况（**部分数据，不可当作结论**）

写这个探针时在自动化沙箱里跑过多次。**controller 创建始终失败**
（`E_ABORT` / `0x8007006A`），所以**没有拿到有效的完整数据**——脚本会明确输出
`INCONCLUSIVE` 并把无效场景排除，不会给出误导性的孤儿结论。

失败运行仍然暴露了几个有用的事实：

| 观察 | 值 | 可信度 |
| :-- | :-- | :-- |
| WebView2 运行时版本 | `153.0.4234.32` | 可信 |
| **environment 创建耗时** | **22 ms**（热）/ **158 ms**（冷）/ **331 ms**（全新 profile + 可见窗口） | 可信 |
| 观察到的单个 Chromium 子进程工作集 | 19 – 79 MB | **仅供参考**——controller 未建成，进程树不完整，不是常驻内存 |
| 宿主自身工作集 | 10 – 20 MB | 可信（与本机无关，探针本身很小） |
| controller 创建 | 失败（`E_ABORT`） | 沙箱限制，非工程缺陷 |

**能得出的唯一结论**：`environment` 创建本身**不是**启动预算的瓶颈（几十到几百毫秒）。
真正的大头在 controller 创建与首次导航——**这部分仍未测**，正是需要你在正常会话里跑的原因。

顺带一提，探针在沙箱里还替我们踩掉了三个真实的坑，都已在代码与脚本中修掉：

1. **`Start-Process -Wait` 等待的是整棵进程树**——而 WebView2 的 Chromium 子进程正好是可能
   比宿主活得久的那批，所以脚本会永远等不到返回。已改为轮询宿主进程自身。
2. **`Expand-Archive` 解 8.8 MB 的 nupkg 会卡住**（数分钟不返回），`tar.exe` 只要 **216 ms**。
3. **`$pid` 是 PowerShell 只读自动变量**，不能用作循环变量。

---

## 目录

| 文件 | 说明 |
| :-- | :-- |
| `main.cpp` | 探针主体：分段计时、内存采样、子孙进程枚举、Job Object、三种退出模式 |
| `CMakeLists.txt` | 独立工程；自动在 `.sdk/` 下定位 SDK |
| `prepare-sdk.ps1` | 下载并解包 WebView2 SDK（用 `tar` 而非 `Expand-Archive`，见下） |
| `run-probe.ps1` | 编排六个场景、PID 差分检测孤儿、打印汇总表、清理自己制造的进程 |
| `page/probe.html` | 探针页面；`?blocks=` 控制内容规模，`?diagrams=` 控制图数量 |
| `.sdk/`、`build/`、`out/` | 生成物，均已被 `.gitignore` 忽略 |

**两个实现细节值得记下来**（都来自实测踩坑）：

1. **解包用 `tar.exe`，不用 `Expand-Archive`**：同一个 8.8 MB 的 nupkg，`tar -xf` 用
   **216 ms**，`Expand-Archive` 跑了几分钟仍未完成。
2. **`#include <wrl.h>`，不是 `<wrl/client.h>`**：`Microsoft::WRL::Callback` 定义在
   `wrl/implements.h`，只包含 `client.h` 会找不到它。
