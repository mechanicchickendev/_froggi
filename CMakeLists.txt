cmake_minimum_required(VERSION 3.1...3.25)
project(
    _froggi
    VERSION 0.1.1
    LANGUAGES CXX C
)

include(utils.cmake)

# Development mode option
option(DEV_MODE "Set up development helper settings" ON)

# ═══════════════════════════════════════════════════════════════════════
# Set external directory
# ═══════════════════════════════════════════════════════════════════════
set(EXTERNAL_DIR ${PROJECT_SOURCE_DIR}/external)

if(NOT EXISTS ${EXTERNAL_DIR})
    message(FATAL_ERROR "External directory not found at: ${EXTERNAL_DIR}")
endif()

message(STATUS "Looking for dependencies in: ${EXTERNAL_DIR}")

# ═══════════════════════════════════════════════════════════════════════
# Add dependencies from external/
# ═══════════════════════════════════════════════════════════════════════
if(EXISTS ${EXTERNAL_DIR}/glfw)
    add_subdirectory(${EXTERNAL_DIR}/glfw ${CMAKE_BINARY_DIR}/glfw)
    message(STATUS "Found glfw")
else()
    message(FATAL_ERROR "glfw not found at ${EXTERNAL_DIR}/glfw")
endif()

if(EXISTS ${EXTERNAL_DIR}/webgpu)
    add_subdirectory(${EXTERNAL_DIR}/webgpu ${CMAKE_BINARY_DIR}/webgpu)
    message(STATUS "Found webgpu")
else()
    message(FATAL_ERROR "webgpu not found at ${EXTERNAL_DIR}/webgpu")
endif()

if(EXISTS ${EXTERNAL_DIR}/glfw3webgpu)
    add_subdirectory(${EXTERNAL_DIR}/glfw3webgpu ${CMAKE_BINARY_DIR}/glfw3webgpu)
    message(STATUS "Found glfw3webgpu")
else()
    message(FATAL_ERROR "glfw3webgpu not found at ${EXTERNAL_DIR}/glfw3webgpu")
endif()

if(EXISTS ${EXTERNAL_DIR}/imgui)
    add_subdirectory(${EXTERNAL_DIR}/imgui ${CMAKE_BINARY_DIR}/imgui)
    message(STATUS "Found imgui")
else()
    message(FATAL_ERROR "imgui not found at ${EXTERNAL_DIR}/imgui")
endif()

# ═══════════════════════════════════════════════════════════════════════
# Set C++ standard globally
# ═══════════════════════════════════════════════════════════════════════
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ═══════════════════════════════════════════════════════════════════════
# Build Engine and Game (ONLY add_subdirectory calls)
# ═══════════════════════════════════════════════════════════════════════

# Build the engine library
if(EXISTS "${CMAKE_SOURCE_DIR}/engine")
    add_subdirectory(engine)
    message(STATUS "Building engine")
else()
    message(FATAL_ERROR "engine/ directory not found at ${CMAKE_SOURCE_DIR}/engine")
endif()

# Build the Sample
if(EXISTS "${CMAKE_SOURCE_DIR}/games/sample")
    add_subdirectory(games/sample)
    message(STATUS "Building Sample game")
else()
    message(WARNING "games/sample/ not found - skipping game build")
endif()

# ════════════════════════════════════════════════════════════════════
