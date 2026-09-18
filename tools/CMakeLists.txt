# tools/ —— 具体工具
#
# 当前为空。首个工具 Loomery 见 README §12.1。
#
# 每个工具一个独立 CMake target，禁止工具之间互相 link（README 目录红线 D3）。
# 新增工具时在此处 add_subdirectory(<ToolName>)，并确保：
#   1. 目录名、分支名（tools/<kebab-case>）、tool_id 三元一致（README §4.1）
#   2. 工具目录自包含：删除该目录后其余工具仍可独立构建（目录红线 D4）
#   3. 具备 Doc/ARCHITECTURE_DESIGN.md 与 README.md（README §6）
#   4. 声明的性能预算可测量（README §8.3）
