cmake_minimum_required(VERSION 3.14)
project(MuteMotion_SteamVR_Shim VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Fetch Raylib (Lightweight 2D graphics framework)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0 # Pull stable release compatible with CMake 3.22
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # Don't build raylib examples
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# 2. Fetch OpenVR SDK
FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG master
)
FetchContent_MakeAvailable(openvr)

# 3. Setup core library linking
# We dynamically link the proprietary core engine built from the neighboring repo
set(CORE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../MuteMotion-Core/build")
set(CORE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../MuteMotion-Core/src")

# App Executable
file(GLOB SOURCES "src/*.cpp" "src/visuals/*.cpp")
add_executable(MuteMotionVR ${SOURCES})

# Include Directories
target_include_directories(MuteMotionVR PRIVATE 
    ${CORE_INC_DIR}
    ${openvr_SOURCE_DIR}/headers
)

# Link Libraries
if(WIN32)
    target_link_libraries(MuteMotionVR PRIVATE
        raylib
        ${openvr_SOURCE_DIR}/lib/win64/openvr_api.lib
        # core engine is loaded dynamically via LoadLibrary in Windows
    )
else()
    target_link_libraries(MuteMotionVR PRIVATE
        raylib
        ${openvr_SOURCE_DIR}/bin/linux64/libopenvr_api.so
        ${CORE_LIB_DIR}/libmutemotion_core.so
        dl
        pthread
    )
endif()

# Custom target to ensure the Core Engine builds first if we run make from this directory
if(WIN32)
    add_custom_target(build_core_engine
        COMMAND ${CMAKE_MAKE_PROGRAM} dll
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../MuteMotion-Core
        COMMENT "Ensuring Core Engine proprietary library is built for Windows..."
    )
else()
    add_custom_target(build_core_engine
        COMMAND ${CMAKE_MAKE_PROGRAM} all
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../MuteMotion-Core
        COMMENT "Ensuring Core Engine proprietary library is built..."
    )
endif()
add_dependencies(MuteMotionVR build_core_engine)

if(WIN32)
    add_custom_command(TARGET MuteMotionVR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${openvr_SOURCE_DIR}/bin/win64/openvr_api.dll"
        $<TARGET_FILE_DIR:MuteMotionVR>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CORE_LIB_DIR}/mutemotion_core.dll"
        $<TARGET_FILE_DIR:MuteMotionVR>
        COMMENT "Copying required DLLs to executable directory..."
    )
endif()
