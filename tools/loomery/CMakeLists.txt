# Loomery —— 工具 target
#
# 注册方式：由 ../CMakeLists.txt 自动发现。
# 本文件属于【工具】，不属于框架。

add_executable(loomery
    src/main.cpp
    src/app/AppWindow.h
    src/app/AppWindow.cpp
)

target_include_directories(loomery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(loomery PRIVATE Qt6::Widgets)

# 渲染面已定用 Qt WebEngine。真正接入文档视图时在这里补：
#   find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)
#   target_link_libraries(loomery PRIVATE Qt6::WebEngineWidgets)
# 现在不提前要求，是为了让"空窗口"这一步不依赖那个大模块。
