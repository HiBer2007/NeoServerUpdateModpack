#pragma once

#include <string>

namespace HiBerCTM {

void InstallCrashHandler(const std::string& dumpDir = "");
void InstallCrtReportHook();
void SetCrashAppName(const std::string& name);
void SetCrashHelpText(const std::string& text);
void AddCrashTypeInfo(const std::string& name, const std::string& description);
void SetCrashCliMode(bool enabled);

} // namespace HiBerCTM
