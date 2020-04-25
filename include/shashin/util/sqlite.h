#pragma once

#include <string>
#include <sqlite3.h>

namespace shashin {
namespace util {

auto sqlite3_bind_string(sqlite3_stmt* stmt, int i, std::string const& str) -> void;
auto sqlite3_bind_string_or_null(sqlite3_stmt* stmt, int i, std::string const& str, bool valid = true) -> void;
auto sqlite3_bind_double_or_null(sqlite3_stmt* stmt, int i, double value, bool valid = true) -> void;
auto sqlite3_bind_int_or_null(sqlite3_stmt* stmt, int i, int value, bool valid = true) -> void;

} // namespace util
} // namespace shashin
