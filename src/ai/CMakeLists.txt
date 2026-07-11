# ============================================================================
# Lilolify — AI Service Layer
# ============================================================================
# AI provider abstraction, concrete provider implementations, OCR, HTTP client.
# Depends on: core (domain layer)
# External deps: libcurl, Tesseract (added in later phases)
# ============================================================================

add_library(lilolify_ai STATIC)
add_library(lilolify::ai ALIAS lilolify_ai)

target_sources(lilolify_ai
    PRIVATE
        src/providers/openai_provider.cpp
        src/providers/gemini_provider.cpp
        src/providers/claude_provider.cpp
        src/providers/ai_provider_factory.cpp
)

target_include_directories(lilolify_ai
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# AI layer depends on core — implements IVisionProvider and IOcrEngine interfaces
target_link_libraries(lilolify_ai
    PUBLIC
        lilolify::core
    PRIVATE
        lilolify::warnings
        nlohmann_json::nlohmann_json
)

target_compile_features(lilolify_ai PUBLIC cxx_std_20)
