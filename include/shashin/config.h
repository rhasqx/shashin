#pragma once

#include <shashin/util/filesystem.h>
#include <string>

namespace shashin {

class Config {
public:
    Config(fs::path const& project_path = fs::current_path(), std::string const& watermark_text = "");
    ~Config();

    auto current_time() const -> std::string const&;
    auto gallery_delim() const -> std::string const&;
    auto watermark_text() const -> std::string const&;
    auto project_path() const -> fs::path const&;
    auto shashin_path() const -> fs::path const&;
    auto gallery_path() const -> fs::path const&;
    auto site_path() const -> fs::path const&;
    auto data_path() const -> fs::path const&;
    auto cache_path() const -> fs::path const&;
    auto cache_dir() const -> std::string const&;
    auto database_path() const -> fs::path const&;
    auto small_width() const -> int;
    auto small_height() const -> int;
    auto small_size() const -> int;
    auto medium_size() const -> int;
    auto large_size() const -> int;
    auto salt_small() const -> std::string const&;
    auto salt_medium() const -> std::string const&;
    auto salt_large() const -> std::string const&;

private:
    std::string const m_current_time{""};

    std::string const m_shashin_dir{"_shashin"};
    std::string const m_gallery_dir{"_gallery"};
    std::string const m_site_dir{"_site"};
    std::string const m_data_dir{"_data"};
    std::string const m_cache_dir{"cache"};
    std::string const m_database_file{"database.sqlite3"};
    std::string const m_salt_small_file{"salt_small.txt"};
    std::string const m_salt_medium_file{"salt_medium.txt"};
    std::string const m_salt_large_file{"salt_large.txt"};

    std::string const m_gallery_delim{"§"};
    std::string const m_watermark_text{""};

    fs::path const m_project_path;
    fs::path const m_shashin_path;
    fs::path const m_gallery_path;
    fs::path const m_site_path;
    fs::path const m_data_path;
    fs::path const m_cache_path;
    fs::path const m_database_path;
    fs::path const m_salt_small_path;
    fs::path const m_salt_medium_path;
    fs::path const m_salt_large_path;

    int const m_small_width{292};
    int const m_small_height{292};
    int const m_small_size{300};
    int const m_medium_size{800};
    int const m_large_size{1440};

    std::string m_salt_small;
    std::string m_salt_medium;
    std::string m_salt_large;
};

} // namespace shashin
