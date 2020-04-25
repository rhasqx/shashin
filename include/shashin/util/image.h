#pragma once

#include <opencv2/core.hpp>
#include <tuple>
#include <string>
#include <shashin/util/filesystem.h>

namespace shashin {
namespace util {

auto watermark(cv::Mat& mat, std::string const& text = "", int fontsize = 32, int margin = 32, int thickness = 4) -> void;
auto resize(cv::Mat& src_mat, fs::path const& dst_path, int size, std::string const& text = "", int fontsize = 32, int margin = 32, int thickness = 4) -> void;
auto crop(cv::Mat& src_mat, fs::path const& dst_path, int cropped_width, int cropped_height, std::string const& text = "", int fontsize = 32, int margin = 32, int thickness = 4) -> void;

auto exif_info(fs::path const& path) -> std::tuple<bool, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, double, double, double>;

} // namespace util
} // namespace shashin
