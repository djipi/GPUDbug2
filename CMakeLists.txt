cmake_minimum_required(VERSION 3.10)
project(GPUDebugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt5 (adjust Qt6 if needed)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/gpudebugger.cpp
    src/Unit1.cpp
)

set(HEADERS
    src/mainwindow.h
    src/gpudebugger.h
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link Qt5 libraries
target_link_libraries(${PROJECT_NAME} Qt5::Widgets)

# Set output directory (optional)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)