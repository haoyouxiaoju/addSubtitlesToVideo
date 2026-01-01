cmake_minimum_required(VERSION 3.16)

project(VideoSubtitleGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/download/qt/6.9.3/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)


include_directories("D:/download/qt/6.9.3/msvc2022_64/include")


set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/FileDropListWidget.cpp
    src/MainWindow.h
    src/FileDropListWidget.h
)

add_executable(VideoSubtitleGenerator ${PROJECT_SOURCES})

target_link_libraries(VideoSubtitleGenerator PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

# Copy scripts to build directory (Post-build event)
add_custom_command(TARGET VideoSubtitleGenerator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/scripts
    ${CMAKE_BINARY_DIR}/scripts
    COMMENT "Copying scripts to build directory"
)
