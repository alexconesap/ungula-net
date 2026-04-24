cmake_minimum_required(VERSION 3.16)
project(ungula_net_lib_tests)

set(CMAKE_CXX_STANDARD 17)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

set(LIB_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

# ---------------------------------------------------------------------------
# UngulaCore (lib/) — needed by test_ntp_time_provider for TimeControl and
# ITimeProvider. Local sibling by default (matches the repo layout during
# development); set -DUSE_LOCAL_DEPS=OFF to FetchContent it from GitHub.
# ---------------------------------------------------------------------------
option(USE_LOCAL_DEPS "Use local sibling lib/ directory for UngulaCore" ON)

if(USE_LOCAL_DEPS)
    set(UNGULA_CORE_SRC ${CMAKE_CURRENT_LIST_DIR}/../../lib/src)
else()
    FetchContent_Declare(
        ungula_core
        GIT_REPOSITORY https://github.com/alexconesap/ungula-core.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(ungula_core)
    set(UNGULA_CORE_SRC ${ungula_core_SOURCE_DIR}/src)
endif()

include_directories(
    ${LIB_SRC}
    ${UNGULA_CORE_SRC}
)

find_package(CURL REQUIRED)

add_executable(test_http_client
    test_http_client.cpp
    ${LIB_SRC}/http/http_client.cpp
)
target_link_libraries(test_http_client gtest_main CURL::libcurl)

# ---------------------------------------------------------------------------
# NtpTimeProvider — pure host test. ntp_client.cpp is compiled too so the
# default seams (ntp_is_synced / ntp_epoch) link; the tests inject fake
# function pointers for all scripted cases, so the desktop SNTP stub never
# actually runs.
# ---------------------------------------------------------------------------
add_executable(test_ntp_time_provider
    test_ntp_time_provider.cpp
    ${LIB_SRC}/ntp/ntp_time_provider.cpp
    ${LIB_SRC}/ntp/ntp_client.cpp
)
target_link_libraries(test_ntp_time_provider gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_http_client)
gtest_discover_tests(test_ntp_time_provider)
