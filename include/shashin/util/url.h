#pragma once

#include <string>

namespace shashin {
namespace util {

auto to_slash(std::string const& str) -> std::string const;
auto to_backslash(std::string const& str) -> std::string const;
auto without_leading_slashes(std::string const& str) -> std::string const;
auto string_to_url(std::string const& str) -> std::string const;

} // namespace util
} // namespace shashin
