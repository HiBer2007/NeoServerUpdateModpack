#pragma once

// UTF-8 path helpers (Windows).
//
// The project keeps every std::string path in UTF-8. On Windows,
// std::filesystem::path(std::string) converts narrow strings with the ANSI
// code page, so Chinese paths (UTF-8 bytes) throw inside path construction.
// Always construct paths through these helpers instead.

#include <filesystem>
#include <string>
#include <system_error>

namespace fsu8 {

inline std::filesystem::path path(const std::string& value)
{
    return std::filesystem::u8path(value);
}

inline std::filesystem::path path(const char* value)
{
    return std::filesystem::u8path(std::string(value));
}

inline std::filesystem::path path(const std::filesystem::path& value)
{
    return value;
}

template <class T>
inline bool exists(const T& value)
{
    std::error_code ec;
    return std::filesystem::exists(path(value), ec);
}

template <class T>
inline bool exists(const T& value, std::error_code& ec)
{
    return std::filesystem::exists(path(value), ec);
}

template <class T>
inline bool is_directory(const T& value)
{
    std::error_code ec;
    return std::filesystem::is_directory(path(value), ec);
}

template <class T>
inline bool is_directory(const T& value, std::error_code& ec)
{
    return std::filesystem::is_directory(path(value), ec);
}

} // namespace fsu8
