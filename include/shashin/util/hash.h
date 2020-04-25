#pragma once

#include <shashin/util/filesystem.h>
#include <string>

namespace shashin {
namespace util {

auto string_to_hash(std::string const& str) -> uint64_t;
auto hash_to_hex_string(const uint64_t& hash) -> std::string const;
auto create_salt(fs::path const& path) -> std::string;

} // namespace util
} // namespace shashin
