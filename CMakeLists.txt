cmake_minimum_required(VERSION 3.25)

# AtriumCppTools —— Manager 工作区的 C++ 重型工具仓库
# 仓库级规范见 README.md（v1.3）。本文件只负责工程编排，不承载任何工具业务逻辑。
project(AtriumCppTools
    VERSION 0.1.0
    DESCRIPTION "Atrium C++ heavy tools managed by AtriumSteward"
    LANGUAGES CXX
)

# --- 工具链约束（依据 README §7.3）--------------------------------------------
if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR
            "AtriumCppTools on Windows requires the MSVC x64 toolchain. "
            "Please configure with an MSVC generator or run build.bat."
        )
    endif()

    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "AtriumCppTools on Windows requires an x64 toolchain.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive- /W4 /utf-8)
endif()

# 产物统一输出到 build/<preset>/bin，禁止散落到源码目录（README §8.1 / D6）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

enable_testing()

# 测试默认开启（README §9.2：改动前必须完成干净构建 + 契约测试）。
option(ATRIUM_BUILD_TESTS "Build unit and contract tests" ON)

# --- 子目录 -------------------------------------------------------------------
# common/：工具间共享。准入条件见 README §2.3（必须有第二个消费者）。
add_subdirectory(common)

# tools/：具体工具。当前为空占位，见 tools/README.md。
add_subdirectory(tools)
