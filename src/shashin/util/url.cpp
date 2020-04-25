#include <shashin/util/url.h>
#include <regex>
#include <vector>
#include <algorithm>

namespace shashin {
namespace util {

auto to_slash(std::string const& str) -> std::string const {
    return std::regex_replace(str, std::regex("\\\\"), "/");
}

auto to_backslash(std::string const& str) -> std::string const {
    return std::regex_replace(str, std::regex("/"), "\\\\");
}

auto without_leading_slashes(std::string const& str) -> std::string const {
    return std::regex_replace(str, std::regex("^\\/"), "");
}

auto string_to_url(std::string const& str) -> std::string const {
    auto url{to_slash(str)};

    // lowercase
    std::transform(url.begin(), url.end(), url.begin(), ::tolower);

    // delete characters
    std::vector<unsigned char> const deletes = {
        '.',
        ',',
        '#',
        '\"',
        '%',
        '=',
        '?',
        '\'',
        '(',
        ')',
        '[',
        ']',
        '{',
        '}',
        '|',
        '~',
        '*'
    };
    for (auto const& ch : deletes) {
        url.erase(std::remove(url.begin(), url.end(), ch), url.end());
    }

    // replace characters
    std::vector<std::pair<std::string const, std::string const>> const replacements = {
        {"ß", "ss"},
        {"æ", "ae"},
        {"é", "e"},
        {"è", "e"},
        {"ê", "e"},
        {"²", "2"},
        {"³", "3"},
        {"ä", "ae"},
        {"ö", "oe"},
        {"ü", "ue"},
    };

    for (auto const& pair : replacements) {
        url = std::regex_replace(url, std::regex(pair.first), pair.second);
    }
    url = std::regex_replace(url, std::regex(" *§.*"), "");
    url = std::regex_replace(url, std::regex("§ *nil *§"), "-");
    url = std::regex_replace(url, std::regex("\\[|\\]|\\(|\\)|\\{|\\}|\\$|§|&| +"), "-");
    url = std::regex_replace(url, std::regex("\\-+"), "-");

    url = std::regex_replace(url, std::regex("\\/"), "-"); // https://github.com/avillafiorita/jekyll-datapage_gen/issues/57

    return url;
}

} // namespace util
} // namespace shashin
