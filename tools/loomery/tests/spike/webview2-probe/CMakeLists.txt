# webview2-probe —— 独立探针工程
#
# 这是一个【独立】的 CMake 工程，不参与仓库主构建：
#   cmake -S . -B build -G "Visual Studio 18 2026" -A x64
#   cmake --build build --config Release
#
# 这样做的原因：探针是一次性测量工具，不应污染 tools/loomery 的构建树，也避免
# 触碰 tools/CMakeLists.txt 的归属争议（架构文档未决项 P2）。

cmake_minimum_required(VERSION 3.25)

project(webview2_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "webview2-probe only targets Windows.")
endif()

# --- 定位 WebView2 SDK ---------------------------------------------------------
# 优先使用显式传入的 SDK 根目录；否则在 .sdk/ 下自动搜索（由 prepare-sdk.ps1 解包）。
set(WEBVIEW2_SDK_ROOT "" CACHE PATH "WebView2 SDK 解包目录（含 build/native）")

set(_search_roots "")
if(WEBVIEW2_SDK_ROOT)
    list(APPEND _search_roots "${WEBVIEW2_SDK_ROOT}")
endif()
file(GLOB _auto_roots LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/*")
foreach(_root IN LISTS _auto_roots)
    if(IS_DIRECTORY "${_root}")
        list(APPEND _search_roots "${_root}")
    endif()
endforeach()

set(_wv2_include "")
set(_wv2_lib "")

foreach(_root IN LISTS _search_roots)
    # 注意：GLOB_RECURSE 不会把模式中的目录段当作通配。写成
    # "${_root}/x64/WebView2LoaderStatic.lib" 会去找字面路径 <root>/x64/，而 SDK
    # 实际布局是 <root>/extracted/build/native/x64/。因此这里只按文件名递归，
    # 再在结果里挑出 x64（SDK 同时提供 x86 / arm64）。
    file(GLOB_RECURSE _headers "${_root}/WebView2.h")
    file(GLOB_RECURSE _libs "${_root}/WebView2LoaderStatic.lib")

    set(_lib_x64 "")
    foreach(_candidate IN LISTS _libs)
        if(_candidate MATCHES "/x64/")
            set(_lib_x64 "${_candidate}")
            break()
        endif()
    endforeach()

    if(_headers AND _lib_x64)
        list(GET _headers 0 _wv2_include)
        set(_wv2_lib "${_lib_x64}")
        get_filename_component(_wv2_include "${_wv2_include}" DIRECTORY)
        break()
    endif()
endforeach()

if(NOT _wv2_include OR NOT _wv2_lib)
    message(FATAL_ERROR
        "未找到 WebView2 SDK。\n"
        "请先运行：  powershell -ExecutionPolicy Bypass -File prepare-sdk.ps1\n"
        "或显式指定：-DWEBVIEW2_SDK_ROOT=<解包后的 SDK 目录>\n"
        "当前搜索根：${_search_roots}")
endif()

message(STATUS "WebView2 SDK include: ${_wv2_include}")
message(STATUS "WebView2 SDK lib    : ${_wv2_lib}")

# --- 目标 ---------------------------------------------------------------------
add_executable(webview2_probe main.cpp)

target_include_directories(webview2_probe PRIVATE "${_wv2_include}")

target_link_libraries(webview2_probe PRIVATE
    "${_wv2_lib}"
    advapi32
    ole32
    oleaut32
    shell32
    shlwapi
    user32
    version
    psapi
)

if(MSVC)
    # /utf-8：源码含中文注释，必须显式声明编码，否则 MSVC 会按本地代码页误读。
    target_compile_options(webview2_probe PRIVATE /utf-8 /W4 /permissive-)
    target_compile_definitions(webview2_probe PRIVATE UNICODE _UNICODE NOMINMAX
        WIN32_LEAN_AND_MEAN)
endif()

set_target_properties(webview2_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# --- 探针页面随产物一起输出 ----------------------------------------------------
add_custom_command(TARGET webview2_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/page"
            "$<TARGET_FILE_DIR:webview2_probe>/page"
    COMMENT "Copying probe page next to the executable")
