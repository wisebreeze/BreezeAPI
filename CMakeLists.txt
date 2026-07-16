cmake_minimum_required(VERSION 3.22)
project(BreezeAPI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Output layout ──────────────────────────────────────────────
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/lib/${ANDROID_ABI}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/bin/${ANDROID_ABI}")

# ── Dobby (inline hook framework) ─────────────────────────────
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_SECTIONS_HANDLER OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/dobby dobby)

# ── BreezeAPI shared library ──────────────────────────────────
add_library(breeze_api SHARED
    src/breeze_api.cpp
    src/hook_manager.cpp
    src/symbol_resolver.cpp
    src/logger.cpp
)

target_include_directories(breeze_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dobby/include
)

target_link_libraries(breeze_api
    dobby
    log
    dl
)

# Strip non-debug builds
if(NOT CMAKE_BUILD_TYPE STREQUAL "debug")
    target_compile_options(breeze_api PRIVATE -O2 -fvisibility=hidden)
    target_link_options(breeze_api PRIVATE -Wl,--strip-all -Wl,--gc-sections)
endif()

# ── Install rules ─────────────────────────────────────────────
install(TARGETS breeze_api
    LIBRARY DESTINATION lib/${ANDROID_ABI}
)
