# common/ —— 工具间共享
#
# 准入条件（README §2.3）：新代码进入本目录前必须回答"它现在有几个消费者？"。
# 少于两个消费者——**接口本身也算**——不得放入 common，应留在该工具内部。
#
# 禁止项（README 目录红线 D2 / D8、红线 C15）：
#   - 不得依赖 tools/ 下的任何代码（依赖方向 tools → common 单向）
#   - 不得包含 UI 框架依赖与具体工具业务逻辑
#   - 不得引入第三方实现与资源（libav*.h / pdfium.h / 图渲染后端 / mermaid.min.js 等）
#
# v2.0 修订记录：已移除 media/（色彩标记 POD + 视频/PDF 窄接口）与 block/（块模型）。
#   二者消费者均为零；且 block/BlockTypes.hpp 的头部注释写死了当时"块为唯一真源"的决策，
#   而该决策已被推翻（README §12.1.2：Markdown 文件为唯一真源，块只是派生缓存）。
#   详见 README §2.3 修订记录。

add_library(atrium_common STATIC
    src/log/Log.cpp
    src/paths/Paths.cpp
)

add_library(Atrium::Common ALIAS atrium_common)

target_include_directories(atrium_common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(atrium_common PUBLIC cxx_std_20)

# 依赖方向守护：common 不得反向依赖 tools。
# 实现方式：在构建期断言 tools/ 的 target 不存在于本 target 的链接接口。
# 完整静态检查（源码级 grep）由 Tools/ci 落地，见 README §1.2 与 §9.1。
if(MSVC)
    target_compile_options(atrium_common PRIVATE /W4 /permissive-)
else()
    target_compile_options(atrium_common PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- 契约测试（README §9.1：契约测试为必需层级）--------------------------------
if(ATRIUM_BUILD_TESTS)
    add_executable(atrium_common_contract_tests tests/ContractTests.cpp)
    target_link_libraries(atrium_common_contract_tests PRIVATE Atrium::Common)

    add_test(NAME atrium_common.contract COMMAND atrium_common_contract_tests)
endif()
