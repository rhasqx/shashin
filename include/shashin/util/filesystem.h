#pragma once

#include <functional>
#if 0
#include <ghc/filesystem.hpp>
namespace fs {
    using namespace ghc::filesystem;
    using ifstream = ghc::filesystem::ifstream;
    using ofstream = ghc::filesystem::ofstream;
    using fstream = ghc::filesystem::fstream;
} // namespace fs
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

auto list_directory_recursive(fs::path const& base, std::function<bool(fs::path const& path)> const& condition) -> std::vector<fs::path>;
