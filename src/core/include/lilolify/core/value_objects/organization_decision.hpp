// ============================================================================
// Lilolify — OrganizationDecision Value Object
// ============================================================================
// Data structure encapsulating the file organization recommendation from the AI.
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace lilolify::core {

/// Results of the AI reasoning engine's file analysis.
struct OrganizationDecision {
    /// Categorized class (e.g. "Invoice", "Receipt", "ID_Card", "Document").
    std::string category;

    /// Suggested clean filename (e.g. "2026-07-07_DeepMind_Invoice.pdf").
    std::string suggested_filename;

    /// Recommended relative destination path (e.g. "Invoices/2026/file.pdf").
    std::string suggested_path;

    /// Descriptive tags for search indexing (e.g. ["tax", "work"]).
    std::vector<std::string> tags;

    /// Extracted document key-value attributes (e.g. {"vendor": "Google", "total": "42"}).
    std::unordered_map<std::string, std::string> attributes;

    /// Confidence score of this choice (0.0 to 1.0).
    double confidence = 0.0;

    /// AI reasoning behind this classification choice.
    std::string reasoning;
};

}  // namespace lilolify::core
