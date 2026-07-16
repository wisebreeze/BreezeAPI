cmake_minimum_required(VERSION 3.22)
project(BreezeAPI VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Output layout ──────────────────────────────────────────────
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/lib/${ANDROID_ABI}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/bin/${ANDROID_ABI}")

# ── Dobby (inline hook framework) ─────────────────────────────
set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_SHARED OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/dobby dobby)

# ── QuickJS (JavaScript engine) ───────────────────────────────
set(QUICKJS_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs)

add_library(quickjs STATIC
    ${QUICKJS_SRC_DIR}/quickjs.c
    ${QUICKJS_SRC_DIR}/libregexp.c
    ${QUICKJS_SRC_DIR}/libunicode.c
    ${QUICKJS_SRC_DIR}/dtoa.c
    ${QUICKJS_SRC_DIR}/quickjs-libc.c
    ${QUICKJS_SRC_DIR}/cutils.c
)

target_include_directories(quickjs PUBLIC
    ${QUICKJS_SRC_DIR}
)

target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="2026-06-04"
)

# QuickJS uses C99 VLAs and other features; suppress warnings
target_compile_options(quickjs PRIVATE
    -Wno-vla-cxx-extension
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -fvisibility=hidden
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "debug")
    target_compile_options(quickjs PRIVATE -O2)
endif()

# ── BreezeAPI shared library ──────────────────────────────────
add_library(breeze_api SHARED
    src/breeze_api.cpp
    src/hook_manager.cpp
    src/symbol_resolver.cpp
    src/js_engine.cpp
    src/logger.cpp
)

target_include_directories(breeze_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dobby/include
    ${QUICKJS_SRC_DIR}
)

# Link dobby_static instead of dobby shared to avoid duplicate symbol
# issues from Dobby's inline Logger::Shared() across compilation units.
# --exclude-libs hides all symbols from static archives so they don't
# leak into the final .so symbol table.
target_link_libraries(breeze_api
    dobby_static
    quickjs
    log
    dl
    m
)

# 16KB page alignment (Android 15+ compatibility)
# See: https://developer.android.com/guide/practices/page-sizes
set(BREEZE_MAX_PAGE_ALIGNMENT "16384" CACHE STRING "Maximum page alignment for .so files")

# Strip non-debug builds
if(NOT CMAKE_BUILD_TYPE STREQUAL "debug")
    target_compile_options(breeze_api PRIVATE -O2 -fvisibility=hidden)
    target_link_options(breeze_api PRIVATE
        -Wl,--strip-all
        -Wl,--gc-sections
        -Wl,--exclude-libs,ALL
        -Wl,-z,max-page-size=${BREEZE_MAX_PAGE_ALIGNMENT}
    )
endif()

# ── Install rules ─────────────────────────────────────────────
install(TARGETS breeze_api
    LIBRARY DESTINATION lib/${ANDROID_ABI}
)
