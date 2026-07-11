# ============================================================================
# Lilolify — Test Configuration
# ============================================================================
# Uses GoogleTest fetched via FetchContent (falls back to vcpkg if available).
# Test targets follow the convention: lilolify_test_<layer>_<name>
# ============================================================================

include(FetchContent)

# Fetch GoogleTest — CMake will download and build it automatically.
# This ensures tests work even without vcpkg installing gtest.
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    FIND_PACKAGE_ARGS  # Use find_package first (vcpkg), fall back to fetch
)

# Prevent GoogleTest from overriding our compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ============================================================================
# Helper function to define test targets consistently
# ============================================================================
#
# Usage:
#   lilolify_add_test(
#       NAME test_result
#       SOURCES unit/core/test_result.cpp
#       LIBRARIES lilolify::core
#   )
#
function(lilolify_add_test)
    cmake_parse_arguments(
        TEST                     # prefix
        ""                       # boolean args
        "NAME"                   # single-value args
        "SOURCES;LIBRARIES"      # multi-value args
        ${ARGN}
    )

    set(TARGET_NAME "lilolify_test_${TEST_NAME}")

    add_executable(${TARGET_NAME} ${TEST_SOURCES})

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            GTest::gtest_main
            GTest::gmock
            lilolify::warnings
            ${TEST_LIBRARIES}
    )

    # Add test directly to avoid WDAC blocks during compile and script include bugs
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${TARGET_NAME}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "unit")
endfunction()

# ============================================================================
# Core Layer Tests
# ============================================================================

lilolify_add_test(
    NAME core_result
    SOURCES unit/core/test_result.cpp
    LIBRARIES lilolify::core
)

lilolify_add_test(
    NAME core_error
    SOURCES unit/core/test_error.cpp
    LIBRARIES lilolify::core
)

lilolify_add_test(
    NAME core_file_entry
    SOURCES unit/core/test_file_entry.cpp
    LIBRARIES lilolify::core
)

lilolify_add_test(
    NAME core_scan_options
    SOURCES unit/core/test_scan_options.cpp
    LIBRARIES lilolify::core
)

lilolify_add_test(
    NAME core_metadata_engine
    SOURCES unit/core/test_metadata_engine.cpp
    LIBRARIES lilolify::core
)

# ============================================================================
# Infrastructure Layer Tests
# ============================================================================

lilolify_add_test(
    NAME infra_mime_detector
    SOURCES unit/infra/test_mime_detector.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_filesystem_scanner
    SOURCES unit/infra/test_filesystem_scanner.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_sha256
    SOURCES unit/infra/test_sha256.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_image_metadata_extractor
    SOURCES unit/infra/test_image_metadata_extractor.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_pdf_metadata_extractor
    SOURCES unit/infra/test_pdf_metadata_extractor.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_text_metadata_extractor
    SOURCES unit/infra/test_text_metadata_extractor.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_ocr_engine
    SOURCES unit/infra/test_ocr_engine.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME ai_providers
    SOURCES unit/ai/test_ai_providers.cpp
    LIBRARIES lilolify::ai nlohmann_json::nlohmann_json
)

lilolify_add_test(
    NAME infra_curl_http_client
    SOURCES unit/infra/test_curl_http_client.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME core_ai_reasoning_engine
    SOURCES unit/core/test_ai_reasoning_engine.cpp
    LIBRARIES lilolify::core nlohmann_json::nlohmann_json
)

lilolify_add_test(
    NAME infra_file_organizer
    SOURCES unit/infra/test_file_organizer.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME infra_sqlite_database
    SOURCES unit/infra/test_sqlite_database.cpp
    LIBRARIES lilolify::infra
)

lilolify_add_test(
    NAME app_pipeline_coordinator
    SOURCES unit/app/test_pipeline_coordinator.cpp
    LIBRARIES lilolify::app lilolify::infra
)

lilolify_add_test(
    NAME app_configuration_manager
    SOURCES unit/app/test_configuration_manager.cpp
    LIBRARIES lilolify::app nlohmann_json::nlohmann_json
)

lilolify_add_test(
    NAME app_shell
    SOURCES unit/app/test_app_shell.cpp
    LIBRARIES lilolify::app lilolify::infra
)


