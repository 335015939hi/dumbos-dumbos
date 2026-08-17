cmake_minimum_required(VERSION 3.16)

project(dumbos_server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

# libmicrohttpd usually provides a pkg-config file named "libmicrohttpd".
pkg_check_modules(MICROHTTPD REQUIRED IMPORTED_TARGET libmicrohttpd)

add_executable(${PROJECT_NAME}
    src/dumb_handler.c
    src/main.c
    src/common.c
    src/dumbpayload.c
    src/ed25519.c
    src/requestid.c
    src/dumb_log.c
)

add_executable(dumb-code
  src/dumb-main.c
  src/dumb-tool-cmd-set.c
  src/dumbpayload.c
  src/common.c
  src/ed25519.c
  src/requestid.c
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        PkgConfig::MICROHTTPD
        crypto
)

target_link_libraries(dumb-code crypto)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# Helpful for debug builds:
#
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
#
# Then you can add sanitizers manually if desired:
#
# target_compile_options(c_http_server PRIVATE -fsanitize=address,undefined)
# target_link_options(c_http_server PRIVATE -fsanitize=address,undefined)
