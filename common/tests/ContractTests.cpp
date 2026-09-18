// common 库的契约基线测试。
// 依据 README §9.1：契约测试为必需层级。
// 当前使用无依赖的断言式测试；测试框架选型（Catch2 / GoogleTest）见 README §8.2 待定项。
//
// v2.0：随 common/media 与 common/block 的移除，删除了对应的契约测试。
//   它们是"零消费者模块"的测试——测试存在本身不能证明模块该存在（README §2.3）。
#include "atrium/common/error/Error.hpp"
#include "atrium/common/error/Outcome.hpp"
#include "atrium/common/log/Log.hpp"
#include "atrium/common/paths/Paths.hpp"

#include <cstdio>
#include <string>

namespace
{

int g_failures = 0;

void Check(bool condition, const char *expression, int line)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL line %d: %s\n", line, expression);
        ++g_failures;
    }
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

void TestErrorModel()
{
    using namespace atrium::common;

    const Error ok = Ok();
    CHECK(ok.IsOk());
    CHECK(ok.Kind == ErrorKind::None);

    const Error bad = MakeError(ErrorKind::NotFound, "missing", 2);
    CHECK(!bad.IsOk());
    CHECK(bad.Kind == ErrorKind::NotFound);
    CHECK(bad.PlatformCode == 2);
}

void TestOutcome()
{
    using namespace atrium::common;

    Outcome<int> success(42);
    CHECK(success.IsOk());
    CHECK(success.Value() == 42);

    Outcome<int> failure(MakeError(ErrorKind::InvalidArgument, "bad input"));
    CHECK(!failure.IsOk());
    CHECK(!static_cast<bool>(failure));
    CHECK(failure.GetError().Kind == ErrorKind::InvalidArgument);

    Outcome<void> voidSuccess;
    CHECK(voidSuccess.IsOk());

    Outcome<void> voidFailure(MakeError(ErrorKind::IoFailure, "io"));
    CHECK(!voidFailure.IsOk());
}

// 退出码与 README §5.8 必须一一对应；改动此处等同于改协议。
void TestExitCodesMatchSpec()
{
    using namespace atrium::common;

    CHECK(static_cast<int>(ExitCode::Ok) == 0);
    CHECK(static_cast<int>(ExitCode::RuntimeFailure) == 1);
    CHECK(static_cast<int>(ExitCode::PreconditionFailed) == 2);
    CHECK(static_cast<int>(ExitCode::IncompatibleProtocol) == 3);
    CHECK(static_cast<int>(ExitCode::SingletonConflict) == 4);
}

void TestPathsFailExplicitly()
{
    using namespace atrium::common;

    // 空入参必须报错，不得返回空字符串。
    auto resolved = ResolveResourcePath("");
    CHECK(!resolved.IsOk());
    CHECK(resolved.GetError().Kind == ErrorKind::InvalidArgument);

    // 不存在的环境变量必须返回 NotFound，不得静默返回空串。
    auto env = GetEnvironmentValue("ATRIUM_CPPTOOLS_DEFINITELY_NOT_SET_12345");
    CHECK(!env.IsOk());
    CHECK(env.GetError().Kind == ErrorKind::NotFound);

    // 存在的环境变量必须能取到非空值。
    auto path = GetEnvironmentValue("PATH");
    CHECK(path.IsOk());
    CHECK(!path.Value().empty());

    // 可执行文件路径必须可用（EXE 场景）。
    auto executable = GetExecutablePath();
    CHECK(executable.IsOk());
    CHECK(!executable.Value().empty());

    auto directory = GetExecutableDirectory();
    CHECK(directory.IsOk());
    CHECK(!directory.Value().empty());
}

void TestLogSinkRoundTrip()
{
    using namespace atrium::common;

    CHECK(GetLogSink() == nullptr);
    CHECK(SetLogSink(nullptr) == nullptr);
    Log(LogLevel::Info, "contract smoke test: log facade reachable");
}

} // namespace

int main()
{
    TestErrorModel();
    TestOutcome();
    TestExitCodesMatchSpec();
    TestPathsFailExplicitly();
    TestLogSinkRoundTrip();

    if (g_failures != 0)
    {
        std::fprintf(stderr, "%d contract check(s) failed\n", g_failures);
        return 1;
    }

    std::printf("atrium_common contract tests passed\n");
    return 0;
}
