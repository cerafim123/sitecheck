cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" APP_VERSION LIMIT_COUNT 1)
project(sitecheck VERSION "${APP_VERSION}" LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(CURL REQUIRED)

add_library(sitecore STATIC src/core.c)
target_include_directories(sitecore PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(sitecore PUBLIC CURL::libcurl)

add_executable(sitecheck src/main.c)
target_compile_definitions(sitecheck PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(sitecheck PRIVATE sitecore)

install(TARGETS sitecheck RUNTIME DESTINATION bin)
