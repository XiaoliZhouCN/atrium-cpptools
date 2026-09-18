# Loomery 架构设计

> **文档版本**：v0.1
> **修订日期**：2026-09-18
> **状态**：设计已定案，**尚未开工**（无源码）
> **适用范围**：`tools/loomery/` 全部内容
> **上位依据**：仓库根 `README.md` **v2.2**（本文档服从其 §2 设计原则、§5 生命周期契约、§7 接口规范、§10 红线）
> **`common/` 兼容基线**：`0d8c995`（依据 README §1.2 第 4 条；升级基线必须显式 merge/rebase 后更新此值）

---

## 0. 一句话定位

**Loomery 是一个本地 Markdown 图文笔记阅读器**：直接读取磁盘上的 `.md` 文件，内嵌 Mermaid 把笔记里的结构可视化，并提供全库引用图与全文检索。

---

## 1. 职责边界

### 1.1 做什么（MVP 范围，依据 D-15）

| # | 能力 | 说明 |
| :-- | :-- | :-- |
| 1 | 扫描与索引笔记库 | 目标规模 **10,000 个 `.md` 文件**（README §12.1.1 A1） |
| 2 | 渲染单篇笔记 | Markdown 方言见 §4.5（依据 D-14） |
| 3 | 内嵌 Mermaid | 渲染正文中的 ` ```mermaid ` 围栏块 |
| 4 | 全库图视图 | 节点 = 笔记，边 = 笔记间引用；支持缩放与跳转 |
| 5 | 全库全文检索 | 中英文，结果可跳转并高亮 |
| 6 | 被 `Steward` 托管 | 命名管道 + manifest，同时可完全独立运行 |

### 1.2 明确不做什么

| 不做 | 依据 |
| :-- | :-- |
| **不自己实现** Markdown 解析、Mermaid 渲染、数学公式渲染 | README §2.2.2、红线 C17 |
| **不做块编辑器**；应用不拥有内容 | D-11 已推翻"块为唯一真源"；Markdown 文件是唯一真源（§12.1.2） |
| 不实现视频解码与 PDF 渲染 | README §2.2.2 |
| 不做数据库视图、不做单篇内子图 | D-15：不在 MVP |
| **不向笔记库写入任何自有格式** | 索引写在 `Storage/cache/loomery/`（README §5.9），删掉可全量重建 |
| 不让 `Steward` 托管窗口或代管状态 | README §7.2 第 3 条 |
| 不启动未被 manifest 声明的子进程 | README §5.7 第 4 条 |

> **关于第 1 条**：这是本设计里最容易被违反的一条。Mermaid 有 35 种图型，"自己写一个渲染器"看起来像核心竞争力，实际是**依赖**。红线 C17 的存在就是为了挡住这个冲动。

---

## 2. 分层与模块

### 2.1 依赖方向

```text
                    ┌──────────────────────────────────────┐
                    │  page/ （HTML + CSS + JS，页面侧）    │
                    │  渲染 Markdown、Mermaid、KaTeX、图视图 │
                    └───────────────┬──────────────────────┘
                                    │  C++ ↔ JS 桥（§4.4，版本化信封）
                    ┌───────────────▼──────────────────────┐
                    │  shell/   窗口、WebView2 宿主、生命周期 │
                    └───────────────┬──────────────────────┘
        ┌───────────────┬───────────┴────────┬──────────────┐
        ▼               ▼                    ▼              ▼
   ┌─────────┐   ┌────────────┐      ┌──────────┐   ┌───────────┐
   │ bridge/ │   │  index/    │      │ graph/   │   │ contract/ │
   │ 桥的信封 │   │ 扫描+索引+检索│      │ 引用图   │   │ Steward协议│
   └─────────┘   └─────┬──────┘      └────┬─────┘   └─────┬─────┘
                       ▼                  ▼               ▼
                 ┌──────────────────────────────────────────┐
                 │  mdscan/  索引侧 Markdown 解析（抽取型）    │
                 │  workspace/  库根、配置、路径              │
                 └───────────────────┬──────────────────────┘
                                     ▼
                          atrium::common （text/error/log/paths/platform）
```

**依赖是单向的**：`page/` 不依赖 C++ 源码，只通过桥通信；`mdscan/` 不依赖 `index/`；所有模块不得反向依赖 `shell/`。

### 2.2 模块清单

| 模块 | 职责 | 关键约束 |
| :-- | :-- | :-- |
| `shell/` | 进程入口、窗口、WebView2 宿主、生命周期状态机 | **必须在 shutdown 时显式释放 WebView2 environment**（§5.4、Q13） |
| `bridge/` | C++↔JS 消息的信封编解码、消息类型表、版本协商 | 与 README §5.4 同构（D-16）；未知类型必须忽略而非崩溃 |
| `contract/` | Steward 协议的帧编解码、manifest 读取、命令/上报 | 帧 = `uint32 LE len` + UTF-8 JSON，单帧 ≤ 1 MiB（README §5.4.1） |
| `workspace/` | 库根路径、配置合并、索引目录解析 | 禁止猜测工作区路径（README §5.9） |
| `mdscan/` | **索引侧** Markdown 解析：抽取标题、正文纯文本、链接、frontmatter | 只抽取、不渲染；允许保守（宁可多抽不可漏抽） |
| `index/` | 增量扫描、SQLite 索引读写、全文检索 | 索引是**派生物**；删除后可全量重建 |
| `graph/` | 引用图构建、度数统计、布局缓存 | 布局计算在后台线程，不得阻塞 UI |
| `media/` | 预留（A3 第 4 档：视频/PDF） | **MVP 为空**；接口按 §2.2.1 形态预留，实现延后 |

### 2.3 关键设计决策

#### 决策 1：Markdown 解析分两侧（显示侧 JS，索引侧 C++）

D-14 要求支持 `[[wiki 链接]]`、提示块、脚注、数学公式——这些**都不是 CommonMark 标准**。

| 侧 | 用什么 | 为什么 |
| :-- | :-- | :-- |
| **显示** | 页面侧 `markdown-it` + 插件 | 这些扩展都有成熟插件；且 KaTeX 本来就必须在页面侧运行。用 C 解析器（`md4c`）实现同样扩展需要自写扩展逻辑 |
| **索引** | C++ 自研抽取器（`mdscan/`） | 只需要标题 / 纯文本 / 链接 / frontmatter，**不需要渲染保真**；且全库 10,000 文件的扫描不能走 JS |

**两个解析器的分歧风险与缓解**：

1. 特征集以 **D-14 为唯一口径**，两侧都只能按它实现，不得各自扩张。
2. 索引侧**保守抽取**：宁可把 `**粗体**` 的星号也留进检索文本，也不要因为解析不确定而漏掉正文。
3. 用**语料回归测试**兜底（§7）：固定一组含全部特征的样例笔记，断言索引侧抽出的链接集合与显示侧渲染出的 `<a>` 集合一致。
4. 显示侧渲染失败不得影响索引；索引失败不得阻止阅读。

> **这是本设计中唯一"同一件事做两遍"的地方**，因此显式记录理由。若将来发现分歧不可控，退路是改为"C++ 解析成 HTML，页面只做 KaTeX/Mermaid 后处理"。

#### 决策 2：索引用 SQLite + FTS5，不自己写全文索引

自己实现倒排索引 = 重新实现搜索引擎，违反 README §2.2.2 的同一条原则。

- **SQLite 用 amalgamation**（单个 `sqlite3.c`）放入 `third_party/sqlite/<version>/`，便于锁定版本与审计。
- **中文分词是真实难点**：FTS5 的 `unicode61` 分词器不切分中文；`trigram` 分词器要求查询 ≥ 3 字符，而中文最常见的恰是**双字词**（"笔记"、"渲染"）。
- **方案**：实现一个 **FTS5 自定义 tokenizer**——ASCII 走词边界，CJK 按 **bigram** 切分。约 100 行，使用 FTS5 公开的 tokenizer API，属"给现成引擎提供输入"，不是自研引擎。
- **待验证**：该 tokenizer 对 1 字查询（"图"）仍无法命中，需在检索层给出降级策略（标题前缀匹配 + 提示"请输入 2 字以上"）。见 §9。

#### 决策 3：渲染必须按视口懒执行

实测（README §12.1.3）：单张 Mermaid 热态渲染 **12.9–29.9 ms**。一篇含 20 张图的笔记若同步全渲染 = **300–600 ms**，直接击穿"打开单篇 ≤ 150 ms"的预算。

因此：

- **文本优先**：正文排版与首屏可读不等待任何图。
- **图懒渲染**：用 `IntersectionObserver` 只渲染进入视口（含预取余量）的图；渲染任务串行排队，避免抢占主线程。
- **结果缓存**：渲染出的 SVG 按（源码哈希 + 主题）缓存，重复打开同一篇不重渲。
- **失败可见**：单张图渲染失败必须在该图位置显示可读错误，**不得静默留白**（红线：禁止静默失败）。

#### 决策 4：图视图布局在后台预计算并缓存

10,000 节点的力导向布局不可能实时算。方案：

1. 布局结果（节点坐标）随索引一并持久化，带 `layout_version`。
2. 首次进入图视图时，若缓存缺失，先用**确定性廉价布局**（按连通分量分组 + 按度数排序）立即出图，后台再算力导向并渐进替换。
3. 页面只负责绘制与交互（缩放、平移、命中测试），图数据由 C++ 下发。

#### 决策 5：WebView2 的释放是硬性步骤

WebView2 会拉起 `msedgewebview2.exe` 及其子进程。若不在退出前显式释放 environment，`Steward` 关闭本工具后会**残留 Chromium 孤儿进程**——这正是 README §9.3 明令禁止的行为。

- 释放顺序：停止页面活动 → `Close()` → 释放 controller → 释放 environment → 等待子进程退出 → 写 `exit` 上报 → 进程退出。
- 必须有**孤儿检测测试**（§7.4）。见 §9 风险 R1。

---

## 3. 数据模型

### 3.1 唯一真源

**磁盘上的 Markdown 文件本身**（README §12.1.2）。本工具不定义任何内容格式，不写入除索引缓存外的任何文件。

### 3.2 索引（派生物，可随时删除重建）

**位置**：`Storage/cache/loomery/<library-hash>/index.sqlite`

依据 README §5.9「状态类数据只允许写入 `Storage/cache/<tool_id>/`」。`<library-hash>` 为库根绝对路径的短哈希，用于支持多个库并存。

**覆盖优先级**（README §5.9）：`manifest 默认值 < 工作区配置 < 用户配置 < 命令行参数`。

**为什么不放在笔记库内**：写进笔记库会污染内容仓库；且用户可能对笔记库只读挂载。

**表结构（草案）**：

```sql
CREATE TABLE meta (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL          -- schema_version / layout_version / last_full_scan_utc ...
);

CREATE TABLE docs (
  id            INTEGER PRIMARY KEY,
  path          TEXT NOT NULL UNIQUE,   -- 库根相对路径，统一 POSIX 分隔符
  mtime_utc     INTEGER NOT NULL,       -- 增量扫描依据
  size_bytes    INTEGER NOT NULL,
  content_hash  TEXT NOT NULL,          -- 内容未变则跳过重解析
  title         TEXT,                   -- frontmatter.title，缺失则取首个 H1
  frontmatter   TEXT                    -- 原始 YAML 文本
);

CREATE TABLE doc_tags (
  doc_id INTEGER NOT NULL,
  tag    TEXT    NOT NULL,
  PRIMARY KEY (doc_id, tag)
);

CREATE TABLE links (
  from_doc  INTEGER NOT NULL,
  to_doc    INTEGER,                    -- 未解析到目标时为 NULL（悬空引用也要保留）
  to_target TEXT    NOT NULL,           -- 原始目标文本
  kind      TEXT    NOT NULL,           -- wiki | relative | anchor | external
  anchor    TEXT,
  line      INTEGER NOT NULL
);

CREATE TABLE graph_layout (
  doc_id INTEGER PRIMARY KEY,
  x REAL NOT NULL,
  y REAL NOT NULL,
  layout_version INTEGER NOT NULL
);

CREATE VIRTUAL TABLE docs_fts USING fts5(
  title, body,
  content='',
  tokenize='loomery_cjk'             -- 自定义 tokenizer，见 §2.3 决策 2
);
```

**索引重建保证**：删除 `index.sqlite` 后，全量重建必须得到语义等价的结果。这是验收项（§7.3）。

### 3.3 链接解析规则

| 形态 | 识别为 | 解析目标 |
| :-- | :-- | :-- |
| `[[笔记名]]` | `wiki` | 按标题 / 文件名在库内匹配 |
| `[[笔记名\|显示文本]]` | `wiki` | 同上，取 `\|` 左侧 |
| `[文本](相对路径.md)` | `relative` | 相对当前文件解析，越出库根则忽略 |
| `[文本](#锚点)` | `anchor` | 同文件内锚点 |
| `[文本](https://…)` | `external` | 不入图，交给系统浏览器 |
| 路径含 `#锚点` | 按上表分类 + 记录 `anchor` | |

**悬空引用必须保留**（`to_doc = NULL`）：它们在图视图里画成"待创建"节点，是图笔记最有价值的信号之一，不得因为解析不到目标就丢弃。

---

## 4. 外部接口

### 4.1 Steward manifest

**位置**：`Docs/tool.manifest.json`（README §5.2 规定在 `Docs/` 下）

```json
{
  "schema_version": 1,
  "tool_id": "loomery",
  "title": "Loomery",
  "version": "0.1.0",
  "entry": "loomery.exe",
  "args": [],
  "transport": "named-pipe",
  "protocol_version": 1,
  "singleton": true,
  "stability": "experimental",
  "heartbeat_interval_ms": 5000,
  "startup_budget_ms": 1500,
  "requires": ["storage", "markdown-library"],
  "capabilities": ["markdown.read", "mermaid.render", "library.search", "graph.view"],
  "perf_budget": {
    "cold_start_to_ready_ms": 1500,
    "idle_rss_mb": 400
  }
}
```

> **注**：`stability` 由 README §7.5 要求、心跳周期由 §5.6 要求，但 §5.2 的字段表**均未列出**。本设计按需要补上，并已记入 §9 待办（建议 README 补齐 §5.2 表）。

### 4.2 传输与就绪

| 项 | 值 | 依据 |
| :-- | :-- | :-- |
| 管道路径 | `\\.\pipe\atrium.steward.loomery` | README §5.4.2 |
| 角色 | **本工具是服务端**，`Steward` 是客户端 | 同上 |
| 模式 | 字节模式 | 同上 |
| 帧 | `uint32 LE len` + UTF-8 JSON，≤ 1 MiB | README §5.4.1 |
| 就绪顺序 | **先建管道监听，再上报 `ready`** | README §5.4.2 |
| 单实例 | 以"创建同名管道首个实例"抢占；失败则唤醒既有实例后**以退出码 4 退出** | 同上 |

**跨平台声明**（README §5.4.2 要求显式声明）：命名管道为 Windows 专有。macOS 侧需等价实现（`AF_UNIX` 域套接字）。**本工具 MVP 仅交付 Windows x64**，macOS 支持不在本期范围。

### 4.3 CLI

```text
loomery.exe [选项]

  --library <path>     笔记库根目录（缺省从配置/上下文解析）
  --index-dir <path>   索引目录（缺省 Storage/cache/loomery/<hash>）
  --pipe <name>        管道名（缺省 atrium.steward.loomery）
  --no-steward         不接入宿主，独立运行（README §7.1 要求）
  --no-window          无窗口模式，仅建索引（用于脚本与测试）
  --reindex            忽略缓存，全量重建索引
  --version            输出版本后退出（退出码 0）
  --help               输出用法后退出（退出码 0）
```

参数解析失败返回退出码 `2` 并输出可读原因（README §7.1）。

### 4.4 C++ ↔ JS 桥（D-16）

**通道**：WebView2 内置 `PostWebMessageAsJson` / `window.chrome.webview.postMessage`。

**信封**（与 README §5.4 同构，但独立版本号）：

```json
{ "bridge_version": 1, "type": "doc.load", "id": "42", "ts": 1758100000000, "payload": { } }
```

**规则**：

1. 单帧上限 **2 MiB**；超长文档分块下发（`doc.load` 带 `chunk_index` / `chunk_total`）。
2. 未知 `type` 必须忽略并记录，不得崩溃、不得断开（向前兼容）。
3. `bridge_version` 不匹配时，页面拒绝服务并回报 `page.error`，**不得降级静默运行**。
4. 所有消息类型必须登记在下表中；新增类型必须同时更新此表与版本号策略。

**C++ → 页面**：

| `type` | payload | 说明 |
| :-- | :-- | :-- |
| `bridge.hello` | `{capabilities}` | 握手；页面须回 `page.ready` |
| `doc.load` | `{doc_id, path, base_uri, markdown, chunk_index, chunk_total}` | 下发正文 |
| `doc.scroll_to` | `{doc_id, anchor}` | 跳转（来自搜索命中 / 链接） |
| `search.results` | `{query_id, hits:[{doc_id, path, line, snippet}]}` | 检索结果 |
| `search.clear` | `{query_id}` | 清除高亮 |
| `graph.data` | `{nodes:[{id,label,degree}], edges:[{from,to,kind}], layout_version}` | 图视图数据 |
| `theme.set` | `{theme}` | 主题（`light` / `dark`） |

**页面 → C++**：

| `type` | payload | 说明 |
| :-- | :-- | :-- |
| `page.ready` | `{bridge_version, capabilities}` | 页面加载完成 |
| `nav.open` | `{kind, target, anchor}` | 用户点击链接 |
| `nav.external` | `{url}` | 外链，交系统浏览器 |
| `search.query` | `{query_id, text}` | 用户输入检索词（需防抖） |
| `doc.visible` | `{doc_id, anchor, ratio}` | 阅读位置，用于持久化 |
| `page.error` | `{kind, message, doc_id?, block_index?}` | 渲染失败，**必须可见** |

**职责划分**：虚拟化与懒渲染由**页面侧**负责（`IntersectionObserver` / `content-visibility`）；C++ 只在需要时下发跳转与检索指令。这样桥保持窄，不需要逐块驱动渲染。

### 4.5 Markdown 方言（D-14，冻结）

| 特性 | 语法 | 备注 |
| :-- | :-- | :-- |
| CommonMark | 标题 / 列表 / 代码块 / 强调 / 链接 / 引用 | 基线 |
| GFM | 表格、任务列表、删除线、自动链接 | |
| YAML frontmatter | `---` 包裹 | `title` / `tags` 等进入索引 |
| wiki 链接 | `[[笔记名]]`、`[[名\|文本]]` | 图视图的边来源 |
| 脚注 | `[^1]` | |
| 提示块 | `> [!NOTE]` 等 | |
| 数学公式 | `$…$`、`$$…$$` | KaTeX |
| Mermaid | ` ```mermaid ` 围栏块 | 交给 Mermaid 渲染 |

**冻结含义**：两侧解析器都只实现此表。扩张方言必须走 README 变更流程。

### 4.6 退出码

| 码 | 含义 | 本工具的具体触发 |
| :-- | :-- | :-- |
| `0` | 正常退出 | 含响应 `shutdown` |
| `1` | 运行期失败 | 索引损坏且无法重建、WebView2 初始化失败 |
| `2` | 前置条件不满足 | 库根不存在 / 不可读、CLI 参数非法 |
| `3` | 协议或 manifest 不兼容 | `bridge_version` / `protocol_version` 不匹配 |
| `4` | 单实例冲突 | 管道首实例创建失败 |

---

## 5. 生命周期

### 5.1 启动

```text
spawn
 ├─ CLI / 配置解析                     ── 失败 → 退出码 2
 ├─ 解析库根与索引目录                  ── 失败 → 退出码 2
 ├─ 创建命名管道首实例（抢占单实例）    ── 失败 → 唤醒既有实例 → 退出码 4
 ├─ 打开索引（缺失则后台全量重建）
 ├─ 创建窗口 + WebView2 environment    ── 失败 → 退出码 1
 ├─ 页面加载 → 桥握手（bridge.hello / page.ready）
 └─ 上报 ready  ◀── 必须在此之后才开始接受命令
```

**关键顺序**：管道监听必须在 `ready` **之前**建立（README §5.4.2），否则宿主连接会失败。

**启动预算分配**（详见 §8）：WebView2 environment 创建是本路径上最大的不确定项，必须与"打开索引"并行，不得串行等待。

### 5.2 运行

- 心跳周期 5 s（manifest 声明）。
- `context.update` 幂等；收到后更新库根 / 当前文档 / 主题。
- `activate` / `deactivate` 幂等：显示 / 隐藏窗口，不重建 WebView2。
- 后台线程：增量扫描、索引写入、图布局。UI 线程禁止阻塞 IO（README §7.3）。

### 5.3 关闭（`shutdown`）

```text
shutdown
 ├─ 取消后台任务（扫描 / 布局）
 ├─ 落盘索引（提交事务）
 ├─ 页面停止活动 → WebView2 Close() → controller 释放 → environment 释放
 ├─ 等待 msedgewebview2.exe 子进程退出   ★ 必须有超时与失败上报
 ├─ 关闭并释放管道
 ├─ 上报 exit(0)
 └─ 进程退出
```

### 5.4 异常路径

| 场景 | 处理 |
| :-- | :-- |
| WebView2 释放超时 | 上报 `error`；**不得**直接 `TerminateProcess` 自己——那会留下孤儿进程。记录残留 PID 供诊断 |
| 索引损坏 | 删除索引文件并全量重建；重建期间提供只读浏览 |
| 单张 Mermaid 渲染失败 | 页面侧原地显示错误；不影响其他图与正文 |
| 页面崩溃（renderer crash） | 重建 controller 并恢复当前文档；连续失败 3 次则上报 `error` 并提示重启 |
| 上次异常退出残留管道 | 重试连接与创建，超时后返回退出码 4（README §5.4.2） |

---

## 6. 构建与依赖

### 6.1 工具链

| 项 | 值 |
| :-- | :-- |
| 语言标准 | C++20 |
| 编译器 | MSVC x64（主力）；MinGW/Clang 交叉验证 |
| 构建 | CMake ≥ 3.25 + `CMakePresets.json` |
| 预设 | 复用框架预设：`windows-ninja-debug` / `windows-vs2026-debug` |
| CMake target | `loomery`；产物 `build/<preset>/bin/loomery.exe` |

### 6.2 第三方依赖（全部锁定版本，清单见 `docs/DEPENDENCIES.md`）

| 依赖 | 形态 | 用途 | 体积代价 |
| :-- | :-- | :-- | :-- |
| **WebView2 SDK** | NuGet / 头 + 导入库 | 宿主 Chromium | 运行时由系统提供（Win10/11 自带） |
| **SQLite** | amalgamation（`sqlite3.c`） | 索引与全文检索 | ~2.5 MB 源码，运行时 ~1 MB |
| `markdown-it` + 插件 | JS，`third_party/` | 显示侧 Markdown | 待统计（D-14 全套插件） |
| `mermaid.min.js` | JS，`third_party/` | 图渲染 | **3.49 MB**（实测） |
| `katex` + 字体 | JS + woff2，`third_party/` | 数学公式 | 待统计（字体占大头） |
| `highlight.js`（可选） | JS，`third_party/` | 代码高亮 | 待统计 |

**JS 资源管理（D-17）**：提交进仓库 `third_party/<name>/<version>/`，在 `docs/DEPENDENCIES.md` 记录来源 URL 与 **sha256**；构建时复制到产物目录，**不在构建期下载、不在运行期走 CDN**。

> **注意**：`third_party/` 与 `docs/DEPENDENCIES.md` 是**仓库级（框架）**资源，按 README §1.2 第 1 条应在 `develop` 分支上落地，**不在本工具分支上直接添加**。这是本工具开工的第一个前置动作。

### 6.3 资源布局

```text
tools/loomery/
├── CMakeLists.txt
├── README.md
├── Docs/
│   ├── ARCHITECTURE_DESIGN.md     # 本文件
│   ├── BUILD.md
│   └── tool.manifest.json
├── include/loomery/               # 模块间接口
├── src/
│   ├── main.cpp
│   ├── shell/
│   ├── bridge/
│   ├── contract/
│   ├── workspace/
│   ├── mdscan/
│   ├── index/
│   ├── graph/
│   └── media/                     # MVP 为空目录（含 .gitkeep 与说明）
├── resources/
│   └── page/                      # 本工具自有的 HTML / CSS / JS
├── tests/
└── benchmarks/
```

页面侧第三方 JS 不进 `resources/`，而在构建时从仓库根 `third_party/` 复制到产物的 `page/vendor/`。

---

## 7. 测试策略

### 7.1 单元测试
`mdscan/` 的链接与 frontmatter 抽取、`graph/` 的图构建、`bridge/` 的信封编解码、`index/` 的 SQL schema 与查询。

### 7.2 契约测试
- README §5 协议：帧编解码、单帧 1 MiB 上限拒绝、未知 `type` 忽略、`protocol_version` 不匹配返回退出码 3。
- 桥协议：`bridge_version` 不匹配的拒绝路径、未知类型忽略、2 MiB 分块边界。
- 退出码：0/1/2/3/4 全部可达且有测试。

### 7.3 语料回归测试（对应 §2.3 决策 1）
固定一组覆盖 D-14 全部特性的样例笔记（含中英混排、嵌套 wiki 链接、悬空引用、超长文档），断言：
1. 索引侧抽出的链接集合与页面侧渲染出的 `<a>` 集合一致；
2. 删除索引后全量重建，检索结果与重建前语义等价；
3. Mermaid 图全部渲染成功（或明确报错，不得静默留白）。

### 7.4 生命周期与孤儿进程测试（强制）
1. 启动 → `ready` → `shutdown` → 退出码 `0`。
2. **退出后 `msedgewebview2.exe` 残留数必须为 0**（README §9.3 明令禁止的行为）。
3. 宿主强制 kill 后，检查残留进程并归类。
4. 重复启动时单实例冲突返回退出码 4。

### 7.5 人工验收
真实笔记库上的开页手感、图视图可读性、中文检索准确率、10,000 文件全量扫描耗时。

---

## 8. 性能预算

数字来源：README §12.1.1（B 组）。**"未实测"标记的项目不得在实现完成前宣称达标。**

| 指标 | 预算 | 状态 |
| :-- | :-- | :-- |
| 冷启动到 `ready` | **≤ 1500 ms** | ⚠ 含未实测的 WebView2 environment 创建成本（R1） |
| 空闲常驻内存 | **≤ 400 MB** | ⚠ 含未实测的 Chromium 子进程开销 |
| 首屏可用 | **≤ 2000 ms** | |
| 打开单篇（文本可读） | **≤ 150 ms** | 依据实测：Markdown 解析 21.6 ms + 排版 8.2 ms（2000 块） |
| 单图渲染（热态） | **≤ 50 ms** | 依据实测：12.9–29.9 ms |
| 单图渲染（冷态） | **≤ 500 ms** | 含 `mermaid.min.js`（3.49 MB）首次加载 |
| 全库检索（10,000 文件） | **≤ 500 ms** 首次，**≤ 100 ms** 已建索引 | 待 SQLite + 自定义 tokenizer 实测 |
| 全量索引重建（10,000 文件） | **≤ 60 s** | 后台进行，不阻塞阅读 |
| 增量扫描（100 个变更文件） | **≤ 1 s** | |
| 滚动 / 缩放帧率 | **≥ 60 FPS** @ 目标分辨率，2,000 块同屏 | ⚠ **未实测**：只测到 parse + layout，未覆盖 paint / composite |
| 消息延迟（Steward 通道） | **≤ 10 ms** | |
| 桥消息延迟（页内） | **≤ 5 ms** | |
| **退出后 `msedgewebview2.exe` 残留** | **0** | 硬性 |

**A3 演进触发器**：若将来引入第 4 档媒体（视频播放 + 逐页 PDF），启动时间、常驻内存、单帧预算与依赖面**必须重写**（README §12.1.1）。

---

## 9. 风险与未决项

### 9.1 风险

| # | 风险 | 影响 | 缓解 | 状态 |
| :-- | :-- | :-- | :-- | :-- |
| **R1** | **WebView2 宿主成本未实测**：冷启动、常驻内存、以及**释放失败导致孤儿进程** | 可能击穿 ≤1500 ms 与 ≤400 MB；孤儿进程违反 §5 契约与 §9.3 | 先做最小 C++ WebView2 宿主实测（README §12.1.4），**在该实测通过前不得宣称架构定案** | **阻塞中** |
| **R2** | 中文检索：FTS5 分词对 1 字查询无效 | 用户搜"图"无结果 | 标题前缀降级 + UI 提示；评估 bigram 覆盖度 | 待验证 |
| **R3** | 两侧 Markdown 解析器分歧 | 检索结果与所见不一致 | 语料回归测试（§7.3）；特征集冻结（D-14） | 已设计缓解 |
| **R4** | 10,000 节点图视图布局 | 首屏可能极慢 | 廉价初始布局 + 后台渐进优化 + 布局缓存（§2.3 决策 4） | 已设计缓解 |
| **R5** | 页面崩溃恢复 | 阅读中断 | 自动重建 controller；连续失败 3 次上报 | 待实现 |
| **R6** | 前端资源供应链（Mermaid / KaTeX 上游变化） | 图与公式渲染结果漂移 | D-17：提交锁定版本 + sha256；升级走显式流程 | 已定案 |

### 9.2 未决项（不得由实现者自行拍板）

| # | 事项 | 阻塞什么 |
| :-- | :-- | :-- |
| P1 | **WebView2 宿主成本实测**（R1） | 架构正式定案、Q9 关闭 |
| P2 | `tools/CMakeLists.txt` 是否允许工具分支修改 | README §1.2 第 6 条把"顶层 CMake"划为框架，但工具必须在此注册才能构建。**当前归属不明** |
| P3 | README §5.2 字段表补齐 `stability` 与心跳周期字段 | manifest 契约冻结 |
| P4 | `third_party/` 与 `docs/DEPENDENCIES.md` 是否确属框架（按 §1.2 应为是） | 本工具开工的第一个前置动作 |
| P5 | 索引 schema 冻结与迁移策略 | 索引格式一旦发布就要处理升级 |
| P6 | 主题范围（是否支持自定义主题） | 影响桥的 `theme.set` 与 Mermaid 配置面 |

---

## 10. 变更记录

| 版本 | 日期 | 变更 |
| :-- | :-- | :-- |
| v0.1 | 2026-09-18 | 首版：依据 README v2.2、D-14~D-18 与 §12.1.3 实测数据建立设计基线 |
