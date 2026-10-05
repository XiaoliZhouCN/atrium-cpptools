# tools/ —— 具体工具
#
# 【本文件属于框架，工具分支不得修改】
#
# 工具通过**自动发现**注册：含 CMakeLists.txt 的直属子目录即视为一个工具。
# 新增工具只需要把自己的目录放进分支，**不需要回来改这个文件**；
# 多个工具分支合入时也不会在这里产生冲突。
#
# 每个工具一个独立 target，禁止互相 link。
# 工具目录必须自包含：删除任一目录后其余工具仍可独立构建。
#
# 命名：目录 tools/<name>，分支 **tool/<name>**。
# 分支名不是笔误：git 无法同时存在 refs/heads/tools 与 refs/heads/tools/<x>
# （前者是文件、后者要求它是目录），实测报
#   fatal: cannot lock ref 'refs/heads/tools/loomery': 'refs/heads/tools' exists

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
    message(STATUS "AtriumCppTools: no tools found under tools/")
endif()
