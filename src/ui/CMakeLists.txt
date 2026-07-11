# ============================================================================
# Lilolify — UI Layer CMakeLists.txt
# ============================================================================

find_package(OpenGL REQUIRED)

# ----------------------------------------------------------------------------
# Build Static ImGui Library Target
# ----------------------------------------------------------------------------
set(IMGUI_SOURCES
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui
    PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
)

target_link_libraries(imgui
    PUBLIC
        glfw
        OpenGL::GL
)

if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

# ----------------------------------------------------------------------------
# Build GUI App Executable
# ----------------------------------------------------------------------------
add_executable(lilolify_gui
    src/gui_app.cpp
    src/ui_main.cpp
)

target_include_directories(lilolify_gui
    PRIVATE
        include
)

target_link_libraries(lilolify_gui
    PRIVATE
        lilolify_app
        lilolify_infra
        lilolify_ai
        lilolify_core
        imgui
        glfw
        OpenGL::GL
        lilolify::warnings
)

if(WIN32)
    target_link_libraries(lilolify_gui PRIVATE ole32 uuid)
endif()

set_target_properties(lilolify_gui PROPERTIES
    OUTPUT_NAME lilolify_gui
)
