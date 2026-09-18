// webview2-probe —— WebView2 宿主成本探针
//
// 目的：为 Loomery 的架构决策（README 12.1.4 / 风险 R1）取得三个数字：
//   1. 冷启动分段耗时（进程入口 → environment → controller → 首次导航完成）
//   2. 常驻内存（宿主自身 + 整棵 msedgewebview2.exe 子孙进程）
//   3. 退出后是否残留 msedgewebview2.exe 孤儿（正常释放 / 被强杀 / 加 Job Object 三种情形）
//
// 这是一次性探针，不属于 Loomery 产品代码，也不链接 common/。
//
// 用法见同级 README.md。

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

// <wrl.h>（而非 <wrl/client.h>）才会带入 Microsoft::WRL::Callback ——
// Callback 定义在 wrl/implements.h 中，只包含 client.h 会找不到它。
#include <wrl.h>
#include <WebView2.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace
{

// ---------------------------------------------------------------- 计时

LARGE_INTEGER g_freq{};
LARGE_INTEGER g_t0{};

void StartClock()
{
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_t0);
}

double SinceStartMs()
{
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart - g_t0.QuadPart) * 1000.0 /
           static_cast<double>(g_freq.QuadPart);
}

// ---------------------------------------------------------------- 选项

struct Options
{
    std::string mode = "clean"; // clean | hard
    std::string reportPath;
    std::string userDataDir;
    std::string pagePath;
    bool freshProfile = false;
    bool jobObject = false;
    bool show = false;
    long long holdMs = 0;
    long long timeoutMs = 30000;
};

Options g_opt;

// ---------------------------------------------------------------- 内存

struct MemInfo
{
    unsigned long long selfWorkingSet = 0;
    unsigned long long selfPrivate = 0;
    unsigned long long childrenWorkingSet = 0;
    int childProcessCount = 0;
};

std::map<DWORD, DWORD> SnapshotParentMap()
{
    std::map<DWORD, DWORD> parents;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        return parents;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry))
    {
        do
        {
            parents[entry.th32ProcessID] = entry.th32ParentProcessID;
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return parents;
}

// 以 rootPid 为根，广度优先收集全部子孙进程 PID。
std::vector<DWORD> CollectDescendants(DWORD rootPid)
{
    const std::map<DWORD, DWORD> parents = SnapshotParentMap();

    std::vector<DWORD> result;
    std::vector<DWORD> frontier{rootPid};
    while (!frontier.empty())
    {
        std::vector<DWORD> next;
        for (const auto &kv : parents)
        {
            const DWORD pid = kv.first;
            const DWORD ppid = kv.second;
            if (pid == rootPid)
            {
                continue;
            }
            if (std::find(frontier.begin(), frontier.end(), ppid) == frontier.end())
            {
                continue;
            }
            if (std::find(result.begin(), result.end(), pid) != result.end())
            {
                continue;
            }
            result.push_back(pid);
            next.push_back(pid);
        }
        frontier.swap(next);
    }
    return result;
}

unsigned long long WorkingSetOf(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h == nullptr)
    {
        return 0;
    }
    PROCESS_MEMORY_COUNTERS_EX counters{};
    unsigned long long ws = 0;
    if (K32GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                                sizeof(counters)))
    {
        ws = static_cast<unsigned long long>(counters.WorkingSetSize);
    }
    CloseHandle(h);
    return ws;
}

MemInfo SampleMemory()
{
    MemInfo info{};

    PROCESS_MEMORY_COUNTERS_EX self{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(),
                                reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&self), sizeof(self)))
    {
        info.selfWorkingSet = static_cast<unsigned long long>(self.WorkingSetSize);
        info.selfPrivate = static_cast<unsigned long long>(self.PrivateUsage);
    }

    const std::vector<DWORD> descendants = CollectDescendants(GetCurrentProcessId());
    for (DWORD pid : descendants)
    {
        const unsigned long long ws = WorkingSetOf(pid);
        if (ws == 0)
        {
            continue;
        }
        info.childrenWorkingSet += ws;
        ++info.childProcessCount;
    }
    return info;
}

// ---------------------------------------------------------------- Job Object

HANDLE g_job = nullptr;

bool CreateKillOnCloseJob()
{
    g_job = CreateJobObjectW(nullptr, nullptr);
    if (g_job == nullptr)
    {
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &limits,
                                 sizeof(limits)))
    {
        CloseHandle(g_job);
        g_job = nullptr;
        return false;
    }
    return true;
}

// 把当前已有的 Chromium 子孙进程收进 Job，之后新生成的也会继承。
// 返回成功纳入的进程数。
int AssignDescendantsToJob()
{
    if (g_job == nullptr)
    {
        return 0;
    }
    int assigned = 0;
    for (DWORD pid : CollectDescendants(GetCurrentProcessId()))
    {
        HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, pid);
        if (h == nullptr)
        {
            continue;
        }
        if (AssignProcessToJobObject(g_job, h))
        {
            ++assigned;
        }
        CloseHandle(h);
    }
    return assigned;
}

// ---------------------------------------------------------------- 探针状态

struct Report
{
    double msProcessEntry = 0.0;
    double msEnvironmentCreated = 0.0;
    double msControllerCreated = 0.0;
    double msWindowCreated = 0.0;
    double msNavigationCompleted = 0.0;
    double msTotalToExit = 0.0;

    MemInfo memAtEnvironment{};
    MemInfo memAtNavigation{};
    MemInfo memAtExit{};

    std::string runtimeVersion;
    std::string error;
    long long hresult = 0;
    int jobAssigned = 0;
    bool jobRequested = false;
    bool jobCreated = false;
    bool navigationFailed = false;
    std::string mode;
    std::string userDataDir;
    long long childPidObserved = 0;
};

Report g_report;

bool g_navigationDone = false;
bool g_fatal = false;

std::string WideToUtf8(const std::wstring &wide)
{
    if (wide.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::string JsonEscape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

std::wstring Utf8ToWide(const std::string &text)
{
    if (text.empty())
    {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::wstring ExecutableDirectory()
{
    wchar_t buffer[MAX_PATH * 4]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, ARRAYSIZE(buffer));
    std::wstring path(buffer, length);
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(0, slash);
}

void PumpMessages(int milliseconds)
{
    const DWORD deadline = GetTickCount() + static_cast<DWORD>(milliseconds);
    for (;;)
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount() >= deadline)
        {
            break;
        }
        Sleep(5);
    }
}

// ---------------------------------------------------------------- 窗口

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

HWND CreateHostWindow()
{
    const wchar_t *className = L"LoomeryWebView2ProbeWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, className, L"Loomery WebView2 Probe",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
                                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (hwnd != nullptr && g_opt.show)
    {
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    return hwnd;
}

} // namespace

// ---------------------------------------------------------------- 入口

int wmain(int argc, wchar_t **argv)
{
    StartClock();

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        auto next = [&](std::string &out) {
            if (i + 1 < argc)
            {
                out = WideToUtf8(argv[++i]);
            }
        };
        if (arg == L"--mode")
        {
            next(g_opt.mode);
        }
        else if (arg == L"--report")
        {
            next(g_opt.reportPath);
        }
        else if (arg == L"--user-data-dir")
        {
            next(g_opt.userDataDir);
        }
        else if (arg == L"--page")
        {
            next(g_opt.pagePath);
        }
        else if (arg == L"--hold-ms")
        {
            std::string value;
            next(value);
            g_opt.holdMs = _strtoi64(value.c_str(), nullptr, 10);
        }
        else if (arg == L"--timeout-ms")
        {
            std::string value;
            next(value);
            g_opt.timeoutMs = _strtoi64(value.c_str(), nullptr, 10);
        }
        else if (arg == L"--fresh-profile")
        {
            g_opt.freshProfile = true;
        }
        else if (arg == L"--job-object")
        {
            g_opt.jobObject = true;
        }
        else if (arg == L"--show")
        {
            g_opt.show = true;
        }
    }

    g_report.mode = g_opt.mode;
    g_report.jobRequested = g_opt.jobObject;

    const std::wstring exeDir = ExecutableDirectory();
    if (g_opt.userDataDir.empty())
    {
        g_opt.userDataDir = WideToUtf8(exeDir + L"\\.wv2profile");
    }
    g_report.userDataDir = g_opt.userDataDir;

    const std::wstring userDataWide = Utf8ToWide(g_opt.userDataDir);
    if (g_opt.freshProfile)
    {
        // 尽力删除；失败不致命（后续 WebView2 会复用已有 profile）。
        std::wstring command = L"cmd.exe /c rmdir /s /q \"" + userDataWide + L"\"";
        _wsystem(command.c_str());
    }

    if (g_opt.jobObject)
    {
        g_report.jobCreated = CreateKillOnCloseJob();
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        g_report.error = "CoInitializeEx failed";
        g_fatal = true;
    }

    g_report.msProcessEntry = SinceStartMs();
    g_report.msWindowCreated = g_report.msProcessEntry;
    HWND hwnd = nullptr;

    if (!g_fatal)
    {
        hwnd = CreateHostWindow();
        if (hwnd == nullptr)
        {
            g_report.error = "CreateWindowExW failed";
            g_fatal = true;
        }
        else
        {
            g_report.msWindowCreated = SinceStartMs();
        }
    }

    // 默认页：探针自带的本地页；找不到则用 about:blank。
    std::wstring pageUrl;
    if (!g_opt.pagePath.empty())
    {
        pageUrl = Utf8ToWide(g_opt.pagePath);
    }
    else
    {
        pageUrl = exeDir + L"\\page\\probe.html";
    }
    {
        const DWORD attributes = GetFileAttributesW(pageUrl.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            pageUrl = L"about:blank";
        }
    }

    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;

    if (!g_fatal)
    {
        const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, userDataWide.c_str(), nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [&](HRESULT result, ICoreWebView2Environment *created) -> HRESULT {
                    g_report.hresult = result;
                    if (FAILED(result) || created == nullptr)
                    {
                        g_report.error = "CreateCoreWebView2EnvironmentWithOptions failed";
                        g_fatal = true;
                        g_navigationDone = true;
                        return S_OK;
                    }
                    environment = created;
                    g_report.msEnvironmentCreated = SinceStartMs();
                    g_report.memAtEnvironment = SampleMemory();

                    if (g_opt.jobObject)
                    {
                        g_report.jobAssigned = AssignDescendantsToJob();
                    }

                    wchar_t *version = nullptr;
                    if (SUCCEEDED(environment->get_BrowserVersionString(&version)) &&
                        version != nullptr)
                    {
                        g_report.runtimeVersion = WideToUtf8(version);
                        CoTaskMemFree(version);
                    }

                    const HRESULT controllerHr = environment->CreateCoreWebView2Controller(
                        hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [&](HRESULT controllerResult,
                                ICoreWebView2Controller *createdController) -> HRESULT {
                                if (FAILED(controllerResult) || createdController == nullptr)
                                {
                                    g_report.hresult = controllerResult;
                                    g_report.error =
                                        "CreateCoreWebView2Controller failed";
                                    g_fatal = true;
                                    g_navigationDone = true;
                                    return S_OK;
                                }
                                controller = createdController;
                                controller->get_CoreWebView2(&webview);
                                g_report.msControllerCreated = SinceStartMs();

                                RECT bounds{0, 0, g_opt.show ? 1024 : 1,
                                            g_opt.show ? 768 : 1};
                                controller->put_Bounds(bounds);
                                if (!g_opt.show)
                                {
                                    controller->put_IsVisible(FALSE);
                                }

                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(webview->get_Settings(&settings)) &&
                                    settings != nullptr)
                                {
                                    settings->put_AreDefaultContextMenusEnabled(FALSE);
                                    settings->put_IsStatusBarEnabled(FALSE);
                                }

                                EventRegistrationToken token{};
                                webview->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [&](ICoreWebView2 *,
                                            ICoreWebView2NavigationCompletedEventArgs *args)
                                            -> HRESULT {
                                            BOOL success = FALSE;
                                            args->get_IsSuccess(&success);
                                            g_report.navigationFailed = (success == FALSE);
                                            if (success == FALSE)
                                            {
                                                COREWEBVIEW2_WEB_ERROR_STATUS status{};
                                                args->get_WebErrorStatus(&status);
                                                g_report.error =
                                                    "navigation failed, status=" +
                                                    std::to_string(static_cast<int>(status));
                                            }
                                            g_report.msNavigationCompleted = SinceStartMs();
                                            g_report.memAtNavigation = SampleMemory();
                                            g_navigationDone = true;
                                            return S_OK;
                                        })
                                        .Get(),
                                    &token);

                                if (g_opt.jobObject)
                                {
                                    g_report.jobAssigned += AssignDescendantsToJob();
                                }

                                webview->Navigate(pageUrl.c_str());
                                return S_OK;
                            })
                            .Get());
                    if (FAILED(controllerHr))
                    {
                        g_report.hresult = controllerHr;
                        g_report.error = "CreateCoreWebView2Controller dispatch failed";
                        g_fatal = true;
                        g_navigationDone = true;
                    }
                    return S_OK;
                })
                .Get());

        if (FAILED(hr))
        {
            g_report.hresult = hr;
            g_report.error = "CreateCoreWebView2EnvironmentWithOptions dispatch failed";
            g_fatal = true;
        }
    }

    // 手动泵消息直到导航完成或超时。
    const DWORD pumpDeadline = GetTickCount() + static_cast<DWORD>(g_opt.timeoutMs);
    while (!g_navigationDone && GetTickCount() < pumpDeadline)
    {
        PumpMessages(20);
    }
    if (!g_navigationDone)
    {
        g_report.error = "timeout waiting for first NavigationCompleted";
    }

    // 稳定后采样一次，让 Chromium 的子进程都起来。
    PumpMessages(1500);
    if (g_opt.jobObject)
    {
        g_report.jobAssigned += AssignDescendantsToJob();
    }
    if (g_report.memAtNavigation.childProcessCount == 0)
    {
        g_report.memAtNavigation = SampleMemory();
    }
    g_report.memAtExit = SampleMemory();

    if (g_opt.holdMs > 0)
    {
        PumpMessages(static_cast<int>(g_opt.holdMs));
    }

    g_report.msTotalToExit = SinceStartMs();

    // ---- 输出报告（无论后续如何退出，报告都已落盘）----
    const std::string json =
        "{\n"
        "  \"mode\": \"" + JsonEscape(g_report.mode) + "\",\n"
        "  \"user_data_dir\": \"" + JsonEscape(g_report.userDataDir) + "\",\n"
        "  \"runtime_version\": \"" + JsonEscape(g_report.runtimeVersion) + "\",\n"
        "  \"error\": \"" + JsonEscape(g_report.error) + "\",\n"
        "  \"hresult\": " + std::to_string(g_report.hresult) + ",\n"
        "  \"navigation_failed\": " + (g_report.navigationFailed ? "true" : "false") + ",\n"
        "  \"job_requested\": " + (g_report.jobRequested ? "true" : "false") + ",\n"
        "  \"job_created\": " + (g_report.jobCreated ? "true" : "false") + ",\n"
        "  \"job_assigned_processes\": " + std::to_string(g_report.jobAssigned) + ",\n"
        "  \"ms_process_entry\": " + std::to_string(g_report.msProcessEntry) + ",\n"
        "  \"ms_window_created\": " + std::to_string(g_report.msWindowCreated) + ",\n"
        "  \"ms_environment_created\": " + std::to_string(g_report.msEnvironmentCreated) + ",\n"
        "  \"ms_controller_created\": " + std::to_string(g_report.msControllerCreated) + ",\n"
        "  \"ms_navigation_completed\": " + std::to_string(g_report.msNavigationCompleted) + ",\n"
        "  \"ms_total_to_exit\": " + std::to_string(g_report.msTotalToExit) + ",\n"
        "  \"self_ws_mb_at_environment\": " +
        std::to_string(g_report.memAtEnvironment.selfWorkingSet / (1024 * 1024)) + ",\n"
        "  \"children_ws_mb_at_environment\": " +
        std::to_string(g_report.memAtEnvironment.childrenWorkingSet / (1024 * 1024)) + ",\n"
        "  \"children_count_at_environment\": " +
        std::to_string(g_report.memAtEnvironment.childProcessCount) + ",\n"
        "  \"self_ws_mb_at_navigation\": " +
        std::to_string(g_report.memAtNavigation.selfWorkingSet / (1024 * 1024)) + ",\n"
        "  \"children_ws_mb_at_navigation\": " +
        std::to_string(g_report.memAtNavigation.childrenWorkingSet / (1024 * 1024)) + ",\n"
        "  \"children_count_at_navigation\": " +
        std::to_string(g_report.memAtNavigation.childProcessCount) + ",\n"
        "  \"self_ws_mb_at_exit\": " +
        std::to_string(g_report.memAtExit.selfWorkingSet / (1024 * 1024)) + ",\n"
        "  \"children_ws_mb_at_exit\": " +
        std::to_string(g_report.memAtExit.childrenWorkingSet / (1024 * 1024)) + ",\n"
        "  \"children_count_at_exit\": " +
        std::to_string(g_report.memAtExit.childProcessCount) + ",\n"
        "  \"host_pid\": " + std::to_string(GetCurrentProcessId()) + "\n"
        "}\n";

    std::fputs(json.c_str(), stdout);
    std::fflush(stdout);

    if (!g_opt.reportPath.empty())
    {
        const std::wstring reportWide = Utf8ToWide(g_opt.reportPath);
        // 报告目录可能不存在（例如 --report out\x.json 而 out\ 尚未创建）。
        // CreateFileW 不会创建父目录，所以这里先补上，否则报告会静默丢失。
        const size_t slash = reportWide.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
        {
            const std::wstring parent = reportWide.substr(0, slash);
            CreateDirectoryW(parent.c_str(), nullptr);
        }
        HANDLE file = CreateFileW(reportWide.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(file, json.data(), static_cast<DWORD>(json.size()), &written, nullptr);
            CloseHandle(file);
        }
        else
        {
            std::fprintf(stderr, "warning: could not write report to %s (GetLastError=%lu)\n",
                         g_opt.reportPath.c_str(), GetLastError());
        }
    }

    // ---- 退出 ----
    if (g_opt.mode == "hard")
    {
        // 模拟宿主超时后的强制回收：不执行任何清理。
        TerminateProcess(GetCurrentProcess(), 1);
        return 1; // 不可达
    }

    // 正常释放顺序：先让页面停止，再释放 controller，最后释放 environment。
    if (webview != nullptr)
    {
        webview->Stop();
    }
    if (controller != nullptr)
    {
        controller->Close();
        controller.Reset();
    }
    webview.Reset();
    environment.Reset();

    // 给 Chromium 子进程一点时间自行退出，然后交还控制权。
    PumpMessages(2000);

    if (g_job != nullptr)
    {
        CloseHandle(g_job);
        g_job = nullptr;
    }
    if (hwnd != nullptr)
    {
        DestroyWindow(hwnd);
    }

    CoUninitialize();

    // 探针本身失败（环境/controller 未建、导航未完成）必须体现为退出码 1，
    // 否则调用方无法从退出码区分"量到了"和"没量到"。
    const bool measured = g_report.error.empty() && !g_report.navigationFailed &&
                          g_report.msNavigationCompleted > 0.0;
    return measured ? 0 : 1;
}
