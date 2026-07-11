# ============================================================================
# Lilolify — Infrastructure Layer
# ============================================================================
# Concrete implementations of domain interfaces: filesystem, MIME detection.
# Depends on: core (domain layer)
# ============================================================================

add_library(lilolify_infra STATIC)
add_library(lilolify::infra ALIAS lilolify_infra)

target_sources(lilolify_infra
    PRIVATE
        src/mime_detector.cpp
        src/filesystem_scanner.cpp
        src/crypto/sha256.cpp
        src/crypto/sha256_calculator.cpp
        src/metadata/image_metadata_extractor.cpp
        src/metadata/pdf_metadata_extractor.cpp
        src/metadata/text_metadata_extractor.cpp
        src/ocr/tesseract_ocr_engine.cpp
        src/network/curl_http_client.cpp
        src/organization/file_organizer.cpp
        src/database/sqlite_database.cpp
)

target_include_directories(lilolify_infra
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Infrastructure depends on core — implements the domain interfaces (ports)
target_link_libraries(lilolify_infra
    PUBLIC
        lilolify::core
    PRIVATE
        lilolify::warnings
        libcurl
        sqlite3
)

if(LILOLIFY_WITH_TESSERACT)
    target_link_libraries(lilolify_infra
        PRIVATE
            Tesseract::libtesseract
            Leptonica::leptonica
    )
endif()

target_compile_features(lilolify_infra PUBLIC cxx_std_20)
