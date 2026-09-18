# tools/ —— 具体工具（当前为空）

本目录存放 **长期维护、独立进程运行、由 `AtriumSteward` 托管生命周期** 的 C++ 工具。

当前**尚无工具**。首个立项工具是 `Loomery`，其立项依据、量化预算与定位见仓库根目录 `README.md` §12.1。

## 新增工具前的检查清单

按顺序逐项确认，任一项不满足则不得建目录：

1. 已通过根 `README.md` §11 的准入评估（9 个问题），且结论为"独立 C++ 工具"。
2. 已定名，满足 §4.3 命名规范，且目录名 / 分支名（`tools/<kebab-case>`）/ `tool_id` 三元一致（§4.1）。
3. 已有可测量的性能预算（§8.3），不接受"够快"这类描述。
4. 已明确与 `Steward` 的生命周期契约（§5），或明确声明暂不接入。
5. 确认工具可在**没有** `Steward` 的环境下独立完成**全部**功能（§2.1 第 8 条）。

## 目录要求

```text
tools/<ToolName>/
├── CMakeLists.txt          # 独立 target，禁止 link 其他工具
├── include/<tool>/
├── src/
├── resources/
├── tests/                  # 单元 / 契约 / 生命周期测试（§9.1）
├── benchmarks/
├── Docs/
│   ├── ARCHITECTURE_DESIGN.md   # 必备，模板见 §6.2
│   └── BUILD.md
└── README.md               # 必备，模板见 §6.1
```

## 红线提醒

- 工具之间禁止互相 link（目录红线 D3）；公共代码必须下沉到 `../common/`，但要先满足 §2.3 的准入条件。
- 工具**不得** `#include` 或 link `AtriumSteward` 源码（红线 C1）。
- 工具**不得**依赖 `Projects/` 下的任何项目（红线 C14）。
- 第三方实现与资源（`libav*.h`、`pdfium.h`、图渲染后端、`mermaid.min.js` 等 JS 资源）只允许出现在工具内的具体实现目录或 `third_party/`，**不得进入 `common/`**（红线 C15、目录红线 D8）。
- **不得自行实现 Mermaid 的解析 / 布局 / 绘制**，必须调用现成实现（红线 C17、README §2.2.2）。
