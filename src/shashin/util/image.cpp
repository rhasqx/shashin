#include <shashin/util/image.h>
#include <shashin/util/string.h>
#include <iostream>
#include <exception>
#include <regex>
#include <easyexif/exif.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/freetype.hpp>

namespace shashin {
namespace util {

auto fix_datetime(std::string& input) -> void;
auto fix_camera_make(std::string& input) -> void;
auto fix_camera_model(std::string& input) -> void;
auto fix_exposure_time(std::string& input) -> void;
auto fix_lens_model(std::string& input) -> void;
auto metering_mode_to_string(int metering_mode) -> std::string;

auto fix_datetime(std::string& input) -> void {
    if (input.size() >= 10) {
        input[4] = '-';
        input[7] = '-';
    }
}

auto fix_camera_make(std::string& input) -> void {
    if (input == "NIKON") {
        input = "Nikon";
    }

    if (input == "NIKON CORPORATION") {
        input = "Nikon Corporation";
    }

    if (input == "Nikon Corporation") {
        input = "Nikon";
    }

    if (input == "FUJIFILM") {
        input = "Fujifilm";
    }
}

auto fix_camera_model(std::string& input) -> void {
    if (input.size() > 5 && input.substr(0, 5) == "NIKON") {
        input = "Nikon" + input.substr(5, std::string::npos);
    }

    if (input == "COOLPIX S5200") {
        input = "Nikon Coolpix S5200";
    }

    if (input == "X20") {
        input = "Fujifilm X20";
    }
}

auto fix_exposure_time(std::string& input) -> void {
    if (input == "1/inf") {
        input = "";
    }
}

auto fix_lens_model(std::string& input) -> void {
    input = std::regex_replace(input, std::regex("\\.0"), "");
    input = std::regex_replace(input, std::regex("(EF(\\-S)?)( ?)(.*)"), "$1 $4");
    input = std::regex_replace(input, std::regex("(.*?)( ?mm)(.*)"), "$1 mm$3");
}

auto metering_mode_to_string(int metering_mode) -> std::string {
    switch (metering_mode) {
        case 0: return "Unknown";
        case 1: return "Average";
        case 2: return "Center-weighted average";
        case 3: return "Spot";
        case 4: return "Multi-spot";
        case 5: return "Multi-segment";
        case 6: return "Partial";
        default: return "Other";
    }
}

auto watermark(cv::Mat& mat, std::string const& text, int fontsize, int margin, int thickness) -> void {
    if (text.size() == 0) {
        return;
    }

    try {
        int baseline{0};
        auto ft2{cv::freetype::createFreeType2()};
        ft2->loadFontData("/Users/mam/Library/Fonts/Roboto-Bold.ttf", 0); // todo
        auto const textsize{ft2->getTextSize(text, fontsize, -1, &baseline)};
        auto const position{cv::Point(mat.cols - textsize.width - margin, mat.rows - margin)};
        ft2->putText(mat, text, position, fontsize, cv::Scalar::all(0), thickness, CV_AA, true);
        ft2->putText(mat, text, position, fontsize, cv::Scalar::all(255), -1, CV_AA, true);
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what()
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
    }
}

auto resize(cv::Mat& src_mat, fs::path const& dst_path, int size, std::string const& text, int fontsize, int margin, int thickness) -> void {
    if (!fs::exists(dst_path.parent_path())) {
        fs::create_directories(dst_path.parent_path());
    }

    try {
        auto const width{(src_mat.cols > src_mat.rows) ? size : int(std::ceil(double(size) * (double(src_mat.cols) / double(src_mat.rows))))};
        auto const height{(src_mat.cols > src_mat.rows) ? int(std::ceil(double(size) * (double(src_mat.rows) / double(src_mat.cols)))) : size};
        std::vector<int> const params{{
            cv::IMWRITE_JPEG_QUALITY, 80,
            cv::IMWRITE_JPEG_PROGRESSIVE, 1,
            cv::IMWRITE_JPEG_OPTIMIZE, 1,
        }};

        cv::Mat dst_mat{cv::imread(dst_path)};
        cv::resize(src_mat, dst_mat, cv::Size(width, height), 0, 0, cv::INTER_AREA);
        watermark(dst_mat, text, fontsize, margin, thickness);
        cv::imwrite(dst_path, dst_mat, params);
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what()
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
    }
}

auto crop(cv::Mat& src_mat, fs::path const& dst_path, int cropped_width, int cropped_height, std::string const& text, int fontsize, int margin, int thickness) -> void {
    if (!fs::exists(dst_path.parent_path())) {
        fs::create_directories(dst_path.parent_path());
    }

    try {
        auto const ratio{std::min(double(src_mat.cols) / double(cropped_width), double(src_mat.rows) / double(cropped_height))};
        auto const width{int(std::ceil(src_mat.cols / ratio))};
        auto const height{int(std::ceil(src_mat.rows / ratio))};
        std::vector<int> const params{{
            cv::IMWRITE_JPEG_QUALITY, 80,
            cv::IMWRITE_JPEG_PROGRESSIVE, 1,
            cv::IMWRITE_JPEG_OPTIMIZE, 1,
        }};

        cv::Mat dst_mat{cv::imread(dst_path)};
        cv::resize(src_mat, dst_mat, cv::Size(width, height), 0, 0, cv::INTER_AREA);
        cv::Rect roi;
        roi.x = std::max(0, int(0.5 * double(width - cropped_width)));
        roi.y = std::max(0, int(0.5 * double(height - cropped_height)));
        roi.width = cropped_width;
        roi.height = cropped_height;
        //std::cerr << roi.x << "," << roi.y << " " << roi.width << "x" << roi.height << " " << width << "x" << height << "\n";
        cv::Mat dst2_mat{dst_mat(roi)};
        watermark(dst2_mat, text, fontsize, margin, thickness);
        cv::imwrite(dst_path, dst2_mat, params);
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what()
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
    }
}

auto exif_info(fs::path const& path) -> std::tuple<bool, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, double, double, double> {
    // https://exiftool.org/TagNames/EXIF.html

    bool exif{false};
    std::string captured_at;
    std::string fstop;
    std::string exposure_time;
    std::string iso_speed;
    std::string exposure_bias;
    std::string flash;
    std::string metering_mode;
    std::string camera_make;
    std::string camera_model;
    std::string lens_make;
    std::string lens_model;
    std::string copyright;
    std::string description;
    std::string software;
    std::string focal_length;
    std::string focal_length_35mm;
    std::string gps;
    double gps_latitude{0};
    double gps_longitude{0};
    double gps_altitude{0};

    auto buffer{util::stackoverflow::load_file_binary(path)};
    easyexif::EXIFInfo exif_info;
    if (exif_info.parseFrom(reinterpret_cast<unsigned char const* const>(buffer.data()), static_cast<unsigned int>(buffer.size())) == 0) {
        exif = true;

        captured_at = exif_info.DateTimeDigitized;
        fstop = double_to_string(exif_info.FNumber, 1);
        exposure_time = "1/" + double_to_string(1.0 / exif_info.ExposureTime);
        iso_speed = int_to_string(exif_info.ISOSpeedRatings);
        exposure_bias = double_to_string(exif_info.ExposureBiasValue, 1);
        flash = exif_info.Flash ? "on" : "off";
        metering_mode = metering_mode_to_string(exif_info.MeteringMode);
        focal_length = double_to_string(exif_info.FocalLength);
        focal_length_35mm = double_to_string(exif_info.FocalLengthIn35mm);
        camera_make = exif_info.Make;
        camera_model = exif_info.Model;
        lens_make = exif_info.LensInfo.Make;
        lens_model = exif_info.LensInfo.Model;
        copyright = exif_info.Copyright;
        description = exif_info.ImageDescription;
        software = exif_info.Software;
        gps = [&exif_info]() -> std::string {
            auto& lat{exif_info.GeoLocation.LatComponents};
            auto& lon{exif_info.GeoLocation.LonComponents};
            std::stringstream ss;
            ss << std::fixed << std::setw(1) << std::setprecision(0) << lat.degrees << "°"
               << std::fixed << std::setw(1) << std::setprecision(0) << lat.minutes << "'"
               << std::fixed << std::setw(1) << std::setprecision(0) << lat.seconds << "\""
               << " " << lat.direction
               << " "
               << std::fixed << std::setw(1) << std::setprecision(0) << lon.degrees << "°"
               << std::fixed << std::setw(1) << std::setprecision(0) << lon.minutes << "'"
               << std::fixed << std::setw(1) << std::setprecision(0) << lon.seconds << "\""
               << " " << lon.direction;
            return ss.str();
        }();
        gps_latitude = exif_info.GeoLocation.Latitude;
        gps_longitude = exif_info.GeoLocation.Longitude;
        gps_altitude = exif_info.GeoLocation.Altitude;

        fix_datetime(captured_at);
        fix_camera_make(camera_make);
        fix_camera_model(camera_model);
        fix_exposure_time(exposure_time);
        fix_lens_model(lens_model);
        make_zero_empty(fstop);
        make_zero_empty(exposure_bias);
        make_zero_empty(iso_speed);
        make_zero_empty(focal_length);
        make_zero_empty(focal_length_35mm);
        remove_precision(fstop);
        stackoverflow::trim(gps);
    }

    return {exif, captured_at, fstop, exposure_time, iso_speed, exposure_bias, flash, metering_mode, camera_make, camera_model, lens_make, lens_model, copyright, description, software, focal_length, focal_length_35mm, gps, gps_latitude, gps_longitude, gps_altitude};
}

} // namespace util
} // namespace shashin
