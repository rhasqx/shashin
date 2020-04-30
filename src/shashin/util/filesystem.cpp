#include <shashin/util/filesystem.h>
#include <vector>

auto list_directory_recursive(fs::path const& base, std::function<bool(fs::path const& path)> const& condition) -> std::vector<fs::path> {
    std::vector<fs::path> paths;
    fs::recursive_directory_iterator it{fs::recursive_directory_iterator{base}};
    std::copy_if(begin(it), end(it), std::back_inserter(paths), condition);
    return paths;
}
