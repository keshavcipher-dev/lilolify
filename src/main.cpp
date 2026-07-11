// ============================================================================
// Lilolify — CLI Entry Point (main.cpp)
// ============================================================================

#include <lilolify/app/app_shell.hpp>
#include <lilolify/infra/database/sqlite_database.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace lilolify;
using namespace lilolify::app;

namespace {

// ============================================================================
// ANSI Color Codes Constants
// ============================================================================
constexpr const char* kColorReset   = "\033[0m";
constexpr const char* kColorRed     = "\033[31m";
constexpr const char* kColorGreen   = "\033[32m";
constexpr const char* kColorYellow  = "\033[33m";
constexpr const char* kColorBlue    = "\033[34m";
constexpr const char* kColorMagenta = "\033[35m";
constexpr const char* kColorCyan    = "\033[36m";
constexpr const char* kColorBold    = "\033[1m";

// ============================================================================
// Helper Outputs
// ============================================================================

void print_header() {
    std::cout << kColorBold << kColorCyan
              << "========================================\n"
              << "   Lilolify — AI-Powered File Reorganizer\n"
              << "========================================\n"
              << kColorReset;
}

void print_usage() {
    print_header();
    std::cout << kColorBold << "Usage:\n" << kColorReset
              << "  lilolify_cli.exe <command> [arguments]\n\n"
              << kColorBold << "Commands:\n" << kColorReset
              << "  " << kColorGreen << "scan <src_dir> <dest_dir> [--copy|--move]" << kColorReset 
              << "  Run organizing job pipeline\n"
              << "  " << kColorGreen << "settings show" << kColorReset
              << "                         Display active configurations\n"
              << "  " << kColorGreen << "settings set-provider <name>" << kColorReset
              << "                 Set provider (openai/gemini/claude)\n"
              << "  " << kColorGreen << "settings set-api-key <key>" << kColorReset
              << "                  Update API access credentials\n"
              << "  " << kColorGreen << "settings set-model <model>" << kColorReset
              << "                  Configure default AI reasoning model\n"
              << "  " << kColorGreen << "history" << kColorReset
              << "                                    List transaction history records\n"
              << "  " << kColorGreen << "undo <transaction-id>" << kColorReset
              << "                  Revert modifications of a transaction\n"
              << "  " << kColorGreen << "help" << kColorReset
              << "                                       Show this help text screen\n\n";
}

// Draw dynamic progress bar
void render_progress_bar(const PipelineProgress& progress) {
    constexpr int kBarWidth = 25;
    double percentage = progress.total_files > 0 ? (100.0 * progress.processed_files / progress.total_files) : 0.0;
    int pos = static_cast<int>(kBarWidth * (percentage / 100.0));
    
    std::cout << "\r" << kColorCyan << "[";
    for (int i = 0; i < kBarWidth; ++i) {
        if (i < pos) std::cout << "█";
        else if (i == pos) std::cout << "▒";
        else std::cout << " ";
    }
    std::cout << "] " << kColorBold << static_cast<int>(percentage) << "%" << kColorReset;
    
    std::cout << " (" << progress.processed_files << "/" << progress.total_files << " files) ";
    
    if (!progress.current_file_path.empty()) {
        std::string fn = std::filesystem::path(progress.current_file_path).filename().string();
        if (fn.length() > 20) {
            fn = fn.substr(0, 17) + "...";
        }
        std::cout << kColorYellow << "Processing: " << fn << kColorReset;
    }
    
    // Output line blank paddings to wipe old stdout sequences
    std::cout << "       " << std::flush;
}

}  // namespace

// ============================================================================
// Main Application Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty() || args[0] == "help" || args[0] == "--help") {
        print_usage();
        return 0;
    }

    // Initialize AppShell
    AppShell shell;
    auto init_res = shell.initialize();
    if (init_res.has_error()) {
        std::cerr << kColorRed << "✘ Failed to initialize AppShell: " 
                  << init_res.error().message() << kColorReset << "\n";
        return 1;
    }

    const std::string command = args[0];

    // ------------------------------------------------------------------------
    // COMMAND: SCAN & ORGANIZE
    // ------------------------------------------------------------------------
    if (command == "scan" || command == "organize") {
        if (args.size() < 3) {
            std::cerr << kColorRed << "✘ Error: Missing source or destination directory.\n" << kColorReset
                      << "Usage: lilolify_cli.exe scan <src_dir> <dest_dir> [--copy|--move]\n";
            return 1;
        }

        std::string src_dir = args[1];
        std::string dest_dir = args[2];
        core::FileActionType action = core::FileActionType::kMove;

        // Parse optional parameters
        if (args.size() >= 4) {
            if (args[3] == "--copy") {
                action = core::FileActionType::kCopy;
            } else if (args[3] == "--move") {
                action = core::FileActionType::kMove;
            }
        }

        std::cout << kColorCyan << "➜ Starting organization pipeline...\n" << kColorReset
                  << "  Source:      " << src_dir << "\n"
                  << "  Destination: " << dest_dir << "\n"
                  << "  Action Mode: " << (action == core::FileActionType::kCopy ? "Copy" : "Move") << "\n\n";

        std::mutex cv_m;
        std::condition_variable cv;
        bool done = false;
        std::string fail_message;

        auto callback = [&](const PipelineProgress& progress) {
            std::lock_guard<std::mutex> lock(cv_m);
            
            // Print progress bar updates
            if (progress.state == PipelineState::kScanning) {
                std::cout << "\r➜ Scanning directory files..." << std::flush;
            } else if (progress.state == PipelineState::kProcessing || progress.state == PipelineState::kReorganizing) {
                render_progress_bar(progress);
            } else if (progress.state == PipelineState::kCompleted) {
                done = true;
                cv.notify_one();
            } else if (progress.state == PipelineState::kFailed) {
                done = true;
                fail_message = "Job pipeline coordinator execution failed.";
                cv.notify_one();
            }
        };

        auto start_res = shell.start_job(src_dir, dest_dir, action, callback);
        if (start_res.has_error()) {
            std::cerr << kColorRed << "✘ Failed to start job: " 
                      << start_res.error().message() << kColorReset << "\n";
            return 1;
        }

        // Wait for loop completion
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [&] { return done; });
        shell.wait_for_job();

        std::cout << "\n\n";

        if (!fail_message.empty()) {
            std::cerr << kColorRed << "✘ Error: " << fail_message << kColorReset << "\n";
            return 1;
        }

        auto p = shell.progress();
        // Since PipelineProgress does not directly store the generated transaction_id,
        // we can read the last transaction ID from the database action history logs!
        std::string tx_id = "unknown";
        infra::SqliteDatabase db;
        if (db.open(shell.settings().database_path).has_value()) {
            auto history_res = db.get_all_history();
            if (history_res.has_value() && !history_res.value().empty()) {
                tx_id = history_res.value().back().transaction_id;
            }
        }

        std::cout << kColorGreen << "✔ Success! Reorganization complete.\n" << kColorReset
                  << "  Total scanned:   " << p.total_files << "\n"
                  << "  AI Categorized:  " << p.processed_files << "\n"
                  << "  Files organized: " << p.organized_files << "\n"
                  << "  Transaction ID:  " << kColorBold << tx_id << kColorReset << "\n";
        return 0;
    }

    // ------------------------------------------------------------------------
    // COMMAND: SETTINGS
    // ------------------------------------------------------------------------
    if (command == "settings") {
        if (args.size() < 2) {
            std::cerr << kColorRed << "✘ Error: Missing settings subcommand (show, set-provider, set-api-key, set-model).\n" << kColorReset;
            return 1;
        }

        auto settings = shell.settings();
        std::string subcmd = args[1];

        if (subcmd == "show") {
            print_header();
            std::cout << kColorBold << "Active Application Configuration:\n" << kColorReset
                      << "  AI Active Provider: " << kColorYellow << settings.ai.active_provider << kColorReset << "\n"
                      << "  AI Model Name:      " << kColorYellow << settings.ai.model << kColorReset << "\n"
                      << "  AI Temperature:     " << settings.ai.temperature << "\n"
                      << "  Scan Max Depth:     " << settings.scan.max_depth << "\n"
                      << "  Database Caching:   " << settings.database_path << "\n";
            return 0;
        }

        if (subcmd == "set-provider") {
            if (args.size() < 3) {
                std::cerr << kColorRed << "✘ Error: Missing provider value.\n" << kColorReset;
                return 1;
            }
            settings.ai.active_provider = args[2];
            // Provide sensible defaults for models
            if (args[2] == "gemini") settings.ai.model = "gemini-1.5-flash";
            else if (args[2] == "claude") settings.ai.model = "claude-3-5-sonnet-latest";
            else settings.ai.model = "gpt-4o";

            auto save_res = shell.update_settings(settings);
            if (save_res.has_error()) {
                std::cerr << kColorRed << "✘ Save failed: " << save_res.error().message() << kColorReset << "\n";
                return 1;
            }
            std::cout << kColorGreen << "✔ AI provider updated successfully to " << args[2] << ".\n" << kColorReset;
            return 0;
        }

        if (subcmd == "set-api-key") {
            if (args.size() < 3) {
                std::cerr << kColorRed << "✘ Error: Missing API key value.\n" << kColorReset;
                return 1;
            }
            // Obfuscate before storing
            ConfigurationManager config;
            settings.ai.api_key = config.obfuscate_key(args[2]);

            auto save_res = shell.update_settings(settings);
            if (save_res.has_error()) {
                std::cerr << kColorRed << "✘ Save failed: " << save_res.error().message() << kColorReset << "\n";
                return 1;
            }
            std::cout << kColorGreen << "✔ API key stored and obfuscated safely on local storage.\n" << kColorReset;
            return 0;
        }

        if (subcmd == "set-model") {
            if (args.size() < 3) {
                std::cerr << kColorRed << "✘ Error: Missing model identifier value.\n" << kColorReset;
                return 1;
            }
            settings.ai.model = args[2];

            auto save_res = shell.update_settings(settings);
            if (save_res.has_error()) {
                std::cerr << kColorRed << "✘ Save failed: " << save_res.error().message() << kColorReset << "\n";
                return 1;
            }
            std::cout << kColorGreen << "✔ AI Model selection updated to " << args[2] << ".\n" << kColorReset;
            return 0;
        }

        std::cerr << kColorRed << "✘ Error: Unknown settings command '" << subcmd << "'.\n" << kColorReset;
        return 1;
    }

    // ------------------------------------------------------------------------
    // COMMAND: HISTORY
    // ------------------------------------------------------------------------
    if (command == "history") {
        print_header();
        
        // Since database_ handles are managed inside AppShell, let's open a temporary SqliteDatabase connection
        // to retrieve raw transaction history directly for display.
        infra::SqliteDatabase db;
        auto db_res = db.open(shell.settings().database_path);
        if (db_res.has_error()) {
            std::cerr << kColorRed << "✘ Failed to read history cache database: " 
                      << db_res.error().message() << kColorReset << "\n";
            return 1;
        }

        // Fetch transaction list from SQLite database
        auto history_res = db.get_all_history();
        if (history_res.has_error()) {
            std::cerr << kColorRed << "✘ Database read error: " << history_res.error().message() << kColorReset << "\n";
            return 1;
        }

        const auto& records = history_res.value();
        if (records.empty()) {
            std::cout << "No file action history logged. Perform a 'scan' command first!\n";
            return 0;
        }

        std::cout << kColorBold << "Executed File Relocation Transactions Ledger:\n" << kColorReset;
        std::string last_tx;
        for (const auto& rec : records) {
            if (rec.transaction_id != last_tx) {
                last_tx = rec.transaction_id;
                std::cout << "\n➜ Transaction Session ID: " << kColorBold << kColorYellow << last_tx << kColorReset << "\n";
            }
            std::cout << "  [" << (rec.action_type == core::FileActionType::kMove ? "MOVE" : "COPY") << "] "
                      << rec.original_path.filename().string() << " -> " << rec.executed_path.string() << "\n";
        }
        std::cout << "\n";
        return 0;
    }

    // ------------------------------------------------------------------------
    // COMMAND: UNDO / ROLLBACK
    // ------------------------------------------------------------------------
    if (command == "undo") {
        if (args.size() < 2) {
            std::cerr << kColorRed << "✘ Error: Missing target transaction session ID.\n" << kColorReset
                      << "Usage: lilolify_cli.exe undo <transaction-id>\n";
            return 1;
        }

        std::string tx_id = args[1];
        std::cout << kColorCyan << "➜ Reverting transaction '" << tx_id << "'...\n" << kColorReset;

        auto undo_res = shell.undo_transaction(tx_id);
        if (undo_res.has_error()) {
            std::cerr << kColorRed << "✘ Undo failed: " << undo_res.error().message() << kColorReset << "\n";
            return 1;
        }

        std::cout << kColorGreen << "✔ Success! Reverted all file organization actions for transaction session " << tx_id << ".\n" << kColorReset;
        return 0;
    }

    std::cerr << kColorRed << "✘ Error: Unknown command '" << command << "'.\n" << kColorReset;
    print_usage();
    return 1;
}
