cmake_minimum_required(VERSION 3.20)
project(linux_device_manager VERSION 0.1.0 LANGUAGES C)

option(DEVMGR_BUILD_TESTS "Build tests" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_COVERAGE "Enable gcov coverage" OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow
                        -Wstrict-prototypes -Wmissing-prototypes)
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
    if(ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=undefined)
    endif()
    if(ENABLE_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

add_library(devmgr_common STATIC)
target_sources(devmgr_common PRIVATE
    src/common/crc32.c
    src/common/error.c
    src/common/log.c
    src/common/ring_buffer.c)
target_include_directories(devmgr_common PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_library(devmgr_protocol STATIC)
target_sources(devmgr_protocol PRIVATE
    src/protocol/frame.c
    src/protocol/parser.c)
target_include_directories(devmgr_protocol PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(devmgr_protocol PUBLIC devmgr_common)

add_library(devmgr_device STATIC src/device/session.c)
target_include_directories(devmgr_device PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(devmgr_device PUBLIC devmgr_protocol)

add_library(devmgr_ipc STATIC src/ipc/codec.c)
target_include_directories(devmgr_ipc PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(devmgr_ipc PUBLIC devmgr_protocol)

add_library(devmgr_upgrade STATIC src/upgrade/upgrade.c)
target_include_directories(devmgr_upgrade PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(devmgr_upgrade PUBLIC devmgr_protocol)

if(UNIX AND NOT APPLE)
    add_library(devmgr_transport STATIC)
    target_sources(devmgr_transport PRIVATE
        src/transport/serial.c
        src/transport/pty.c)
    target_include_directories(devmgr_transport PUBLIC ${PROJECT_SOURCE_DIR}/include)
    target_compile_definitions(devmgr_transport PRIVATE _DEFAULT_SOURCE)
    target_link_libraries(devmgr_transport PUBLIC devmgr_common)

    target_sources(devmgr_upgrade PRIVATE src/upgrade/firmware.c)
    target_compile_definitions(devmgr_upgrade PRIVATE _GNU_SOURCE)

    add_executable(device-sim
        apps/device-sim/main.c
        simulator/device/simulator.c)
    target_include_directories(device-sim PRIVATE simulator/device)
    target_compile_definitions(device-sim PRIVATE _POSIX_C_SOURCE=200809L)
    target_link_libraries(device-sim PRIVATE devmgr_protocol devmgr_transport)

    add_executable(devmgrd apps/devmgrd/main.c apps/devmgrd/daemon.c)
    target_include_directories(devmgrd PRIVATE apps/devmgrd)
    target_link_libraries(devmgrd PRIVATE devmgr_device devmgr_ipc devmgr_transport devmgr_upgrade)

    add_executable(devctl apps/devctl/main.c)
    target_link_libraries(devctl PRIVATE devmgr_ipc devmgr_upgrade)
endif()

if(DEVMGR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
