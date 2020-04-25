#include <shashin/util/string.h>
#include <iomanip>
#include <vector>
#include <regex>
#include <sstream>
#include <fstream>

namespace shashin {
namespace util {

auto dump_to_file(fs::path const& ofile, std::string const& str) -> void {
    std::ofstream ofs{ofile};
    ofs << str;
    ofs.close();
}

auto int_to_string(int value) -> std::string {
    std::stringstream ss;
    ss << std::fixed << value;
    return ss.str();
}

auto double_to_string(double value, int precision) -> std::string {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

auto str_split(const std::string& str, const std::string& delim) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    size_t prev{0};
    size_t pos{0};
    do {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) {
            pos = str.length();
        }
        std::string token{str.substr(prev, pos - prev)};
        if (!token.empty()) {
            tokens.push_back(token);
        }
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

auto make_zero_empty(std::string& input) -> void {
    if (input == "0.0" || input == "0") {
        input = "";
    }
}

auto remove_precision(std::string& input) -> void {
    input = std::regex_replace(input, std::regex("\\.0"), "");
}

namespace stackoverflow {

// https://stackoverflow.com/a/51353040/2777836

auto load_file_binary(std::string const& path) -> std::vector<std::byte> {
    std::ifstream ifs{path, std::ios::binary|std::ios::ate};
    if (!ifs) {
        throw std::runtime_error(path + ": " + std::strerror(errno));
    }
    auto end{ifs.tellg()};
    ifs.seekg(0, std::ios::beg);
    auto size{std::size_t(end - ifs.tellg())};
    if (size == 0) {
        return {};
    }
    std::vector<std::byte> buffer{size};
    if (!ifs.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()))) {
        throw std::runtime_error(path + ": " + std::strerror(errno));
    }
    return buffer;
}

// https://stackoverflow.com/a/217605/2777836

// trim from start (in place)
void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

} // namespace stackoverflow

} // namespace util
} // namespace shashin
