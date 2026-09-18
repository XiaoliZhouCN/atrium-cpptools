# tools/ —— 具体工具

未来这个仓库会放**多个**工具，每个工具一个独立目录、一个独立 target。

| 工具 | 目录 | 分支 | 状态 |
| :-- | :-- | :-- | :-- |
| Loomery | `tools/loomery/` | `tool/loomery` | 骨架（空窗口） |

## 新增工具

按顺序确认，任一项不满足则不得建目录：

1. 确认它确实是独立工具，而不是脚本或某个已有工具的一部分。
2. 定名：目录 `tools/<name>`，**分支 `tool/<kebab-case>`**，两者必须对应。
3. 目录自包含：删除它之后其余工具仍能独立构建。
4. 已有可测量的性能预算，不接受"够快"这类描述。
5. 具备 `README.md` 与 `Docs/ARCHITECTURE_DESIGN.md`。

注册是**自动**的（`../CMakeLists.txt` 遍历直属子目录），不需要改任何框架文件。

## 目录要求

```text
tools/<name>/
├── CMakeLists.txt          # 独立 target，禁止 link 其他工具
├── README.md
├── Docs/
│   ├── ARCHITECTURE_DESIGN.md
│   └── tool.manifest.json   # 接入宿主时提供
├── src/
├── resources/
└── tests/
```

## 注意：分支名是 `tool/<name>` 而不是 `tools/<name>`

git 无法同时存在 `refs/heads/tools` 与 `refs/heads/tools/<x>`——前者是文件、后者要求它是目录。实测报错：

```
fatal: cannot lock ref 'refs/heads/tools/loomery': 'refs/heads/tools' exists
```

所以目录叫 `tools/loomery`、分支叫 `tool/loomery`。这个不一致是 git 的约束，不是命名疏漏。
