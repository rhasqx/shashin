#include <shashin/shashin.h>
#include <shashin/util/hash.h>
#include <shashin/util/image.h>
#include <shashin/util/parallel.h>
#include <shashin/util/string.h>
#include <shashin/util/url.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <regex>
#include <algorithm>
#include <iomanip>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <opencv2/highgui/highgui.hpp>

namespace shashin {

static std::mutex mtx;

Shashin::Shashin(fs::path const& project_path, std::string const& watermark_text)
    : m_config{project_path, watermark_text} {
    create_directories();
    open_database();
    exec_query(R"sql(
        CREATE TABLE IF NOT EXISTS nodes (
            id integer PRIMARY KEY AUTOINCREMENT NOT NULL,
            depth integer NOT NULL,
            path text NOT NULL,
            name varchar NOT NULL,
            url varchar NOT NULL,
            hash text NOT NULL,
            captured_at datetime NOT NULL DEFAULT '',
            title varchar NOT NULL DEFAULT '',
            event varchar NOT NULL DEFAULT '',
            location varchar NOT NULL DEFAULT '',
            city varchar NOT NULL DEFAULT '',
            country varchar NOT NULL DEFAULT '',
            created_at datetime NOT NULL,
            updated_at datetime NOT NULL
        );
        CREATE UNIQUE INDEX IF NOT EXISTS nodes_path_idx ON nodes(path);

        CREATE TABLE IF NOT EXISTS images (
            id integer PRIMARY KEY AUTOINCREMENT NOT NULL,
            depth integer NOT NULL,
            path text NOT NULL,
            name varchar NOT NULL,
            parent varchar NOT NULL,
            small varchar NOT NULL,
            medium varchar NOT NULL,
            large varchar NOT NULL,
            exif integer NOT NULL DEFAULT 0,

            width integer NOT NULL DEFAULT 0,
            height integer NOT NULL DEFAULT 0,
            large_width integer NOT NULL DEFAULT 0,
            large_height integer NOT NULL DEFAULT 0,
            medium_width integer NOT NULL DEFAULT 0,
            medium_height integer NOT NULL DEFAULT 0,
            small_width integer NOT NULL DEFAULT 0,
            small_height integer NOT NULL DEFAULT 0,

            captured_at varchar NOT NULL DEFAULT '',
            fstop varchar NOT NULL DEFAULT '',
            exposure_time varchar NOT NULL DEFAULT '',
            iso_speed varchar NOT NULL DEFAULT '',
            exposure_bias varchar NOT NULL DEFAULT '',
            flash varchar NOT NULL DEFAULT '',
            metering_mode varchar NOT NULL DEFAULT '',
            focal_length varchar NOT NULL DEFAULT '',
            focal_length_35mm varchar NOT NULL DEFAULT '',
            camera_make varchar NOT NULL DEFAULT '',
            camera_model varchar NOT NULL DEFAULT '',
            lens_make varchar NOT NULL DEFAULT '',
            lens_model varchar NOT NULL DEFAULT '',
            software varchar NOT NULL DEFAULT '',
            description varchar NOT NULL DEFAULT '',
            copyright varchar NOT NULL DEFAULT '',
            gps varchar NOT NULL DEFAULT '',
            gps_latitude double NOT NULL DEFAULT '',
            gps_longitude double NOT NULL DEFAULT '',
            gps_altitude double NOT NULL DEFAULT '',

            created_at datetime NOT NULL,
            updated_at datetime NOT NULL
        );
        CREATE UNIQUE INDEX IF NOT EXISTS images_path_idx ON images(path);
    )sql");

    // --------------------------------------------------------------

    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

#if SHASHIN_DEBUG
    std::cout << "salt small:  '" << m_salt_small << "'\n"
              << "salt medium: '" << m_salt_medium << "'\n"
              << "salt large:  '" << m_salt_large << "'\n\n";
#endif

    sync_nodes();
    sync_images();
    update_exif();
    process_images();
    create_gallery_files();
    dump_list_html();

    timestamp_end = util::make_timestamp();
    std::cout << "---------------------------------" << "\n"
              << std::setfill(' ') << std::setw(8) << util::time_between<std::chrono::seconds>(timestamp_start, timestamp_end) << " " << "sec" << "  " << "total or" << "\n"
              << std::setfill(' ') << std::setw(8) << util::time_between<std::chrono::minutes>(timestamp_start, timestamp_end) << " " << "min" << "  " << "total" << "\n";

}

Shashin::~Shashin() {
    close_database();
}

auto Shashin::open_database() -> void {
    auto rc{0};
    rc = sqlite3_open_v2(m_config.database_path().string().c_str(), &m_db, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Error: " << sqlite3_errmsg(m_db)
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
        throw fs::filesystem_error("Failed to open database: " + m_config.database_path().string(), std::error_code());
    }
}

auto Shashin::close_database() -> void {
    auto rc{0};
    rc = sqlite3_close(m_db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error: " << sqlite3_errmsg(m_db)
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
    }
}

auto Shashin::exec_query(std::string const& query, int (*callback)(void*, int argc, char**, char**), void* dst) const -> void {
    auto rc{0};
    char* sqlite_error_message{nullptr};
    rc = sqlite3_exec(m_db, query.c_str(), callback, dst, &sqlite_error_message);
    if (rc != SQLITE_OK) {
        std::cerr << "Error: " << sqlite3_errmsg(m_db)
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
    }
    sqlite3_free(sqlite_error_message);
}

auto Shashin::exec_transaction(char const* const query, std::function<void(sqlite3_stmt* stmt)> func) const -> void {
    auto rc{0};
    sqlite3_stmt* stmt{nullptr};
    sqlite3_mutex_enter(sqlite3_db_mutex(m_db));
    exec_query("PRAGMA synchronous=OFF");
    exec_query("PRAGMA count_changes=OFF");
    exec_query("PRAGMA journal_mode=MEMORY");
    exec_query("PRAGMA temp_store=MEMORY");
    exec_query("BEGIN TRANSACTION");
    rc = sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Error: " << sqlite3_errmsg(m_db)
            #ifdef SHASHIN_DEBUG
                  << " [" << __FILE__ << ":" << __LINE__ << "]"
            #endif
                  << "\n";
    } else {
        func(stmt);
    }
    exec_query("COMMIT TRANSACTION");
    sqlite3_finalize(stmt);
    sqlite3_mutex_leave(sqlite3_db_mutex(m_db));
}

auto Shashin::create_directories() const -> void {
    std::vector<fs::path> const paths {
        m_config.shashin_path(),
        m_config.gallery_path(),
        m_config.site_path(),
        m_config.cache_path(),
        m_config.data_path()
    };
    for (auto const& path : paths) {
        if (!fs::exists(path)) {
            fs::create_directories(path);
        }
    }
    for (auto const& path : paths) {
        if (!fs::exists(path)) {
            throw fs::filesystem_error("Failed to create directory: " + path.string(), std::error_code());
        }
    }
}

auto Shashin::is_gallery(std::string const& name) const -> bool {
    return std::count(name.begin(), name.end(), *(m_config.gallery_delim().c_str())) > 0;
}

auto Shashin::gallery_parts(std::string const&name) const -> std::tuple<std::string, std::string, std::string, std::string, std::string, std::string> {
    std::string captured_at;
    std::string title;
    std::string event;
    std::string location;
    std::string city;
    std::string country;

    if (is_gallery(name)) {
        captured_at = [&name]() -> std::string const {
            std::smatch match;
            if (std::regex_match(name, match, std::regex("^[0-9]{6}.*"))) {
                return "20" + name.substr(0, 2) + "-" + name.substr(2, 2) + "-" + name.substr(4, 2) + " 00:00:00";
            }
            return std::string("");
        }();

        std::vector<std::string> tokens{util::str_split(name, m_config.gallery_delim())};
        auto const finalize_meta_data{[](std::string& input) -> void {
            util::stackoverflow::trim(input);
            if (input == "nil" || input == "null") {
                input = "";
            }
        }};

        title = [&]() -> std::string const {
            std::string str;
            if (tokens.size() > 0) {
                str = tokens[0];
            }
            if (captured_at.size() > 0 && str.size() > 6) {
                str = str.substr(6);
            }
            finalize_meta_data(str);
            return str;
        }();

        event = [&]() -> std::string const {
            std::string str;
            if (tokens.size() > 1) {
                str = tokens[1];
            }
            finalize_meta_data(str);
            return str;
        }();

        location = [&]() -> std::string const {
            std::string str;
            if (tokens.size() > 2) {
                str = tokens[2];
            }
            finalize_meta_data(str);
            return str;
        }();

        city = [&]() -> std::string const {
            std::string str;
            if (tokens.size() > 3) {
                str = tokens[3];
            }
            str = std::regex_replace(str, std::regex(",.*"), "");
            finalize_meta_data(str);
            return str;
        }();

        country = [&]() -> std::string const {
            std::string str;
            if (tokens.size() > 3) {
                str = tokens[3];
            }
            str = std::regex_replace(str, std::regex(".*?,"), "");
            finalize_meta_data(str);
            return str;
        }();
    }

    return {captured_at, title, event, location, city, country};
}

auto Shashin::sync_nodes() const -> void {
    long long duration_ms{0};
    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

    auto const nodes{list_directory_recursive(m_config.gallery_path(), [](fs::path const& path) -> bool {
        return fs::is_directory(path);
    })};

    exec_transaction(R"sql(
        INSERT INTO nodes (created_at, updated_at, depth, path, name, url, hash, captured_at, title, event, location, city, country)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
        ON CONFLICT(path) DO UPDATE SET updated_at=?;
    )sql", [this, &nodes](sqlite3_stmt* stmt) -> void {
        int i{0};
        for (auto const& abs_path: nodes) {
            auto const rel_path{fs::relative(abs_path, m_config.gallery_path())};
            auto const path{rel_path.string()};
            auto const name{rel_path.filename().string()};
            auto const url{util::string_to_url(path)};
            auto const hash{util::hash_to_hex_string(util::string_to_hash(path))};
            auto const depth{static_cast<int>(std::count(path.begin(), path.end(), '/'))};
            auto [captured_at, title, event, location, city, country]{gallery_parts(name)};

            i = 0;
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // created_at
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // updated_at
            sqlite3_bind_int(stmt, ++i, depth); // depth
            util::sqlite3_bind_string(stmt, ++i, path); // path
            util::sqlite3_bind_string(stmt, ++i, name); // name
            util::sqlite3_bind_string(stmt, ++i, url); // url
            util::sqlite3_bind_string(stmt, ++i, hash); // hash
            util::sqlite3_bind_string(stmt, ++i, captured_at); // captured_at
            util::sqlite3_bind_string(stmt, ++i, title); // title
            util::sqlite3_bind_string(stmt, ++i, event); // event
            util::sqlite3_bind_string(stmt, ++i, location); // location
            util::sqlite3_bind_string(stmt, ++i, city); // city
            util::sqlite3_bind_string(stmt, ++i, country); // country
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // updated_at

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
    });

    exec_query("DELETE FROM nodes WHERE updated_at < '" + m_config.current_time() + "'");

    timestamp_end = util::make_timestamp();
    duration_ms = util::time_between(timestamp_start, timestamp_end);
    std::cout << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms" << "  " << "sync nodes" << "\n" << std::flush;
}

auto Shashin::sync_images() const -> void {
    long long duration_ms{0};
    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

    auto const images{list_directory_recursive(m_config.gallery_path(), [](fs::path const& path) -> bool {
        return !fs::is_directory(path) && fs::is_regular_file(path)
            && (path.extension() == ".jpg" || path.extension() == ".jpeg" || path.extension() == ".jpe");
    })};

    exec_transaction(R"sql(
        INSERT INTO images (created_at, updated_at, depth, path, name, parent, small, medium, large)
        VALUES (?,?,?,?,?,?,?,?,?)
        ON CONFLICT(path) DO UPDATE SET updated_at=?;
    )sql", [this, &images](sqlite3_stmt* stmt) -> void {
        int i{0};
        for (auto const& abs_path: images) {
            auto const rel_path{fs::relative(abs_path, m_config.gallery_path())};
            auto const path{rel_path.string()};
            auto const name{rel_path.filename().string()};
            auto const depth{static_cast<int>(std::count(path.begin(), path.end(), '/'))};
            auto const parent{rel_path.parent_path().string()};
            auto const small{util::hash_to_hex_string(util::string_to_hash(path + m_config.salt_small()))};
            auto const medium{util::hash_to_hex_string(util::string_to_hash(path + m_config.salt_medium()))};
            auto const large{util::hash_to_hex_string(util::string_to_hash(path + m_config.salt_large()))};

            i = 0;
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // created_at
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // updated_at
            sqlite3_bind_int(stmt, ++i, depth); // depth
            util::sqlite3_bind_string(stmt, ++i, path); // path
            util::sqlite3_bind_string(stmt, ++i, name); // name
            util::sqlite3_bind_string(stmt, ++i, parent); // parent
            util::sqlite3_bind_string(stmt, ++i, small); // small
            util::sqlite3_bind_string(stmt, ++i, medium); // medium
            util::sqlite3_bind_string(stmt, ++i, large); // large
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // updated_at

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
    });

    exec_query("DELETE FROM images WHERE updated_at < '" + m_config.current_time() + "'");

    timestamp_end = util::make_timestamp();
    duration_ms = util::time_between(timestamp_start, timestamp_end);
    std::cout << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms" << "  " << "sync images" << "\n" << std::flush;
}

auto Shashin::update_exif() const -> void {
    long long duration_ms{0};
    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

    std::vector<std::string> images;
    std::unordered_map<std::string, std::tuple<bool, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, double, double, double>> images_with_exif;

    exec_transaction(R"sql(
        SELECT path FROM images WHERE exif is NULL or exif != 1;
    )sql", [this, &images](sqlite3_stmt* stmt) -> void {
        auto rc{0};
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            images.push_back(std::string(reinterpret_cast<char const* const>(sqlite3_column_text(stmt, 0))));
        }
        if (rc != SQLITE_DONE) {
            std::cerr << "Error: " << sqlite3_errmsg(m_db)
                #ifdef SHASHIN_DEBUG
                      << " [" << __FILE__ << ":" << __LINE__ << "]"
                #endif
                      << "\n";
        }
    });

    util::process_parallel([this, &images, &images_with_exif](int worker_number, int lower_bound, int upper_bound) {
        long long duration_ms{0};
        auto timestamp_end{util::make_timestamp()};
        auto timestamp_start{util::make_timestamp()};

        for (auto i{lower_bound}; i < upper_bound; ++i) {
            //std::cout << images[size_t(i)] << "\n";
            images_with_exif[images[size_t(i)]] = util::exif_info(fs::path(m_config.gallery_path()).append(images[size_t(i)]));
        }

        timestamp_end = util::make_timestamp();
        duration_ms = util::time_between(timestamp_start, timestamp_end);

#if SHASHIN_DEBUG
        mtx.lock();
        std::cout << "worker_number: " << std::setw(1) << worker_number
                  << "    "
                  << "lower_bound: " << std::setw(4) << lower_bound
                  << "    "
                  << "upper_bound: " << std::setw(4) << upper_bound
                  << "    "
                  << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms"
                  << "\n" << std::flush;
        mtx.unlock();
#endif
    }, int(images.size()));

    exec_transaction(R"sql(
        UPDATE images SET
            captured_at = ?,
            fstop = ?,
            exposure_time = ?,
            iso_speed = ?,
            exposure_bias = ?,
            flash = ?,
            metering_mode = ?,
            focal_length = ?,
            focal_length_35mm = ?,
            camera_make = ?,
            camera_model = ?,
            lens_make = ?,
            lens_model = ?,
            software = ?,
            description = ?,
            copyright = ?,
            gps = ?,
            gps_latitude = ?,
            gps_longitude = ?,
            gps_altitude = ?,

            exif = ?,
            updated_at = ?
        WHERE path = ?;
    )sql", [this, &images, &images_with_exif](sqlite3_stmt* stmt) -> void {
        auto i{0};
        for (auto const& path: images) {
            auto [exif, captured_at, fstop, exposure_time, iso_speed, exposure_bias, flash, metering_mode, camera_make, camera_model, lens_make, lens_model, copyright, description, software, focal_length, focal_length_35mm, gps, gps_latitude, gps_longitude, gps_altitude]{images_with_exif[path]};

            i = 0;
            util::sqlite3_bind_string(stmt, ++i, captured_at); // captured_at
            util::sqlite3_bind_string(stmt, ++i, fstop); // fstop
            util::sqlite3_bind_string(stmt, ++i, exposure_time); // exposure_time
            util::sqlite3_bind_string(stmt, ++i, iso_speed); // iso_speed
            util::sqlite3_bind_string(stmt, ++i, exposure_bias); // exposure_bias
            util::sqlite3_bind_string(stmt, ++i, flash); // flash
            util::sqlite3_bind_string(stmt, ++i, metering_mode); // metering_mode
            util::sqlite3_bind_string(stmt, ++i, focal_length); // focal_length
            util::sqlite3_bind_string(stmt, ++i, focal_length_35mm); // focal_length_35mm
            util::sqlite3_bind_string(stmt, ++i, camera_make); // camera_make
            util::sqlite3_bind_string(stmt, ++i, camera_model); // camera_model
            util::sqlite3_bind_string(stmt, ++i, lens_make); // lens_make
            util::sqlite3_bind_string(stmt, ++i, lens_model); // lens_model
            util::sqlite3_bind_string(stmt, ++i, software); // software
            util::sqlite3_bind_string(stmt, ++i, description); // description
            util::sqlite3_bind_string(stmt, ++i, copyright); // copyright
            util::sqlite3_bind_string(stmt, ++i, gps); // gps
            sqlite3_bind_double(stmt, ++i, gps_latitude); // gps_latitude
            sqlite3_bind_double(stmt, ++i, gps_longitude); // gps_longitude
            sqlite3_bind_double(stmt, ++i, gps_altitude); // gps_altitude
            sqlite3_bind_int(stmt, ++i, exif); // exif
            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // updated_at
            util::sqlite3_bind_string(stmt, ++i, path); // path

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
    });

    timestamp_end = util::make_timestamp();
    duration_ms = util::time_between(timestamp_start, timestamp_end);
    std::cout << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms" << "  " << "update exif" << "\n" << std::flush;
}

auto Shashin::dump_list_html() const -> void {
    long long duration_ms{0};
    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

    std::stringstream ss;

    // style
    ss << "<style>" << "\n";
    ss << R"css(
        * {
            font-size: 12px;
        }
        td {
            border: 1px solid grey;
            padding: 2px 8px;
        }
    )css" << "\n";
    ss << "</style>" << "\n";

    // nodes
    ss << "<table>" << "\n";
    ss << "<tr>" << "\n"
       << "<td><b>depth</b></td>" << "\n"
       << "<td><b>path</b></td>" << "\n"
       << "<td><b>created_at</b></td>" << "\n"
       << "<td><b>updated_at</b></td>" << "\n"
       << "<td><b>name</b></td>" << "\n"
       << "<td><b>url</b></td>" << "\n"
       << "<td><b>hash</b></td>" << "\n"
       << "<td><b>captured_at</b></td>" << "\n"
       << "<td><b>title</b></td>" << "\n"
       << "<td><b>event</b></td>" << "\n"
       << "<td><b>location</b></td>" << "\n"
       << "<td><b>city</b></td>" << "\n"
       << "<td><b>country</b></td>" << "\n"
       << "</tr>" << "\n";
    exec_transaction(R"sql(
        SELECT
            depth,
            path,
            created_at,
            updated_at,
            name,
            url,
            hash,
            captured_at,
            title,
            event,
            location,
            city,
            country
        FROM nodes
        ORDER BY depth, path, captured_at, title;
    )sql", [this, &ss](sqlite3_stmt* stmt) -> void {
        auto i{0};
        auto rc{0};
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            i = -1;
            auto depth{sqlite3_column_int(stmt, ++i)};
            auto path{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto created_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto updated_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto name{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto url{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto hash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto captured_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto title{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto event{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto location{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto city{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto country{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};

            ss << "<tr>" << "\n"
               << "<td><nobr>" << depth << "</nobr></td>" << "\n"
               << "<td><nobr>" << path << "</nobr></td>" << "\n"
               << "<td><nobr>" << created_at << "</nobr></td>" << "\n"
               << "<td><nobr>" << updated_at << "</nobr></td>" << "\n"
               << "<td><nobr>" << name << "</nobr></td>" << "\n"
               << "<td><nobr>" << url << "</nobr></td>" << "\n"
               << "<td><nobr>" << hash << "</nobr></td>" << "\n"
               << "<td><nobr>" << captured_at << "</nobr></td>" << "\n"
               << "<td><nobr>" << title << "</nobr></td>" << "\n"
               << "<td><nobr>" << event << "</nobr></td>" << "\n"
               << "<td><nobr>" << location << "</nobr></td>" << "\n"
               << "<td><nobr>" << city << "</nobr></td>" << "\n"
               << "<td><nobr>" << country << "</nobr></td>" << "\n"
               << "</tr>" << "\n";
        }
        if (rc != SQLITE_DONE) {
            std::cerr << "Error: " << sqlite3_errmsg(m_db)
                #ifdef SHASHIN_DEBUG
                      << " [" << __FILE__ << ":" << __LINE__ << "]"
                #endif
                      << "\n";
        }
    });
    ss << "</table>" << "\n";

    // images
    ss << "<table>" << "\n";
    ss << "<tr>" << "\n"
       << "<td><b>path</b></td>" << "\n"
       << "<td><b>hash</b></td>" << "\n"
       << "<td><b>small</b></td>" << "\n"
       << "<td><b>medium</b></td>" << "\n"
       << "<td><b>large</b></td>" << "\n"
       << "<td><b>created_at</b></td>" << "\n"
       << "<td><b>updated_at</b></td>" << "\n"
       << "<td><b>captured_at</b></td>" << "\n"
       << "<td><b>fstop</b></td>" << "\n"
       << "<td><b>exposure_time</b></td>" << "\n"
       << "<td><b>iso_speed</b></td>" << "\n"
       << "<td><b>exposure_bias</b></td>" << "\n"
       << "<td><b>flash</b></td>" << "\n"
       << "<td><b>metering_mode</b></td>" << "\n"
       << "<td><b>focal_length</b></td>" << "\n"
       << "<td><b>focal_length_35mm</b></td>" << "\n"
       << "<td><b>camera_make</b></td>" << "\n"
       << "<td><b>camera_model</b></td>" << "\n"
       << "<td><b>lens_make</b></td>" << "\n"
       << "<td><b>lens_model</b></td>" << "\n"
       << "<td><b>software</b></td>" << "\n"
       << "<td><b>description</b></td>" << "\n"
       << "<td><b>copyright</b></td>" << "\n"
       << "<td><b>gps</b></td>" << "\n"
       << "</tr>" << "\n";
    exec_transaction(R"sql(
        SELECT
            i.path,
            n.hash,
            i.small,
            i.medium,
            i.large,
            i.created_at,
            i.updated_at,
            i.captured_at,
            i.fstop,
            i.exposure_time,
            i.iso_speed,
            i.exposure_bias,
            i.flash,
            i.metering_mode,
            i.focal_length,
            i.focal_length_35mm,
            i.camera_make,
            i.camera_model,
            i.lens_make,
            i.lens_model,
            i.software,
            i.description,
            i.copyright,
            i.gps
        FROM images i INNER JOIN nodes n ON i.parent = n.path
        ORDER BY i.parent, i.captured_at;
    )sql", [this, &ss](sqlite3_stmt* stmt) -> void {
        auto i{0};
        auto rc{0};
        auto const extension{".jpg"};
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            i = -1;
            auto path{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto hash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto small{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto medium{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto large{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto created_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto updated_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto captured_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto fstop{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto exposure_time{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto iso_speed{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto exposure_bias{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto flash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto metering_mode{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto focal_length{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto focal_length_35mm{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto camera_make{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto camera_model{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto lens_make{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto lens_model{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto software{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto description{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto copyright{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto gps{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};

            auto const dst_path_small{fs::path{m_config.cache_path()}.append(hash).append(small + extension)};
            auto const dst_path_medium{fs::path{m_config.cache_path()}.append(hash).append(medium + extension)};
            auto const dst_path_large{fs::path{m_config.cache_path()}.append(hash).append(large + extension)};

            fstop = fstop.size() > 0 ? "f/" + fstop : fstop;
            exposure_bias = exposure_bias.size() > 0 ? exposure_bias + " EV" : exposure_bias;
            focal_length = focal_length.size() > 0 ? focal_length + " mm" : focal_length;
            focal_length_35mm = focal_length_35mm.size() > 0 ? focal_length_35mm + " mm" : focal_length_35mm;

            ss << "<tr>" << "\n"
               << "<td><nobr>" << path << "</nobr></td>" << "\n"
               << "<td><nobr>" << hash << "</nobr></td>" << "\n"
               << "<td><nobr><a href=\"" << dst_path_small.string() << "\">" << small << "</a></nobr></td>" << "\n"
               << "<td><nobr><a href=\"" << dst_path_medium.string() << "\">" << medium << "</a></nobr></td>" << "\n"
               << "<td><nobr><a href=\"" << dst_path_large.string() << "\">" << large << "</a></nobr></td>" << "\n"
               << "<td><nobr>" << created_at << "</nobr></td>" << "\n"
               << "<td><nobr>" << updated_at << "</nobr></td>" << "\n"
               << "<td><nobr>" << captured_at << "</nobr></td>" << "\n"
               << "<td><nobr>" << fstop << "</nobr></td>" << "\n"
               << "<td><nobr>" << exposure_time << "</nobr></td>" << "\n"
               << "<td><nobr>" << iso_speed << "</nobr></td>" << "\n"
               << "<td><nobr>" << exposure_bias << "</nobr></td>" << "\n"
               << "<td><nobr>" << flash << "</nobr></td>" << "\n"
               << "<td><nobr>" << metering_mode << "</nobr></td>" << "\n"
               << "<td><nobr>" << focal_length << "</nobr></td>" << "\n"
               << "<td><nobr>" << focal_length_35mm << "</nobr></td>" << "\n"
               << "<td><nobr>" << camera_make << "</nobr></td>" << "\n"
               << "<td><nobr>" << camera_model << "</nobr></td>" << "\n"
               << "<td><nobr>" << lens_make << "</nobr></td>" << "\n"
               << "<td><nobr>" << lens_model << "</nobr></td>" << "\n"
               << "<td><nobr>" << software << "</nobr></td>" << "\n"
               << "<td><nobr>" << description << "</nobr></td>" << "\n"
               << "<td><nobr>" << copyright << "</nobr></td>" << "\n"
               << "<td><nobr>" << gps << "</nobr></td>" << "\n"
               << "</tr>" << "\n";
        }
        if (rc != SQLITE_DONE) {
            std::cerr << "Error: " << sqlite3_errmsg(m_db)
                #ifdef SHASHIN_DEBUG
                      << " [" << __FILE__ << ":" << __LINE__ << "]"
                #endif
                      << "\n";
        }
    });
    ss << "</table>" << "\n";

    std::ofstream ofs{fs::path{m_config.shashin_path()}.append("list.html")};
    ofs << ss.str();
    ofs.close();

    timestamp_end = util::make_timestamp();
    duration_ms = util::time_between(timestamp_start, timestamp_end);
    std::cout << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms" << "  " << "dump list html" << "\n" << std::flush;
}

auto Shashin::process_images() const -> void {
    long long duration_ms{0};
    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

    std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, int, int, int, int, int, int, int, int>> images;

    exec_transaction(R"sql(
        SELECT
            i.path,
            n.hash,
            i.small,
            i.medium,
            i.large,
            i.width,
            i.height,
            i.large_width,
            i.large_height,
            i.medium_width,
            i.medium_height,
            i.small_width,
            i.small_height
        FROM images i INNER JOIN nodes n ON i.parent = n.path
        ORDER BY i.parent, i.captured_at;
    )sql", [this, &images](sqlite3_stmt* stmt) -> void {
        auto i{0};
        auto rc{0};
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            i = -1;
            auto path{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto hash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto small{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto medium{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto large{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
            auto width{sqlite3_column_int(stmt, ++i)};
            auto height{sqlite3_column_int(stmt, ++i)};
            auto large_width{sqlite3_column_int(stmt, ++i)};
            auto large_height{sqlite3_column_int(stmt, ++i)};
            auto medium_width{sqlite3_column_int(stmt, ++i)};
            auto medium_height{sqlite3_column_int(stmt, ++i)};
            auto small_width{sqlite3_column_int(stmt, ++i)};
            auto small_height{sqlite3_column_int(stmt, ++i)};
            images.push_back({path, hash, small, medium, large, width, height, large_width, large_height, medium_width, medium_height, small_width, small_height});
        }
        if (rc != SQLITE_DONE) {
            std::cerr << "Error: " << sqlite3_errmsg(m_db)
                #ifdef SHASHIN_DEBUG
                      << " [" << __FILE__ << ":" << __LINE__ << "]"
                #endif
                      << "\n";
        }
    });

    util::process_parallel([this, &images](int worker_number, int lower_bound, int upper_bound) {
        (void)worker_number;
        auto const extension{".jpg"};
        auto percent{0};
        for (auto i{lower_bound}; i < upper_bound; ++i) {
            if (worker_number == 0) {
                auto temp{int(double(i) / double(upper_bound) * 100) % 101};
                if (temp > percent) {
                    percent = temp;
                    std::cout << "        " << "   " << "  " << std::setfill(' ') << std::setw(3) << percent << " " << "%" << "\n" << std::flush;
                }
            }

            auto [path, hash, small, medium, large, width, height, large_width, large_height, medium_width, medium_height, small_width, small_height]{images[size_t(i)]};

            auto const src_path{fs::path{m_config.gallery_path()}.append(path)};
            auto const dst_path_small{fs::path{m_config.cache_path()}.append("small").append(hash).append(small + extension)};
            auto const dst_path_medium{fs::path{m_config.cache_path()}.append("medium").append(hash).append(medium + extension)};
            auto const dst_path_large{fs::path{m_config.cache_path()}.append("large").append(hash).append(large + extension)};

            if (!fs::exists(dst_path_small) || !fs::exists(dst_path_medium) || !fs::exists(dst_path_large)) {
                cv::Mat src_mat{cv::imread(src_path)};
                std::get<5>(images[size_t(i)]) = src_mat.size().width;
                std::get<6>(images[size_t(i)]) = src_mat.size().height;

                if (!fs::exists(dst_path_small) || (std::get<11>(images[size_t(i)]) == 0 || std::get<12>(images[size_t(i)]) == 0)) {
                    try {
                        util::crop(src_mat, dst_path_small, m_config.small_width(), m_config.small_height());
                        std::get<11>(images[size_t(i)]) = m_config.small_width();
                        std::get<12>(images[size_t(i)]) = m_config.small_height();
                    } catch (std::exception const& e) {
                        std::cerr << __FILE__ << ":" << __LINE__ << "\n"
                                  << "file: " << src_path << "\n"
                                  << e.what() << "\n"
                                  << "\n";
                        exit(0);
                    }
                }
                if (!fs::exists(dst_path_medium) || (std::get<9>(images[size_t(i)]) == 0 || std::get<10>(images[size_t(i)]) == 0)) {
                    util::resize(src_mat, dst_path_medium, m_config.medium_size(), m_config.watermark_text(), 24, 16, 4);
                    cv::Mat tmp_mat{cv::imread(dst_path_medium)};
                    std::get<9>(images[size_t(i)]) = tmp_mat.size().width;
                    std::get<10>(images[size_t(i)]) = tmp_mat.size().height;
                }
                if (!fs::exists(dst_path_large) || (std::get<7>(images[size_t(i)]) == 0 || std::get<8>(images[size_t(i)]) == 0)) {
                    util::resize(src_mat, dst_path_large, m_config.large_size(), m_config.watermark_text(), 36, 32, 6);
                    cv::Mat tmp_mat{cv::imread(dst_path_large)};
                    std::get<7>(images[size_t(i)]) = tmp_mat.size().width;
                    std::get<8>(images[size_t(i)]) = tmp_mat.size().height;
                }
            }
        }
    }, int(images.size()));

    exec_transaction(R"sql(
        UPDATE images SET
            width = ?,
            height = ?,
            large_width = ?,
            large_height = ?,
            medium_width = ?,
            medium_height = ?,
            small_width = ?,
            small_height = ?,

            updated_at = ?
        WHERE path = ?;
    )sql", [this, &images](sqlite3_stmt* stmt) -> void {
        auto i{0};
        for (auto const& image: images) {
            auto [path, hash, small, medium, large, width, height, large_width, large_height, medium_width, medium_height, small_width, small_height]{image};

            i = 0;
            util::sqlite3_bind_int_or_null(stmt, ++i, width); // width
            util::sqlite3_bind_int_or_null(stmt, ++i, height); // height
            util::sqlite3_bind_int_or_null(stmt, ++i, large_width); // large_width
            util::sqlite3_bind_int_or_null(stmt, ++i, large_height); // large_height
            util::sqlite3_bind_int_or_null(stmt, ++i, medium_width); // medium_width
            util::sqlite3_bind_int_or_null(stmt, ++i, medium_height); // medium_height
            util::sqlite3_bind_int_or_null(stmt, ++i, small_width); // small_width
            util::sqlite3_bind_int_or_null(stmt, ++i, small_height); // small_height

            util::sqlite3_bind_string(stmt, ++i, m_config.current_time()); // updated_at
            util::sqlite3_bind_string(stmt, ++i, path); // path

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
    });

    timestamp_end = util::make_timestamp();
    duration_ms = util::time_between(timestamp_start, timestamp_end);
    std::cout << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms" << "  " << "process images" << "\n" << std::flush;
}

auto Shashin::create_gallery_files() const -> void {
    long long duration_ms{0};
    auto timestamp_end{util::make_timestamp()};
    auto timestamp_start{util::make_timestamp()};

    // nodes
    {
        std::stringstream ss;
        ss << "\"" << "hash" << "\"" << ","
           << "\"" << "path" << "\"" << ","
           << "\"" << "depth" << "\"" << ","
           << "\"" << "name" << "\"" << ","
           << "\"" << "url" << "\"" << ","
           << "\"" << "captured_at" << "\"" << ","
           << "\"" << "title" << "\"" << ","
           << "\"" << "event" << "\"" << ","
           << "\"" << "location" << "\"" << ","
           << "\"" << "city" << "\"" << ","
           << "\"" << "country" << "\"" << "\n";
        exec_transaction(R"sql(
            SELECT
                depth,
                path,
                created_at,
                updated_at,
                name,
                url,
                hash,
                captured_at,
                title,
                event,
                location,
                city,
                country
            FROM nodes
            ORDER BY depth, path, captured_at, title;
        )sql", [this, &ss](sqlite3_stmt* stmt) -> void {
            auto i{0};
            auto rc{0};
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                i = -1;
                auto depth{sqlite3_column_int(stmt, ++i)};
                auto path{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto created_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto updated_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto name{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto url{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto hash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto captured_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto title{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto event{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto location{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto city{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto country{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};

                ss << "\"" << hash << "\"" << ","
                   << "\"" << path << "\"" << ","
                   << "\"" << depth << "\"" << ","
                   << "\"" << name << "\"" << ","
                   << "\"" << url << "\"" << ","
                   << "\"" << captured_at << "\"" << ","
                   << "\"" << title << "\"" << ","
                   << "\"" << event << "\"" << ","
                   << "\"" << location << "\"" << ","
                   << "\"" << city << "\"" << ","
                   << "\"" << country << "\"" << "\n";
            }
            if (rc != SQLITE_DONE) {
                std::cerr << "Error: " << sqlite3_errmsg(m_db)
                    #ifdef SHASHIN_DEBUG
                          << " [" << __FILE__ << ":" << __LINE__ << "]"
                    #endif
                          << "\n";
            }
        });
        util::dump_to_file(fs::path{m_config.data_path()}.append("nodes.csv"), ss.str());
    }

    // images
    {
        std::stringstream ss;
        ss << "\"" << "node_hash" << "\"" << ","
           << "\"" << "small_hash" << "\"" << ","
           << "\"" << "medium_hash" << "\"" << ","
           << "\"" << "large_hash" << "\"" << ","
           << "\"" << "small_path" << "\"" << ","
           << "\"" << "medium_path" << "\"" << ","
           << "\"" << "large_path" << "\"" << ","
           << "\"" << "width" << "\"" << ","
           << "\"" << "height" << "\"" << ","
           << "\"" << "large_width" << "\"" << ","
           << "\"" << "large_height" << "\"" << ","
           << "\"" << "medium_width" << "\"" << ","
           << "\"" << "medium_height" << "\"" << ","
           << "\"" << "small_width" << "\"" << ","
           << "\"" << "small_height" << "\"" << ","
           << "\"" << "captured_at" << "\"" << ","
           << "\"" << "fstop" << "\"" << ","
           << "\"" << "exposure_time" << "\"" << ","
           << "\"" << "iso_speed" << "\"" << ","
           << "\"" << "exposure_bias" << "\"" << ","
           << "\"" << "flash" << "\"" << ","
           << "\"" << "metering_mode" << "\"" << ","
           << "\"" << "focal_length" << "\"" << ","
           << "\"" << "focal_length_35mm" << "\"" << ","
           << "\"" << "camera_make" << "\"" << ","
           << "\"" << "camera_model" << "\"" << ","
           << "\"" << "lens_make" << "\"" << ","
           << "\"" << "lens_model" << "\"" << ","
           << "\"" << "software" << "\"" << ","
           << "\"" << "description" << "\"" << ","
           << "\"" << "copyright" << "\"" << ","
           << "\"" << "gps" << "\"" << "\n";
        exec_transaction(R"sql(
            SELECT
                i.path,
                n.hash,
                i.small,
                i.medium,
                i.large,
                i.width,
                i.height,
                i.large_width,
                i.large_height,
                i.medium_width,
                i.medium_height,
                i.small_width,
                i.small_height,
                i.created_at,
                i.updated_at,
                i.captured_at,
                i.fstop,
                i.exposure_time,
                i.iso_speed,
                i.exposure_bias,
                i.flash,
                i.metering_mode,
                i.focal_length,
                i.focal_length_35mm,
                i.camera_make,
                i.camera_model,
                i.lens_make,
                i.lens_model,
                i.software,
                i.description,
                i.copyright,
                i.gps
            FROM images i INNER JOIN nodes n ON i.parent = n.path
            ORDER BY i.parent, i.captured_at;
        )sql", [this, &ss](sqlite3_stmt* stmt) -> void {
            auto i{0};
            auto rc{0};
            auto const extension{".jpg"};
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                i = -1;
                auto path{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto hash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto small{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto medium{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto large{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto width{sqlite3_column_int(stmt, ++i)};
                auto height{sqlite3_column_int(stmt, ++i)};
                auto large_width{sqlite3_column_int(stmt, ++i)};
                auto large_height{sqlite3_column_int(stmt, ++i)};
                auto medium_width{sqlite3_column_int(stmt, ++i)};
                auto medium_height{sqlite3_column_int(stmt, ++i)};
                auto small_width{sqlite3_column_int(stmt, ++i)};
                auto small_height{sqlite3_column_int(stmt, ++i)};
                auto created_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto updated_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto captured_at{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto fstop{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto exposure_time{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto iso_speed{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto exposure_bias{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto flash{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto metering_mode{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto focal_length{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto focal_length_35mm{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto camera_make{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto camera_model{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto lens_make{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto lens_model{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto software{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto description{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto copyright{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};
                auto gps{std::string{reinterpret_cast<char const* const>(sqlite3_column_text(stmt, ++i))}};

                auto const dst_path_small{"/" + fs::path{m_config.cache_dir()}.append("small").append(hash).append(small + extension).string()};
                auto const dst_path_medium{"/" + fs::path{m_config.cache_dir()}.append("medium").append(hash).append(medium + extension).string()};
                auto const dst_path_large{"/" + fs::path{m_config.cache_dir()}.append("large").append(hash).append(large + extension).string()};

                gps = std::regex_replace(gps, std::regex(R"raw(°)raw"), "&deg;");
                gps = std::regex_replace(gps, std::regex(R"raw(")raw"), "&quot;");
                gps = std::regex_replace(gps, std::regex(R"raw(')raw"), "&apos;");

                ss << "\"" << hash << "\"" << ","
                   << "\"" << small<< "\"" << ","
                   << "\"" << medium << "\"" << ","
                   << "\"" << large << "\"" << ","
                   << "\"" << dst_path_small << "\"" << ","
                   << "\"" << dst_path_medium << "\"" << ","
                   << "\"" << dst_path_large << "\"" << ","
                   << "\"" << width << "\"" << ","
                   << "\"" << height << "\"" << ","
                   << "\"" << large_width << "\"" << ","
                   << "\"" << large_height << "\"" << ","
                   << "\"" << medium_width << "\"" << ","
                   << "\"" << medium_height << "\"" << ","
                   << "\"" << small_width << "\"" << ","
                   << "\"" << small_height << "\"" << ","
                   << "\"" << captured_at << "\"" << ","
                   << "\"" << fstop << "\"" << ","
                   << "\"" << exposure_time << "\"" << ","
                   << "\"" << iso_speed << "\"" << ","
                   << "\"" << exposure_bias << "\"" << ","
                   << "\"" << flash << "\"" << ","
                   << "\"" << metering_mode << "\"" << ","
                   << "\"" << focal_length << "\"" << ","
                   << "\"" << focal_length_35mm << "\"" << ","
                   << "\"" << camera_make << "\"" << ","
                   << "\"" << camera_model << "\"" << ","
                   << "\"" << lens_make << "\"" << ","
                   << "\"" << lens_model << "\"" << ","
                   << "\"" << software << "\"" << ","
                   << "\"" << description << "\"" << ","
                   << "\"" << copyright << "\"" << ","
                   << "\"" << gps << "\"" << "\n";
            }
            if (rc != SQLITE_DONE) {
                std::cerr << "Error: " << sqlite3_errmsg(m_db)
                    #ifdef SHASHIN_DEBUG
                          << " [" << __FILE__ << ":" << __LINE__ << "]"
                    #endif
                          << "\n";
            }
        });
        util::dump_to_file(fs::path{m_config.data_path()}.append("images.csv"), ss.str());
    }

    timestamp_end = util::make_timestamp();
    duration_ms = util::time_between(timestamp_start, timestamp_end);
    std::cout << std::setfill(' ') << std::setw(8) << duration_ms << " " << "ms" << "  " << "create gallery files" << "\n" << std::flush;
}

} // namespace shashin
