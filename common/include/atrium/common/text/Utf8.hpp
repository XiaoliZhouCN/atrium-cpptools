// 基础文本类型。
// 规范依据：README §5.4（协议编码统一 UTF-8）、§7.4（所有文本文件统一 UTF-8 无 BOM）。
#pragma once

#include <string>
#include <string_view>

namespace atrium::common
{

// 接口边界上一律使用 UTF-8 文本。
// 说明：若将来出现需要携带语言标记或保证 Unicode 规范化的场景（例如从 Markdown
//       解析出的文本需要区分 NFC / NFD），应替换本别名，而不是继续在各处使用裸 std::string。
using Utf8 = std::string;
using Utf8View = std::string_view;

} // namespace atrium::common
