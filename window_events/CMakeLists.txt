cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(window_events LANGUAGES CXX)

set(BUILD_TESTS OFF)
set(BUILD_DOCS OFF)

add_subdirectory(../neutrino_framework framework)

set(SOURCES
    src/main.cpp
    src/view.hpp
    src/view.cpp
    src/event_handler.hpp
    src/event_handler.cpp
    src/data_context.hpp
    src/data_context.cpp
)

configure_file(${CMAKE_SOURCE_DIR}/data/UbuntuMono-Regular.ttf ${CMAKE_BINARY_DIR}/data/UbuntuMono-Regular.ttf COPYONLY)

add_executable(${PROJECT_NAME} "")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

target_link_libraries(${PROJECT_NAME} neutrino)
target_include_directories(${PROJECT_NAME} PRIVATE $<TARGET_PROPERTY:neutrino,INCLUDE_DIRECTORIES>)
target_include_directories(${PROJECT_NAME} PRIVATE src)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES USE_FOLDERS ON)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER src)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_GENERATE_SCHEME TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
