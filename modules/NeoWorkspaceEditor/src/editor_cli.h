#pragma once

namespace editor_cli {

/// 是否为 CLI 调用：首个非程序参数是 info / exec / help。
bool is_cli_invocation(int argc, char* argv[]);

/// 运行编辑器 CLI（无 GUI、无 QApplication）。返回进程退出码：
/// 0 = 成功，1 = 操作失败，2 = 参数/用法错误。
int run(int argc, char* argv[]);

} // namespace editor_cli
