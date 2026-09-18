cmake_minimum_required(VERSION 3.25)

# AtriumCppTools —— Manager 工作区的 C++ 工具仓库。
#
# 本文件只负责工程编排与全局设置，不承载任何工具的业务逻辑。
# 工具在 tools/<name>/ 下自包含，由 tools/CMakeLists.txt 自动发现。

project(AtriumCppTools
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt：moc / rcc / uic 自动处理，省掉手写 qt_wrap_* 的麻烦。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
endif()

# 产物统一输出到 build/<preset>/bin，不散落到源码目录。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

enable_testing()

option(ATRIUM_BUILD_TESTS "Build unit and contract tests" ON)

# Qt 基础模块在顶层统一查找；各工具按需再声明自己用到的子模块
# （例如渲染面接入时会用到 WebEngineWidgets）。
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_subdirectory(tools)
