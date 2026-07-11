# ============================================================================
# Lilolify — Compiler Warnings Module
# ============================================================================
# Sets up strict, cross-platform compiler warning flags.
# Usage: target_link_libraries(my_target PRIVATE lilolify_warnings)
# ============================================================================

# Create an INTERFACE library that propagates warning flags to any target
# that links against it. This avoids repeating flags across CMakeLists.
add_library(lilolify_warnings INTERFACE)
add_library(lilolify::warnings ALIAS lilolify_warnings)

# Determine whether to treat warnings as errors
option(LILOLIFY_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# --- MSVC Flags ---
set(MSVC_WARNINGS
    /W4           # Highest practical warning level
    /w14242       # 'identifier': conversion, possible loss of data
    /w14254       # 'operator': conversion, possible loss of data
    /w14263       # Member function does not override base class virtual
    /w14265       # Class has virtual functions, but destructor is not virtual
    /w14287       # 'operator': unsigned/negative constant mismatch
    /w14296       # 'operator': expression is always true/false
    /w14311       # 'variable': pointer truncation
    /w14545       # Expression before comma evaluates to a function
    /w14546       # Function call before comma missing argument list
    /w14547       # Operator before comma has no effect
    /w14549       # Operator before comma has no effect (intentional?)
    /w14555       # Expression has no effect; expected side-effect
    /w14619       # #pragma warning: nonexistent warning number
    /w14640       # Thread-unsafe static member initialization
    /w14826       # Conversion is sign-extended
    /w14905       # Wide string literal cast to 'LPSTR'
    /w14906       # String literal cast to 'LPWSTR'
    /w14928       # Illegal copy-initialization
    /permissive-  # Enforce standards conformance
)

# --- GCC/Clang Flags ---
set(GCC_CLANG_WARNINGS
    -Wall                 # Reasonable baseline warnings
    -Wextra               # Additional useful warnings
    -Wpedantic            # Warn on non-standard extensions
    -Wshadow              # Warn on variable shadowing
    -Wnon-virtual-dtor    # Warn on classes with virtual funcs but no virtual dtor
    -Wold-style-cast      # Warn on C-style casts
    -Wcast-align          # Warn on potential alignment issues
    -Wunused              # Warn on anything unused
    -Woverloaded-virtual  # Warn if a virtual function is hidden
    -Wconversion          # Warn on type conversions that may lose data
    -Wsign-conversion     # Warn on sign conversions
    -Wnull-dereference    # Warn on null dereference paths
    -Wdouble-promotion    # Warn on implicit float-to-double promotion
    -Wformat=2            # Warn on format string issues
    -Wimplicit-fallthrough # Warn on fallthrough in switch cases
)

# GCC-only additional warnings
set(GCC_ONLY_WARNINGS
    -Wmisleading-indentation  # Warn on misleading indentation
    -Wduplicated-cond         # Warn on duplicated conditions in if-else
    -Wduplicated-branches     # Warn on duplicated if-else branches
    -Wlogical-op              # Warn on suspicious logical operations
    -Wuseless-cast            # Warn on useless casts
)

# Apply warnings based on compiler
if(MSVC)
    set(PROJECT_WARNINGS ${MSVC_WARNINGS})
    if(LILOLIFY_ENABLE_WARNINGS_AS_ERRORS)
        list(APPEND PROJECT_WARNINGS /WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(PROJECT_WARNINGS ${GCC_CLANG_WARNINGS})
    if(LILOLIFY_ENABLE_WARNINGS_AS_ERRORS)
        list(APPEND PROJECT_WARNINGS -Werror)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_WARNINGS ${GCC_CLANG_WARNINGS} ${GCC_ONLY_WARNINGS})
    if(LILOLIFY_ENABLE_WARNINGS_AS_ERRORS)
        list(APPEND PROJECT_WARNINGS -Werror)
    endif()
else()
    message(WARNING "Lilolify: Unknown compiler '${CMAKE_CXX_COMPILER_ID}' — no warnings set.")
endif()

target_compile_options(lilolify_warnings INTERFACE ${PROJECT_WARNINGS})

message(STATUS "Lilolify: Compiler warnings configured for ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Lilolify: Warnings as errors = ${LILOLIFY_ENABLE_WARNINGS_AS_ERRORS}")
