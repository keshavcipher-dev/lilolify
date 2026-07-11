# ============================================================================
# Lilolify — Application Layer
# ============================================================================
# Use cases, orchestrators, and DTOs.
# Depends on: core (domain layer)
# Depended on by: ui (presentation layer)
# ============================================================================

add_library(lilolify_app STATIC)
add_library(lilolify::app ALIAS lilolify_app)

target_sources(lilolify_app
    PRIVATE
        src/app_placeholder.cpp
        src/pipeline_coordinator.cpp
        src/configuration_manager.cpp
        src/app_shell.cpp
)

target_include_directories(lilolify_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Application layer depends on core (domain) and uses infrastructure/AI adapters
target_link_libraries(lilolify_app
    PUBLIC
        lilolify::core
    PRIVATE
        lilolify::warnings
        lilolify::infra
        lilolify::ai
)

target_compile_features(lilolify_app PUBLIC cxx_std_20)
