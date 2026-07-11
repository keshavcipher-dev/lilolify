// ============================================================================
// Lilolify — Desktop GUI Main Entry Point (ui_main.cpp)
// ============================================================================

#include <lilolify/ui/gui_app.hpp>
#include <iostream>

int main(int, char*[]) {
    try {
        lilolify::ui::GuiApp app;
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal Exception in GUI Application: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal Unknown Exception in GUI Application.\n";
        return 1;
    }
}
