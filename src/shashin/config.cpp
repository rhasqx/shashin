#include <shashin/config.h>
#include <shashin/util/hash.h>
#include <shashin/util/time.h>

namespace shashin {

static std::mutex mtx;

Config::Config(fs::path const& project_path, std::string const& watermark_text)
    : m_current_time{util::timepoint_to_string(std::chrono::system_clock::now(), "%Y-%m-%d %H:%M:%S")}
    , m_watermark_text{watermark_text}
    , m_project_path{project_path}
    , m_shashin_path{fs::path{project_path}.append(m_shashin_dir)}
    , m_gallery_path{fs::path{project_path}.append(m_gallery_dir)}
    , m_site_path{fs::path{project_path}.append(m_site_dir)}
    , m_data_path{fs::path{project_path}.append(m_data_dir)}
    , m_cache_path{fs::path{m_site_path}.append(m_cache_dir)}
    , m_database_path{fs::path{m_shashin_path}.append(m_database_file)}
    , m_salt_small_path{fs::path{m_shashin_path}.append(m_salt_small_file)}
    , m_salt_medium_path{fs::path{m_shashin_path}.append(m_salt_medium_file)}
    , m_salt_large_path{fs::path{m_shashin_path}.append(m_salt_large_file)}
    , m_salt_small{util::create_salt(m_salt_small_path)}
    , m_salt_medium{util::create_salt(m_salt_medium_path)}
    , m_salt_large{util::create_salt(m_salt_large_path)} {
    // nil
}

Config::~Config() {
    // nil
}

auto Config::current_time() const -> std::string const& {
    return m_current_time;
}

auto Config::gallery_delim() const -> std::string const& {
    return m_gallery_delim;
}

auto Config::watermark_text() const -> std::string const& {
    return m_watermark_text;
}

auto Config::project_path() const -> fs::path const& {
    return m_project_path;
}

auto Config::shashin_path() const -> fs::path const& {
    return m_shashin_path;
}

auto Config::gallery_path() const -> fs::path const& {
    return m_gallery_path;
}

auto Config::site_path() const -> fs::path const& {
    return m_site_path;
}

auto Config::data_path() const -> fs::path const& {
    return m_data_path;
}

auto Config::cache_path() const -> fs::path const& {
    return m_cache_path;
}

auto Config::cache_dir() const -> std::string const& {
    return m_cache_dir;
}

auto Config::database_path() const -> fs::path const& {
    return m_database_path;
}

auto Config::small_width() const -> int {
    return m_small_width;
}

auto Config::small_height() const -> int {
    return m_small_height;
}

auto Config::small_size() const -> int {
    return m_small_size;
}

auto Config::medium_size() const -> int {
    return m_medium_size;
}

auto Config::large_size() const -> int {
    return m_large_size;
}

auto Config::salt_small() const -> std::string const& {
    return m_salt_small;
}

auto Config::salt_medium() const -> std::string const& {
    return m_salt_medium;
}

auto Config::salt_large() const -> std::string const& {
    return m_salt_large;
}

} // namespace shashin
