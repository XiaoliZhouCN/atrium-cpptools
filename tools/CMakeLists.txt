# tools/ —— 具体工具
#
# 【本文件属于框架，工具分支不得修改】（README §1.2 第 6 条、红线 C12）
#
# 工具通过**自动发现**注册：含 CMakeLists.txt 的直属子目录即视为一个工具。
# 这样新增工具只需要把自己的目录放进分支，**永远不需要回来改这个文件**；
# 多个工具分支合入 tools 时也不会在这里产生冲突。
#
# 每个工具一个独立 CMake target，禁止工具之间互相 link（目录红线 D3）。
# 工具目录必须自包含：删除任一目录后其余工具仍可独立构建（目录红线 D4）。
# 工具还需具备（README §6）：
#   1. 目录名、分支名（tool/<kebab-case>）、tool_id 三元一致（§4.1）
#   2. Docs/ARCHITECTURE_DESIGN.md 与 README.md
#   3. 可测量的性能预算（§8.3）

file(GLOB atrium_tool_candidates
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*"
)

set(ATRIUM_TOOL_COUNT 0)

foreach(candidate IN LISTS atrium_tool_candidates)
    if(IS_DIRECTORY "${candidate}" AND EXISTS "${candidate}/CMakeLists.txt")
        get_filename_component(tool_name "${candidate}" NAME)
        message(STATUS "AtriumCppTools: adding tool '${tool_name}'")
        add_subdirectory("${candidate}")
        math(EXPR ATRIUM_TOOL_COUNT "${ATRIUM_TOOL_COUNT} + 1")
    endif()
endforeach()

if(ATRIUM_TOOL_COUNT EQUAL 0)
    message(STATUS "AtriumCppTools: no tools found under tools/ (expected on the framework branches)")
endif()
