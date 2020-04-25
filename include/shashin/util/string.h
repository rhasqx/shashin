#pragma once

#include <shashin/util/filesystem.h>
#include <string>
#include <sqlite3.h>

namespace shashin {
namespace util {

auto dump_to_file(fs::path const& ofile, std::string const& str) -> void;
auto int_to_string(int value) -> std::string;
auto double_to_string(double value, int precision = 0) -> std::string;
auto str_split(const std::string& str, const std::string& delim) -> std::vector<std::string>;
auto make_zero_empty(std::string& input) -> void;
auto remove_precision(std::string& input) -> void;

namespace stackoverflow {
auto load_file_binary(std::string const& path) -> std::vector<std::byte>;
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
std::string ltrim_copy(std::string s);
std::string rtrim_copy(std::string s);
std::string trim_copy(std::string s);
} // namespace stackoverflow

} // namespace util
} // namespace shashin
