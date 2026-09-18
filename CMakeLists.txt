cmake_minimum_required(VERSION 3.25)

# AtriumCppTools —— 从零开始的最小起点。
# 这一版只有一个目标：把「窗口」立起来。目录与分层在架构定下来之后再收。
project(AtriumCppTools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 的 moc/uic/rcc 自动处理，省掉手写 qt_wrap_* 的麻烦。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(loomery
    src/main.cpp
    src/app/AppWindow.h
    src/app/AppWindow.cpp
)

target_include_directories(loomery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(loomery PRIVATE Qt6::Widgets)
