#pragma once

#include <shashin/config.h>
#include <shashin/util/filesystem.h>
#include <shashin/util/time.h>
#include <shashin/util/sqlite.h>
#include <string>

namespace shashin {

class Shashin {
public:
    Shashin(fs::path const& project_path = fs::current_path(), std::string const& watermark_text = "");
    ~Shashin();

private:
    Config m_config;
    sqlite3* m_db{nullptr};

    auto open_database() -> void;
    auto close_database() -> void;
    auto exec_query(std::string const& query, int (*callback)(void*, int argc, char**, char**) = nullptr, void* dst = nullptr) const -> void;
    auto exec_transaction(char const* const query, std::function<void(sqlite3_stmt* stmt)> func) const -> void;

    auto create_directories() const -> void;
    auto is_gallery(std::string const& name) const -> bool;
    auto gallery_parts(std::string const& name) const -> std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>;

    auto sync_nodes() const -> void;
    auto sync_images() const -> void;
    auto update_exif() const -> void;
    auto dump_list_html() const -> void;
    auto create_gallery_files() const -> void;
    auto process_images() const -> void;
};

} // namespace shashin
