cmake_minimum_required(VERSION 3.10)
project(GPUDebugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt5 (adjust Qt6 if needed)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION_MAJOR_MINOR)
string(STRIP "${VERSION_MAJOR_MINOR}" VERSION_MAJOR_MINOR)

# Get git commit count (if available)
execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT GIT_COMMIT_COUNT)
    set(GIT_COMMIT_COUNT 0)
endif()

set(APP_VERSION "${VERSION_MAJOR_MINOR}.${GIT_COMMIT_COUNT}")

# Get build date
string(TIMESTAMP APP_BUILD_DATE "%Y-%m-%d %H:%M:%S")

# Configure version.h
configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_SOURCE_DIR}/src/version.h
    @ONLY
)

# Add the generated version.h to your include paths
include_directories(${CMAKE_BINARY_DIR})

# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/debugger.cpp
)

set(HEADERS
    src/mainwindow.h
    src/debugger.h
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link Qt5 libraries
target_link_libraries(${PROJECT_NAME} Qt5::Widgets)

# Set output directory (optional)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)