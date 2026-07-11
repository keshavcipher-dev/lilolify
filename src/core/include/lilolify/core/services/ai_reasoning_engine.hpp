// ============================================================================
// Lilolify — AiReasoningEngine
// ============================================================================
// Orchestrator service running OCR, formatting prompts, executing remote
// queries, and parsing structured JSON metadata into OrganizationDecisions.
// ============================================================================

#pragma once

#include <lilolify/core/entities/file_entry.hpp>
#include <lilolify/core/interfaces/i_ai_provider.hpp>
#include <lilolify/core/interfaces/i_ocr_engine.hpp>
#include <lilolify/core/value_objects/organization_decision.hpp>

namespace lilolify::core {

/// Coordinates file content analysis, OCR execution, prompt formatting,
/// and AI queries to produce file organization recommendations.
class AiReasoningEngine {
public:
    /// Construct reasoning engine.
    ///
    /// @param ai_provider  The configured AI model service backend.
    /// @param ocr_engine   The offline printed character scanner service.
    AiReasoningEngine(IAiProvider& ai_provider, IOcrEngine& ocr_engine) noexcept;
    
    ~AiReasoningEngine() = default;

    // Disable copy/move
    AiReasoningEngine(const AiReasoningEngine&) = delete;
    AiReasoningEngine& operator=(const AiReasoningEngine&) = delete;
    AiReasoningEngine(AiReasoningEngine&&) = delete;
    AiReasoningEngine& operator=(AiReasoningEngine&&) = delete;

    /// Process, analyze, and categorize a file entry.
    /// Returns the recommended organization decision structure.
    [[nodiscard]] Result<OrganizationDecision, Error> analyze(
        const FileEntry& entry);

private:
    [[nodiscard]] std::string build_system_prompt() const;
    [[nodiscard]] std::string build_user_prompt(
        const FileEntry& entry,
        const std::string& extra_text) const;

    [[nodiscard]] Result<OrganizationDecision, Error> parse_ai_json(
        const std::string& ai_json_text) const;

    [[nodiscard]] OrganizationDecision make_fallback_decision(
        const FileEntry& entry,
        const std::string& reasoning_reason,
        const std::string& extracted_text = "") const;

    IAiProvider& ai_provider_;
    IOcrEngine& ocr_engine_;
};

}  // namespace lilolify::core
