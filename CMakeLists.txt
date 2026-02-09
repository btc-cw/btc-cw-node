cmake_minimum_required(VERSION 3.15)
project(btc_cw_node
    VERSION   1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# 1. Core submodule
# ---------------------------------------------------------------------------
add_subdirectory(deps/btc-cw-core)

# ---------------------------------------------------------------------------
# 2. Required system dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(FFTW3     REQUIRED fftw3)
find_package(CURL REQUIRED)

# ---------------------------------------------------------------------------
# 3. Optional: RTL-SDR support
# ---------------------------------------------------------------------------
option(BTCCW_ENABLE_SDR "Build with RTL-SDR support" OFF)

set(NODE_SOURCES
    src/main.cpp
    src/node_engine.cpp
    src/audio_io.cpp
    src/gateway.cpp
    src/goertzel.cpp
    src/morse_decoder.cpp
    src/deframer.cpp
    src/decode_pipeline.cpp
)

if(BTCCW_ENABLE_SDR)
    pkg_check_modules(LIBRTLSDR REQUIRED librtlsdr)
    list(APPEND NODE_SOURCES src/sdr_input.cpp)
endif()

# ---------------------------------------------------------------------------
# 4. Executable
# ---------------------------------------------------------------------------
add_executable(btc-cw-node ${NODE_SOURCES})

target_include_directories(btc-cw-node
    PRIVATE
        include
        ${PORTAUDIO_INCLUDE_DIRS}
        ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(btc-cw-node
    PRIVATE
        btccw_core
        ${PORTAUDIO_LIBRARIES}
        ${FFTW3_LIBRARIES}
        CURL::libcurl
)

if(BTCCW_ENABLE_SDR)
    target_compile_definitions(btc-cw-node PRIVATE BTCCW_HAS_SDR=1)
    target_include_directories(btc-cw-node PRIVATE ${LIBRTLSDR_INCLUDE_DIRS})
    target_link_libraries(btc-cw-node PRIVATE ${LIBRTLSDR_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
# 5. Export compile_commands.json for editor tooling
# ---------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
