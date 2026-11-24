cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" APP_VERSION LIMIT_COUNT 1)
project(sitecheck VERSION "${APP_VERSION}" LANGUAGES C)
include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(CURL REQUIRED)

add_library(sitecore STATIC src/core.c)
target_include_directories(sitecore PUBLIC "${CMAKE_SOURCE_DIR}/src")


add_executable(sitecheck src/main.c)
target_compile_definitions(sitecheck PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(sitecheck PRIVATE sitecore CURL::libcurl)

install(TARGETS sitecheck RUNTIME DESTINATION bin)

install(FILES packaging/sitecheck.conf
    DESTINATION /etc/sitecheck
)

install(FILES packaging/man/sitecheck.1
    DESTINATION share/man/man1
)

if(BUILD_TESTING)
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE sitecore)
    target_compile_definitions(test_core PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    )
    add_test(NAME core COMMAND test_core)

    add_test(NAME cli_version COMMAND sitecheck --version)
    set_tests_properties(cli_version PROPERTIES
        PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
    )

    add_test(NAME cli_bad_args COMMAND sitecheck --no-such-option)
    set_tests_properties(cli_bad_args PROPERTIES WILL_FAIL TRUE)
endif()
