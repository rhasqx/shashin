#pragma once

#include <chrono>
#include <string>

namespace shashin {
namespace util {

auto make_timestamp() -> std::chrono::time_point<std::chrono::steady_clock>;

template<typename T = std::chrono::milliseconds>
auto time_between(std::chrono::time_point<std::chrono::steady_clock> const& start, std::chrono::time_point<std::chrono::steady_clock> const& end) -> long long {
    return std::chrono::duration_cast<T>(end - start).count();
}

auto timepoint_to_string(std::chrono::system_clock::time_point const& time, std::string const& format) -> std::string const;

} // namespace util
} // namespace shashin
