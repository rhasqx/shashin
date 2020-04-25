#include <shashin/util/time.h>
#include <sstream>
#include <fstream>

namespace shashin {
namespace util {

auto make_timestamp() -> std::chrono::time_point<std::chrono::steady_clock> {
    return std::chrono::steady_clock::now();
}

auto timepoint_to_string(std::chrono::system_clock::time_point const& time, std::string const& format) -> std::string const {
    auto const ts{std::chrono::system_clock::to_time_t(time)};
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&ts), format.c_str());
    return ss.str();
};

} // namespace util
} // namespace shashin
