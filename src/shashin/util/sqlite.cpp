#include <shashin/util/sqlite.h>
#include <cstring>

namespace shashin {
namespace util {

auto sqlite3_bind_string(sqlite3_stmt* stmt, int i, std::string const& str) -> void {
    sqlite3_bind_text(stmt, i, str.c_str(), static_cast<int>(std::strlen(str.c_str())), nullptr);
}

auto sqlite3_bind_string_or_null(sqlite3_stmt* stmt, int i, std::string const& str, bool valid) -> void {
    if (valid) {
        sqlite3_bind_string(stmt, i, str);
    } else {
        sqlite3_bind_null(stmt, i);
    }
}

auto sqlite3_bind_double_or_null(sqlite3_stmt* stmt, int i, double value, bool valid) -> void {
    if (valid) {
        sqlite3_bind_double(stmt, i, value);
    } else {
        sqlite3_bind_null(stmt, i);
    }
}

auto sqlite3_bind_int_or_null(sqlite3_stmt* stmt, int i, int value, bool valid) -> void {
    if (valid) {
        sqlite3_bind_int(stmt, i, value);
    } else {
        sqlite3_bind_null(stmt, i);
    }
}

} // namespace util
} // namespace shashin
