cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(shashin CXX)

# -----------------------------------------------------------------------------
# third party -- header only

function(download_file FILENAME DESTINATION URL)
    if(NOT EXISTS "${DESTINATION}/${FILENAME}")
        file(DOWNLOAD "${URL}/${FILENAME}" "${DESTINATION}/${FILENAME}" INACTIVITY_TIMEOUT 30 TIMEOUT 30)
    endif()
endfunction()

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_BINARY_DIR}/third-party")

set(GHC_DIR "${THIRD_PARTY_DIR}/ghc")
set(GHC_URL "https://raw.githubusercontent.com/gulrak/filesystem/v1.2.4/include/ghc/")

set(CITY_DIR "${THIRD_PARTY_DIR}/city")
set(CITY_URL "https://raw.githubusercontent.com/aappleby/smhasher/master/src/")

set(JSON_DIR "${THIRD_PARTY_DIR}/nlohmann")
set(JSON_URL "https://raw.githubusercontent.com/nlohmann/json/v3.7.3/single_include/nlohmann/")

set(EASYEXIF_DIR "${THIRD_PARTY_DIR}/easyexif")
set(EASYEXIF_URL "https://raw.githubusercontent.com/mayanklahiri/easyexif/v1.0/")

foreach(FILENAME "filesystem.hpp" "fs_fwd.hpp" "fs_impl.hpp" "fs_std.hpp" "fs_std_fwd.hpp" "fs_std_impl.hpp")
    download_file(${FILENAME} ${GHC_DIR} ${GHC_URL})
endforeach()

foreach(FILENAME "City.cpp" "City.h")
    download_file(${FILENAME} ${CITY_DIR} ${CITY_URL})
endforeach()

foreach(FILENAME "json.hpp")
    download_file(${FILENAME} ${JSON_DIR} ${JSON_URL})
endforeach()

foreach(FILENAME "exif.cpp" "exif.h")
    download_file(${FILENAME} ${EASYEXIF_DIR} ${EASYEXIF_URL})
endforeach()

# -----------------------------------------------------------------------------
# third party -- SQLite

#find_package(SQLite3)# todo

set(SQLITE_FOUND 1)
set(SQLite3_INCLUDE_DIRS "/usr/local/opt/sqlite/include")
set(SQLite3_LIBRARIES "/usr/local/opt/sqlite/lib/libsqlite3.dylib")

# -----------------------------------------------------------------------------
# third party -- OpenCV

find_package(OpenCV
    HINTS
        "/usr/local/opt/opencv"
        "C:/Program Files/opencv/build"
)

# -----------------------------------------------------------------------------
# shashin

add_executable(${PROJECT_NAME} "src/main.cpp")
target_sources(${PROJECT_NAME} PRIVATE
    "src/shashin/config.cpp"
    "src/shashin/shashin.cpp"
    "src/shashin/util/filesystem.cpp"
    "src/shashin/util/hash.cpp"
    "src/shashin/util/image.cpp"
    "src/shashin/util/parallel.cpp"
    "src/shashin/util/sqlite.cpp"
    "src/shashin/util/string.cpp"
    "src/shashin/util/time.cpp"
    "src/shashin/util/url.cpp"
)
target_sources(${PROJECT_NAME} PRIVATE
    "include/shashin/config.h"
    "include/shashin/shashin.h"
    "include/shashin/util/filesystem.h"
    "include/shashin/util/hash.h"
    "include/shashin/util/image.h"
    "include/shashin/util/parallel.h"
    "include/shashin/util/sqlite.h"
    "include/shashin/util/string.h"
    "include/shashin/util/time.h"
    "include/shashin/util/url.h"
)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "src" "include" ${THIRD_PARTY_DIR})
target_sources(${PROJECT_NAME} PRIVATE "${THIRD_PARTY_DIR}/city/City.cpp")
target_sources(${PROJECT_NAME} PRIVATE "${THIRD_PARTY_DIR}/easyexif/exif.cpp")
target_compile_definitions(${PROJECT_NAME} PRIVATE SHASHIN_DEBUG=0)

if(${SQLITE_FOUND})
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${SQLite3_LIBRARIES})
endif()

if(${OpenCV_FOUND})
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBRARIES})
endif()
