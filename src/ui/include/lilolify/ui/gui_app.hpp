// ============================================================================
// Lilolify — Desktop GUI Application Controller
// ============================================================================
// Manages the ImGui context, configures custom high-contrast dark themes,
// captures thread-safe callback events, and renders modular layouts.
// ============================================================================

#pragma once

#include <lilolify/app/app_shell.hpp>

#include <string>
#include <mutex>
#include <vector>

// Forward declare GLFW window structure to avoid cluttering includes
struct GLFWwindow;

namespace lilolify::ui {

/// Graphical Controller class handling rendering cycles and user inputs routing.
class GuiApp {
public:
    GuiApp() noexcept;
    ~GuiApp();

    // Disable copy/move
    GuiApp(const GuiApp&) = delete;
    GuiApp& operator=(const GuiApp&) = delete;
    GuiApp(GuiApp&&) = delete;
    GuiApp& operator=(GuiApp&&) = delete;

    /// Initialize window frame buffers, setups layouts, and enters main refresh loop.
    ///
    /// @return Exit status code (0 on clean quit, 1 on initialization failures).
    [[nodiscard]] int run();

private:
    [[nodiscard]] bool init_window();
    void render_ui();
    void shutdown_window();

    // Panel Rendering Layers
    void draw_dashboard_panel();
    void draw_settings_panel();
    void draw_history_panel();

    // Themes
    void apply_dark_theme();

    // Facade orchestrator shell
    app::AppShell app_shell_;
    GLFWwindow* window_{nullptr};

    // Thread-safe state replication buffers
    mutable std::mutex progress_mutex_;
    app::PipelineProgress progress_snapshot_;
    std::string active_transaction_id_;
    bool is_job_running_{false};

    // Dashboard text input buffers
    char scan_root_buf_[512]{""};
    char dest_root_buf_[512]{""};
    int action_type_radio_{0}; // 0 = Move, 1 = Copy

    // Settings panel input buffers
    int ai_provider_idx_{0}; // 0 = openai, 1 = gemini, 2 = claude
    char api_key_buf_[256]{""};
    char model_buf_[128]{""};
    float temperature_{0.1f};
    int max_depth_{50};

    // Reversion parameters
    char undo_tx_id_buf_[128]{""};

    // Error/Success status message banners
    std::string pipeline_status_banner_;
    std::string settings_status_banner_;
    std::string undo_status_banner_;
};

}  // namespace lilolify::ui
