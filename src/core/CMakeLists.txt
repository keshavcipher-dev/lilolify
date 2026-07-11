# ============================================================================
# Lilolify — Core Library (Domain Layer)
# ============================================================================
# This is the innermost layer of Clean Architecture.
#
# RULES:
#   1. This library has ZERO external dependencies (no Qt, no SQLite, no curl)
#   2. It defines entities, value objects, domain services, and interfaces (ports)
#   3. All other layers depend on this; this depends on nothing
#   4. Only pure C++20 Standard Library is allowed here
# ============================================================================

add_library(lilolify_core STATIC)
add_library(lilolify::core ALIAS lilolify_core)

# --- Source Files ---
target_sources(lilolify_core
    PRIVATE
        src/core_placeholder.cpp
        src/metadata_engine.cpp
        src/services/ai_reasoning_engine.cpp
)

# --- Include Directories ---
# PUBLIC: consumers of lilolify::core inherit these include paths
target_include_directories(lilolify_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# --- Compiler Warnings ---
target_link_libraries(lilolify_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        lilolify::warnings
)

# --- Compile Features ---
# Explicit C++20 requirement for this target
target_compile_features(lilolify_core PUBLIC cxx_std_20)
