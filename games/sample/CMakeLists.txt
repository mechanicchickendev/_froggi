cmake_minimum_required(VERSION 3.1...3.25)
project(Sample_Game)

# Development mode option (should be inherited from root)
if(NOT DEFINED DEV_MODE)
    option(DEV_MODE "Set up development helper settings" ON)
endif()

# ═══════════════════════════════════════════════════════════════════════
# Collect game sources from scripts/ directory
# ═══════════════════════════════════════════════════════════════════════
file(GLOB_RECURSE GAME_SOURCES "scripts/*.cpp")

if(NOT GAME_SOURCES)
    message(WARNING "No C++ source files found in scripts/ directory")
    message(WARNING "Looking in: ${CMAKE_CURRENT_SOURCE_DIR}/scripts/")
endif()

# ═══════════════════════════════════════════════════════════════════════
# Create executable
# ═══════════════════════════════════════════════════════════════════════
add_executable(sample ${GAME_SOURCES})

# ═══════════════════════════════════════════════════════════════════════
# Link to engine library
# ═══════════════════════════════════════════════════════════════════════
target_link_libraries(sample PRIVATE froggi_engine)

# ═══════════════════════════════════════════════════════════════════════
# Include engine headers
# ═══════════════════════════════════════════════════════════════════════
target_include_directories(sample PRIVATE
    ${PROJECT_SOURCE_DIR}/engine/core
)

# ═══════════════════════════════════════════════════════════════════════
# Resource directory (DEV_MODE vs RELEASE)
# ═══════════════════════════════════════════════════════════════════════
if(DEV_MODE)
    # In dev mode, load resources from source tree (for live editing)
    target_compile_definitions(sample PRIVATE
        RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
    message(STATUS "sample: DEV_MODE enabled - Resources from ${CMAKE_CURRENT_SOURCE_DIR}/assets")
else()
    # In release mode, load resources relative to executable (portable)
    target_compile_definitions(sample PRIVATE
        RESOURCE_DIR="./assets"
    )
    message(STATUS "sample: RELEASE mode - Resources from ./assets (relative to executable)")
endif()

# ═══════════════════════════════════════════════════════════════════════
# Compiler settings
# ═══════════════════════════════════════════════════════════════════════
set_target_properties(sample PROPERTIES
    CXX_STANDARD 17
    VS_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1"
)

# Warning treatment (if function exists from utils.cmake)
if(COMMAND target_treat_all_warnings_as_errors)
    target_treat_all_warnings_as_errors(sample)
endif()

# Copy WebGPU binaries (if function exists from utils.cmake)
if(COMMAND target_copy_webgpu_binaries)
    target_copy_webgpu_binaries(sample)
    message(STATUS "WebGPU binaries will be copied for sample")
endif()

# GLM warning bypass (MSVC)
if(MSVC)
    target_compile_options(sample PUBLIC /wd4201)
endif()

# ═══════════════════════════════════════════════════════════════════════
# Copy assets to build directory (next to executable)
# ═══════════════════════════════════════════════════════════════════════
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:sample>/assets
        COMMENT "Copying assets to build directory"
    )
    message(STATUS " assets will be copied on build to $<TARGET_FILE_DIR:sample>/assets")
else()
    message(WARNING "sample assets directory not found at ${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()

message(STATUS "sample configured")
message(STATUS "  Source files: ${CMAKE_CURRENT_SOURCE_DIR}/scripts/")
message(STATUS "  Assets: ${CMAKE_CURRENT_SOURCE_DIR}/assets/")
message(STATUS "  Executable: sample")
# ═══════════════════════════════════════════════════════════════════════
# Copy engine shaders to game directory (needed for rendering)
# ═══════════════════════════════════════════════════════════════════════
add_custom_command(TARGET sample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/shaders
    $<TARGET_FILE_DIR:sample>/shaders
    COMMENT "Copying engine shaders to game directory"
)
message(STATUS "Engine shaders will be copied to game directory")
# ═══════════════════════════════════════════════════════════════════════
# END OF GAME CMakeLists.txt
# ═══════════════════════════════════════════════════════════════════════
