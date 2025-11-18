cmake_minimum_required(VERSION 3.1...3.25)
project(FroggiEngine)

get_filename_component(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(EXTERNAL_DIR ${ROOT_DIR}/external)

message(STATUS "Engine: Looking for external dependencies at ${EXTERNAL_DIR}")

# ═══════════════════════════════════════════════════════════════════════
# Find Jolt Physics - REQUIRED
# ═══════════════════════════════════════════════════════════════════════
set(JOLT_DIR "${EXTERNAL_DIR}/jolt/Build")

# Add Jolt as subdirectory if using source
if(EXISTS "${JOLT_DIR}/CMakeLists.txt")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
    
    # Enable debug renderer in ALL builds
    set(DEBUG_RENDERER_IN_DISTRIBUTION ON CACHE BOOL "Enable debug renderer in all builds" FORCE)
    
    # CRITICAL: Enable RTTI so we can inherit from DebugRendererSimple
    set(CPP_RTTI_ENABLED ON CACHE BOOL "Enable C++ RTTI" FORCE)
    
    # This ensures the flags are set
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJPH_DEBUG_RENDERER")
    
    add_subdirectory(${JOLT_DIR} ${CMAKE_BINARY_DIR}/jolt)
    
    # Force the debug renderer flag on the Jolt target
    target_compile_definitions(Jolt PUBLIC JPH_DEBUG_RENDERER)
    
    message(STATUS "Jolt Physics found - physics system enabled with debug renderer and RTTI")
else()
    message(FATAL_ERROR "Jolt Physics not found at ${JOLT_DIR}")
endif()

# ═══════════════════════════════════════════════════════════════════════
# Collect engine sources
# ═══════════════════════════════════════════════════════════════════════
set(ENGINE_SOURCES
    core/renderer.cpp
    core/engine.cpp
    core/resource_manager.cpp
    core/implementations.cpp
    core/animation_system.cpp
    core/collision_system.cpp
    core/jolt_debug_renderer.cpp
)

# ═══════════════════════════════════════════════════════════════════════
# Create static library
# ═══════════════════════════════════════════════════════════════════════
add_library(froggi_engine STATIC ${ENGINE_SOURCES})

# Define JPH_DEBUG_RENDERER for the engine
target_compile_definitions(froggi_engine PUBLIC JPH_DEBUG_RENDERER)

# ═══════════════════════════════════════════════════════════════════════
# Include directories
# ═══════════════════════════════════════════════════════════════════════
target_include_directories(froggi_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${EXTERNAL_DIR}/stb
    ${JOLT_DIR}
)

# ═══════════════════════════════════════════════════════════════════════
# Link to dependencies
# ═══════════════════════════════════════════════════════════════════════
target_link_libraries(froggi_engine PUBLIC
    glfw
    webgpu
    glfw3webgpu
    imgui
    Jolt
)

# ═══════════════════════════════════════════════════════════════════════
# ImGui sources (add to engine)
# ═══════════════════════════════════════════════════════════════════════
set(IMGUI_DIR ${EXTERNAL_DIR}/imgui)
if(EXISTS ${IMGUI_DIR})
    target_sources(froggi_engine PRIVATE
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
    )
    
    target_include_directories(froggi_engine PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    message(STATUS "ImGui sources added to engine")
else()
    message(WARNING "ImGui directory not found at ${IMGUI_DIR}")
endif()

# ═══════════════════════════════════════════════════════════════════════
# Compiler settings
# ═══════════════════════════════════════════════════════════════════════
set_target_properties(froggi_engine PROPERTIES
    CXX_STANDARD 17
    VS_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1"
)

# Warning treatment (if function exists from utils.cmake)
if(COMMAND target_treat_all_warnings_as_errors)
    target_treat_all_warnings_as_errors(froggi_engine)
endif()

# GLM warning bypass (MSVC)
if(MSVC)
    target_compile_options(froggi_engine PUBLIC /wd4201)
endif()

# ═══════════════════════════════════════════════════════════════════════
# Copy shaders to build directory
# ═══════════════════════════════════════════════════════════════════════
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    # Copy to engine directory (for engine tests)
    add_custom_command(TARGET froggi_engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:froggi_engine>/shaders
        COMMENT "Copying engine shaders to engine build directory"
    )
    message(STATUS "Shaders will be copied to engine build directory")
else()
    message(WARNING "Engine shaders directory not found at ${CMAKE_CURRENT_SOURCE_DIR}/shaders")
endif()
#  ═══════════════════════════════════════════════════════════════════════
