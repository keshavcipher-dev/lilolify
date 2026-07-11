// ============================================================================
// Lilolify — Desktop GUI Application Controller Implementation
// ============================================================================

#include <lilolify/ui/gui_app.hpp>
#include <lilolify/infra/database/sqlite_database.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <shobjidl.h>
#endif

#include <filesystem>
#include <iostream>
#include <sstream>

namespace lilolify::ui {

using namespace lilolify::app;

namespace {

// ============================================================================
// Windows COM File & Folder Dialogues
// ============================================================================

#ifdef _WIN32
std::string open_folder_dialog(HWND hwnd_parent) {
    std::string result;
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, 
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        FILEOPENDIALOGOPTIONS options;
        if (SUCCEEDED(pFileOpen->GetOptions(&options))) {
            pFileOpen->SetOptions(options | FOS_PICKFOLDERS);
        }
        if (SUCCEEDED(pFileOpen->Show(hwnd_parent))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, nullptr, 0, nullptr, nullptr);
                    std::string str(size_needed - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &str[0], size_needed, nullptr, nullptr);
                    result = std::move(str);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return result;
}

std::string open_file_dialog(HWND hwnd_parent) {
    std::string result;
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, 
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC fileTypes[] = {
            { L"All Supported Files (*.jpg;*.png;*.pdf;*.txt)", L"*.jpg;*.jpeg;*.png;*.webp;*.pdf;*.txt" },
            { L"Images (*.jpg; *.png; *.webp)", L"*.jpg;*.jpeg;*.png;*.webp" },
            { L"PDF Documents (*.pdf)", L"*.pdf" },
            { L"All Files (*.*)", L"*.*" }
        };
        pFileOpen->SetFileTypes(4, fileTypes);
        if (SUCCEEDED(pFileOpen->Show(hwnd_parent))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, nullptr, 0, nullptr, nullptr);
                    std::string str(size_needed - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &str[0], size_needed, nullptr, nullptr);
                    result = std::move(str);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return result;
}
#else
std::string open_folder_dialog(void*) { return ""; }
std::string open_file_dialog(void*) { return ""; }
#endif

}  // namespace

// ============================================================================
// Constructors / Destructors
// ============================================================================

GuiApp::GuiApp() noexcept
    : window_(nullptr), is_job_running_(false) {}

GuiApp::~GuiApp() {
    shutdown_window();
}

// ============================================================================
// Main Application Event Loop
// ============================================================================

int GuiApp::run() {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    // 1. Initialize GLFW and create Desktop Window
    if (!init_window()) {
        std::cerr << "Failed to initialize application window.\n";
        return 1;
    }

    // 2. Initialize AppShell Facade
    auto init_res = app_shell_.initialize();
    if (init_res.has_error()) {
        pipeline_status_banner_ = "AppShell Init Error: " + std::string(init_res.error().message());
    } else {
        // Pre-fill inputs from settings loaded on boot
        auto s = app_shell_.settings();
        
        // Match active provider index
        if (s.ai.active_provider == "gemini") {
            ai_provider_idx_ = 1;
        } else if (s.ai.active_provider == "claude") {
            ai_provider_idx_ = 2;
        } else if (s.ai.active_provider == "openrouter") {
            ai_provider_idx_ = 3;
        } else {
            ai_provider_idx_ = 0;
        }

        // De-obfuscate API Key for displaying in settings inputs
        ConfigurationManager config;
        std::string dec = config.deobfuscate_key(s.ai.api_key);
        strncpy_s(api_key_buf_, dec.c_str(), sizeof(api_key_buf_) - 1);
        
        strncpy_s(model_buf_, s.ai.model.c_str(), sizeof(model_buf_) - 1);
        temperature_ = static_cast<float>(s.ai.temperature);
        max_depth_ = static_cast<int>(s.scan.max_depth);
    }

    // 3. Apply Dark Theme styles
    apply_dark_theme();

    // 4. Frame refresh loop
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Start Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render controls panels
        render_ui();

        // OpenGL rendering cycle
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.08f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }

    // 5. Cleanup
    (void)app_shell_.shutdown();
    shutdown_window();

#ifdef _WIN32
    CoUninitialize();
#endif

    return 0;
}

// ============================================================================
// GLFW OpenGL Window Lifecycle
// ============================================================================

bool GuiApp::init_window() {
    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create desktop frame
    window_ = glfwCreateWindow(1150, 780, "Lilolify — Intelligent File Organizer Dashboard", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable VSync

    // Initialize ImGui bindings
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Load modern Segoe UI system font for high-end typography
    ImGuiIO& io = ImGui::GetIO();
#ifdef _WIN32
    std::string font_path = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (std::filesystem::exists(font_path)) {
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), 18.5f);
    } else {
        io.Fonts->AddFontDefault();
    }
#else
    io.Fonts->AddFontDefault();
#endif

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    return true;
}

void GuiApp::shutdown_window() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

// ============================================================================
// UI Canvas Rendering Coordinates
// ============================================================================

void GuiApp::render_ui() {
    // Fill the entire window client space
    int w, h;
    glfwGetWindowSize(window_, &w, &h);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));

    ImGui::Begin("MainPanel", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Draw glowing top banner
    ImGui::TextColored(ImVec4(0.00f, 0.95f, 1.00f, 1.00f), " LILOLIFY  |  Intelligent AI Digital File Organization System");
    ImGui::Separator();
    ImGui::Spacing();

    // Create tab panels layout with styling parameters
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
    bool began = ImGui::BeginTabBar("ControlTabs", ImGuiTabBarFlags_None);
    ImGui::PopStyleVar();

    if (began) {
        if (ImGui::BeginTabItem("Dashboard (Organize Files)")) {
            draw_dashboard_panel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AI & System Configurations")) {
            draw_settings_panel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(" Relocation History Ledger ")) {
            draw_history_panel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ============================================================================
// PANEL DRAW: DASHBOARD
// ============================================================================

void GuiApp::draw_dashboard_panel() {
    ImGui::Spacing();

    // Check if API key is not configured
    auto s = app_shell_.settings();
    ConfigurationManager config;
    std::string key = config.deobfuscate_key(s.ai.api_key);
    bool is_fake_key = key.empty() || key == "AIzaSyB-abcdef1234567890";

    if (is_fake_key) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.10f, 0.10f, 1.00f));
        ImGui::BeginChild("ApiWarningBanner", ImVec2(0, 42), true);
        ImGui::TextColored(ImVec4(1.00f, 0.35f, 0.35f, 1.00f), "  [!] AI VISION OFFLINE: Please configure a valid API Key in settings to enable direct image content/visual analysis.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    
    // Main UI Columns layout (Left: Folder scan; Right: Single file upload)
    float window_width = ImGui::GetContentRegionAvail().x;
    float col_w = (window_width - 30.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.10f, 0.14f, 0.50f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));

    ImGui::BeginChild("FolderPanel", ImVec2(col_w, 290), true, ImGuiWindowFlags_None);
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.00f, 0.95f, 1.00f, 1.00f), " 📂 Option A: Batch Folder Reorganization");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Source Directory:");
        ImGui::InputText("##src_dir", scan_root_buf_, sizeof(scan_root_buf_));
        ImGui::SameLine();
        if (ImGui::Button("Browse##src_btn")) {
#ifdef _WIN32
            HWND hwnd = glfwGetWin32Window(window_);
            std::string selected = open_folder_dialog(hwnd);
            if (!selected.empty()) {
                strncpy_s(scan_root_buf_, selected.c_str(), sizeof(scan_root_buf_) - 1);
            }
#endif
        }

        ImGui::Spacing();
        ImGui::Text("Destination Base Directory:");
        ImGui::InputText("##dest_dir", dest_root_buf_, sizeof(dest_root_buf_));
        ImGui::SameLine();
        if (ImGui::Button("Browse##dest_btn")) {
#ifdef _WIN32
            HWND hwnd = glfwGetWin32Window(window_);
            std::string selected = open_folder_dialog(hwnd);
            if (!selected.empty()) {
                strncpy_s(dest_root_buf_, selected.c_str(), sizeof(dest_root_buf_) - 1);
            }
#endif
        }

        ImGui::Spacing();
        ImGui::Text("Action Strategy:");
        ImGui::RadioButton("Move Files", &action_type_radio_, 0); ImGui::SameLine();
        ImGui::RadioButton("Copy Files", &action_type_radio_, 1);
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 30);

    ImGui::BeginChild("FilePanel", ImVec2(col_w, 290), true, ImGuiWindowFlags_None);
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.98f, 0.00f, 0.75f, 1.00f), " ⚡ Option B: Direct File Upload");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Target File / Image to Upload:");
        ImGui::InputText("##file_dir", scan_root_buf_, sizeof(scan_root_buf_));
        ImGui::SameLine();
        if (ImGui::Button("Browse File##file_btn")) {
#ifdef _WIN32
            HWND hwnd = glfwGetWin32Window(window_);
            std::string selected = open_file_dialog(hwnd);
            if (!selected.empty()) {
                strncpy_s(scan_root_buf_, selected.c_str(), sizeof(scan_root_buf_) - 1);
            }
#endif
        }

        ImGui::Spacing();
        ImGui::Text("Destination Base Directory:");
        ImGui::InputText("##dest_dir_file", dest_root_buf_, sizeof(dest_root_buf_));
        ImGui::SameLine();
        if (ImGui::Button("Browse##dest_btn_file")) {
#ifdef _WIN32
            HWND hwnd = glfwGetWin32Window(window_);
            std::string selected = open_folder_dialog(hwnd);
            if (!selected.empty()) {
                strncpy_s(dest_root_buf_, selected.c_str(), sizeof(dest_root_buf_) - 1);
            }
#endif
        }

        ImGui::Spacing();
        ImGui::Text("Action Strategy:");
        ImGui::RadioButton("Move File", &action_type_radio_, 0); ImGui::SameLine();
        ImGui::RadioButton("Copy File", &action_type_radio_, 1);
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Replicate current background thread progress if running
    is_job_running_ = app_shell_.is_job_running();
    if (is_job_running_) {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        progress_snapshot_ = app_shell_.progress();
        active_transaction_id_ = progress_snapshot_.status_message;
    }

    // Process Buttons Layout
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.65f, 0.80f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.80f, 1.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.50f, 0.65f, 1.00f));
    
    if (!is_job_running_) {
        if (ImGui::Button("Execute Organization Pipeline", ImVec2(280, 45))) {
            pipeline_status_banner_ = "";
            active_transaction_id_ = "";

            // Validate Paths
            if (!std::filesystem::exists(scan_root_buf_)) {
                pipeline_status_banner_ = "Error: Specified Source Path does not exist. Please use the Browse buttons to select a valid file or folder.";
            } else {
                // Automatically default destination path if left empty to prevent mandatory requirements
                if (strlen(dest_root_buf_) == 0) {
                    std::filesystem::path src_path(scan_root_buf_);
                    std::string auto_dest;
                    if (std::filesystem::is_regular_file(src_path)) {
                        auto_dest = src_path.parent_path().string();
                    } else {
                        auto_dest = (src_path.parent_path() / "Lilolify_Organized").string();
                    }
                    strncpy_s(dest_root_buf_, auto_dest.c_str(), sizeof(dest_root_buf_) - 1);
                }

                core::FileActionType act = action_type_radio_ == 1 ? core::FileActionType::kCopy : core::FileActionType::kMove;
                
                auto callback = [this](const app::PipelineProgress& p) {
                    std::lock_guard<std::mutex> lock(progress_mutex_);
                    progress_snapshot_ = p;
                };

                auto start_res = app_shell_.start_job(scan_root_buf_, dest_root_buf_, act, callback);
                if (start_res.has_error()) {
                    pipeline_status_banner_ = "Failed to launch pipeline: " + std::string(start_res.error().message());
                }
            }
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.15f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.25f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.10f, 0.10f, 1.00f));
        if (ImGui::Button("Force Stop Active Pipeline Job", ImVec2(280, 45))) {
            app_shell_.cancel_job();
        }
        ImGui::PopStyleColor(3);
    }
    
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Render Progress indicators
    app::PipelineState state_val = app::PipelineState::kIdle;
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        state_val = progress_snapshot_.state;
    }

    if (is_job_running_ || state_val != app::PipelineState::kIdle) {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        double percentage = progress_snapshot_.total_files > 0 
            ? (100.0 * progress_snapshot_.processed_files / progress_snapshot_.total_files) 
            : 0.0;

        std::string state_str = "Idle";
        ImVec4 state_col = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

        switch (progress_snapshot_.state) {
            case app::PipelineState::kScanning:
                state_str = "DISCOVERING: Traversing pathways and resolving MIME magic bytes...";
                state_col = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
                break;
            case app::PipelineState::kProcessing:
                state_str = "ANALYZING: Running local OCR scanners and AI reasoning providers...";
                state_col = ImVec4(1.00f, 0.85f, 0.00f, 1.00f);
                break;
            case app::PipelineState::kReorganizing:
                state_str = "EXECUTING: Copying/moving categorized files to destinations...";
                state_col = ImVec4(1.00f, 0.55f, 0.00f, 1.00f);
                break;
            case app::PipelineState::kCompleted:
                state_str = "SUCCESS: Reorganization completed successfully!";
                state_col = ImVec4(0.00f, 0.90f, 0.00f, 1.00f);
                break;
            case app::PipelineState::kCancelled:
                state_str = "CANCELLED: Pipeline execution aborted by user.";
                state_col = ImVec4(0.90f, 0.15f, 0.15f, 1.00f);
                break;
            case app::PipelineState::kFailed:
                state_str = "FAILED: Error encountered during pipeline run.";
                state_col = ImVec4(0.90f, 0.15f, 0.15f, 1.00f);
                break;
            default:
                break;
        }

        ImGui::Text("Active Status: "); ImGui::SameLine();
        ImGui::TextColored(state_col, "%s", state_str.c_str());

        std::string progress_text = std::to_string(progress_snapshot_.processed_files) + " of " + 
                                    std::to_string(progress_snapshot_.total_files) + " assets classified";
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.00f, 0.90f, 0.75f, 1.00f));
        ImGui::ProgressBar(static_cast<float>(percentage / 100.0), ImVec2(-1.0f, 28.0f), progress_text.c_str());
        ImGui::PopStyleColor();

        if (!progress_snapshot_.current_file_path.empty()) {
            ImGui::TextColored(ImVec4(0.60f, 0.70f, 0.80f, 1.00f), "Processing: %s", progress_snapshot_.current_file_path.c_str());
        }
    }

    if (!pipeline_status_banner_.empty()) {
        ImGui::Spacing();
        ImVec4 banner_col = (pipeline_status_banner_.find("Error") != std::string::npos || 
                             pipeline_status_banner_.find("Failed") != std::string::npos) 
            ? ImVec4(0.90f, 0.15f, 0.15f, 1.00f) 
            : ImVec4(0.00f, 0.90f, 0.00f, 1.00f);
        ImGui::TextColored(banner_col, "%s", pipeline_status_banner_.c_str());
    }
}

// ============================================================================
// PANEL DRAW: SETTINGS
// ============================================================================

void GuiApp::draw_settings_panel() {
    ImGui::Spacing();
    
    ImGui::BeginChild("SettingsInnerPanel", ImVec2(750, 360), true);
    {
        ImGui::TextColored(ImVec4(0.00f, 0.95f, 1.00f, 1.00f), "System Configurations & AI Model Bindings");
        ImGui::Separator();
        ImGui::Spacing();

        const char* providers[] = { 
            "OpenAI Backend API (GPT-4o)", 
            "Google Gemini Backend API (1.5 Flash)", 
            "Anthropic Claude Backend API (3.5 Sonnet)",
            "OpenRouter Gateway API (Gemini/Llama)"
        };
        ImGui::Combo("AI Provider Type", &ai_provider_idx_, providers, IM_ARRAYSIZE(providers));

        ImGui::InputText("AI Model Identifier", model_buf_, sizeof(model_buf_));
        ImGui::InputText("API Authorization Token", api_key_buf_, sizeof(api_key_buf_), ImGuiInputTextFlags_Password);

        ImGui::SliderFloat("Model Temperature", &temperature_, 0.0f, 1.0f, "%.2f");
        ImGui::SliderInt("Scan Depth Limit", &max_depth_, 1, 100);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save & Apply Configurations", ImVec2(240, 40))) {
            settings_status_banner_ = "";

            AppSettings s = app_shell_.settings();
            
            if (ai_provider_idx_ == 1) {
                s.ai.active_provider = "gemini";
            } else if (ai_provider_idx_ == 2) {
                s.ai.active_provider = "claude";
            } else if (ai_provider_idx_ == 3) {
                s.ai.active_provider = "openrouter";
            } else {
                s.ai.active_provider = "openai";
            }

            s.ai.model = model_buf_;
            s.ai.temperature = temperature_;
            s.scan.max_depth = static_cast<std::uint32_t>(max_depth_);

            ConfigurationManager config;
            s.ai.api_key = config.obfuscate_key(api_key_buf_);

            auto save_res = app_shell_.update_settings(s);
            if (save_res.has_error()) {
                settings_status_banner_ = "Failed to update configurations: " + std::string(save_res.error().message());
            } else {
                settings_status_banner_ = "Configurations successfully saved and applied!";
            }
        }
    }
    ImGui::EndChild();

    if (!settings_status_banner_.empty()) {
        ImGui::Spacing();
        ImVec4 banner_col = (settings_status_banner_.find("saved") != std::string::npos) 
            ? ImVec4(0.00f, 0.90f, 0.00f, 1.00f) 
            : ImVec4(0.90f, 0.15f, 0.15f, 1.00f);
        ImGui::TextColored(banner_col, "%s", settings_status_banner_.c_str());
    }
}

// ============================================================================
// PANEL DRAW: HISTORY LEDGER
// ============================================================================

void GuiApp::draw_history_panel() {
    ImGui::Spacing();

    infra::SqliteDatabase db;
    auto open_res = db.open(app_shell_.settings().database_path);
    
    std::vector<core::FileActionRecord> records;
    if (open_res.has_value()) {
        auto history_res = db.get_all_history();
        if (history_res.has_value()) {
            records = std::move(history_res).value();
        }
    }

    if (records.empty()) {
        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.65f, 1.00f), "No file actions logged in the Relational Database cache yet.");
        return;
    }

    if (!undo_status_banner_.empty()) {
        ImGui::TextColored(ImVec4(0.00f, 0.90f, 0.00f, 1.00f), "%s", undo_status_banner_.c_str());
        ImGui::Spacing();
    }

    // Scrollable Table layout
    if (ImGui::BeginChild("HistoryScroll", ImVec2(0, 0), true)) {
        if (ImGui::BeginTable("HistoryTable", 4, 
                             ImGuiTableFlags_Borders | 
                             ImGuiTableFlags_RowBg | 
                             ImGuiTableFlags_Resizable)) {
            
            ImGui::TableSetupColumn("Transaction Session ID", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("Original File Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Relocated Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Action Control", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableHeadersRow();

            std::string last_tx;
            for (const auto& rec : records) {
                ImGui::TableNextRow();
                
                // Column 1: Session ID
                ImGui::TableNextColumn();
                if (rec.transaction_id != last_tx) {
                    ImGui::TextColored(ImVec4(0.00f, 0.95f, 1.00f, 1.00f), "%s", rec.transaction_id.c_str());
                }

                // Column 2: Original Path
                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.original_path.filename().string().c_str());

                // Column 3: Executed Target Path
                ImGui::TableNextColumn();
                
                // Color-code categories tags dynamically based on destination folders path!
                std::string path_str = rec.executed_path.string();
                ImVec4 tag_color = ImVec4(0.80f, 0.80f, 0.80f, 1.00f); // Default gray

                if (path_str.find("Schedules") != std::string::npos) {
                    tag_color = ImVec4(1.00f, 0.75f, 0.10f, 1.00f); // Yellow-Orange for Schedules
                } else if (path_str.find("Financial") != std::string::npos) {
                    tag_color = ImVec4(0.00f, 0.90f, 0.20f, 1.00f); // Green for Financial
                } else if (path_str.find("Identification") != std::string::npos) {
                    tag_color = ImVec4(0.00f, 0.85f, 1.00f, 1.00f); // Cyan for Identification
                } else if (path_str.find("Credentials") != std::string::npos) {
                    tag_color = ImVec4(0.55f, 0.40f, 0.90f, 1.00f); // Slate Blue for Credentials
                } else if (path_str.find("Notes") != std::string::npos) {
                    tag_color = ImVec4(0.75f, 0.40f, 0.75f, 1.00f); // Pink-Purple for Notes
                } else if (path_str.find("Academic") != std::string::npos) {
                    tag_color = ImVec4(1.00f, 0.60f, 0.20f, 1.00f); // Peach/Amber for Academic
                } else if (path_str.find("Medical") != std::string::npos) {
                    tag_color = ImVec4(0.95f, 0.30f, 0.30f, 1.00f); // Red for Medical
                } else if (path_str.find("Legal") != std::string::npos) {
                    tag_color = ImVec4(0.85f, 0.65f, 0.20f, 1.00f); // Amber for Legal
                } else if (path_str.find("Media") != std::string::npos) {
                    tag_color = ImVec4(1.00f, 0.35f, 0.65f, 1.00f); // Pink-Magenta for Media
                }

                ImGui::TextColored(tag_color, "%s", path_str.c_str());

                // Column 4: Revert Button
                ImGui::TableNextColumn();
                if (rec.transaction_id != last_tx) {
                    last_tx = rec.transaction_id;
                    
                    std::string btn_lbl = "Undo Session##" + last_tx;
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.15f, 0.15f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.10f, 0.10f, 1.00f));
                    
                    if (ImGui::Button(btn_lbl.c_str())) {
                        undo_status_banner_ = "";
                        auto undo_res = app_shell_.undo_transaction(last_tx);
                        if (undo_res.has_error()) {
                            undo_status_banner_ = "Reversion failed: " + std::string(undo_res.error().message());
                        } else {
                            undo_status_banner_ = "Transaction session " + last_tx + " reverted successfully!";
                        }
                    }
                    ImGui::PopStyleColor(3);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

// ============================================================================
// STYLES: CUSTOM DYNAMIC GLOW THEME
// ============================================================================

void GuiApp::apply_dark_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 8.0f;
    
    style.FramePadding = ImVec2(14, 10);
    style.ItemSpacing = ImVec2(12, 12);
    style.WindowPadding = ImVec2(20, 20);
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 16.0f;

    // Theme: Obsidian cyberpunk with neon glowing accents
    colors[ImGuiCol_Text]                   = ImVec4(0.96f, 0.97f, 0.99f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.55f, 0.64f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.06f, 0.08f, 1.00f); // Obsidian Dark
    colors[ImGuiCol_ChildBg]                = ImVec4(0.09f, 0.10f, 0.14f, 0.75f); // Translucent Dark Glass
    colors[ImGuiCol_PopupBg]                = ImVec4(0.07f, 0.08f, 0.11f, 0.95f);
    colors[ImGuiCol_Border]                 = ImVec4(0.18f, 0.22f, 0.33f, 0.80f); // Sleek Border
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    colors[ImGuiCol_FrameBg]                = ImVec4(0.13f, 0.15f, 0.22f, 0.70f); // Glass input
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.22f, 0.33f, 0.85f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.00f, 0.95f, 1.00f, 0.20f);
    
    colors[ImGuiCol_TitleBg]                = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.00f, 0.95f, 1.00f, 0.15f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    
    // Cyan Accent buttons with micro-glow
    colors[ImGuiCol_Button]                 = ImVec4(0.00f, 0.55f, 0.70f, 0.85f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.95f, 1.00f, 1.00f); // Electric Cyan
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.60f, 0.70f, 1.00f);
    
    colors[ImGuiCol_Header]                 = ImVec4(0.13f, 0.15f, 0.22f, 0.80f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.95f, 1.00f, 0.30f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.95f, 1.00f, 0.50f);
    
    // Glowing purple tabs
    colors[ImGuiCol_Tab]                    = ImVec4(0.09f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.98f, 0.00f, 0.75f, 0.80f); // Hot Purple
    colors[ImGuiCol_TabActive]              = ImVec4(0.98f, 0.00f, 0.75f, 1.00f); // Hot Purple Active
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.09f, 0.10f, 0.14f, 1.00f);

    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.18f, 0.22f, 0.33f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]    = ImVec4(0.00f, 0.95f, 1.00f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]     = ImVec4(0.00f, 0.95f, 1.00f, 1.00f);

    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.95f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.95f, 1.00f, 0.80f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.00f, 0.95f, 1.00f, 1.00f);
}

}  // namespace lilolify::ui
