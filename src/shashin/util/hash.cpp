#include <shashin/util/hash.h>
#include <shashin/util/string.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <regex>
#include <algorithm>
#include <iomanip>
#include <tuple>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <random>
#include <city/City.h>

namespace shashin {
namespace util {

auto string_to_hash(std::string const& str) -> uint64_t {
    return CityHash64(str.c_str(), str.size());
}

auto hash_to_hex_string(uint64_t const& hash) -> std::string const {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2 * sizeof(hash)) << std::hex << hash;
    return ss.str();
}

auto create_salt(fs::path const& path) -> std::string {
    if (fs::exists(path) && fs::is_regular_file(path)) {
        std::ifstream ifs{path};
        std::string salt;
        std::getline(ifs, salt);
        ifs.close();
        util::stackoverflow::trim(salt);
        if (salt.size() > 0) {
            return salt;
        }
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, std::numeric_limits<int>::max());
    std::stringstream ss;
    ss << std::uppercase << std::setfill('0') << std::hex << dist(rng) << dist(rng);
    std::ofstream ofs{path};
    ofs << ss.str();
    ofs.close();
    return ss.str();
}

} // namespace util
} // namespace shashin
