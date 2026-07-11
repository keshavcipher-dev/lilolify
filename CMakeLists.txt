# ============================================================================
# Lilolify — Root CMakeLists.txt
# ============================================================================
# This is the top-level build configuration for the Lilolify project.
#
# Architecture:
#   src/core/   → Domain Layer      (zero external dependencies)
#   src/app/    → Application Layer  (depends on core)
#   src/infra/  → Infrastructure     (depends on core)
#   src/ai/     → AI Service Layer   (depends on core)
#   src/ui/     → Presentation Layer (depends on app, infra, ai)
#
# Dependencies flow INWARD: ui → app → core ← infra ← ai
# The core layer NEVER depends on outer layers.
# ============================================================================

cmake_minimum_required(VERSION 3.25)

# ---- Project Declaration ----
project(
    lilolify
    VERSION 0.1.0
    DESCRIPTION "AI-Powered Intelligent Digital File Organization System"
    HOMEPAGE_URL "https://github.com/lilolify/lilolify"
    LANGUAGES CXX
)

# ---- Global Settings ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE tooling (clangd, VS Code, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options ----
option(LILOLIFY_ENABLE_TESTING "Build unit and integration tests" ON)
option(LILOLIFY_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LILOLIFY_WITH_TESSERACT "Build with local Tesseract OCR support" ON)

# ---- Dependency Detection ----
if(LILOLIFY_WITH_TESSERACT)
    # vcpkg installs them as tesseract and leptonica
    find_package(Tesseract CONFIG)
    find_package(Leptonica CONFIG)

    if(Tesseract_FOUND AND Leptonica_FOUND)
        add_compile_definitions(LILOLIFY_WITH_TESSERACT)
        message(STATUS "Lilolify: Tesseract and Leptonica found. Local OCR support enabled.")
    else()
        set(LILOLIFY_WITH_TESSERACT OFF)
        message(WARNING "Lilolify: Tesseract/Leptonica NOT found. Local OCR support will be mock-only.")
    endif()
endif()

# Find or fetch nlohmann_json
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL            https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(nlohmann_json)

# Find or fetch CURL with Windows Native Schannel SSL support
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY OFF CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    curl
    URL            https://github.com/curl/curl/archive/refs/tags/curl-8_4_0.zip
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(curl)

# Find or fetch SQLite Amalgamation
FetchContent_Declare(
    sqlite
    URL            https://github.com/azadkuh/sqlite-amalgamation/archive/refs/heads/master.zip
)
FetchContent_GetProperties(sqlite)
if(NOT sqlite_POPULATED)
    FetchContent_Populate(sqlite)
    add_library(sqlite3 STATIC "${sqlite_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlite3 PUBLIC "${sqlite_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(sqlite3 PRIVATE /W0)
    else()
        target_compile_options(sqlite3 PRIVATE -w)
    endif()
endif()

# Find or fetch GLFW
FetchContent_Declare(
    glfw
    URL            https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Find or fetch Dear ImGui
FetchContent_Declare(
    imgui
    URL            https://github.com/ocornut/imgui/archive/refs/tags/v1.90.9.zip
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# ---- CMake Modules ----
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# ---- Output Directories ----
# Place all build artifacts in predictable locations
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---- Source Layers ----
# Added in dependency order: core first (no deps), then layers that depend on it.
add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(src/infra)
add_subdirectory(src/ai)
add_subdirectory(src/ui)

# ---- Command Line Interface (CLI Executable) ----
add_executable(lilolify_cli src/main.cpp)
target_link_libraries(lilolify_cli
    PRIVATE
        lilolify_app
        lilolify_infra
        lilolify_ai
        lilolify_core
        lilolify::warnings
)

# ---- Testing ----
if(LILOLIFY_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Build Summary ----
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Lilolify v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "  C++ Standard  : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler      : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Testing       : ${LILOLIFY_ENABLE_TESTING}")
message(STATUS "  Tesseract OCR : ${LILOLIFY_WITH_TESSERACT}")
message(STATUS "  Warnings=Err  : ${LILOLIFY_ENABLE_WARNINGS_AS_ERRORS}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
