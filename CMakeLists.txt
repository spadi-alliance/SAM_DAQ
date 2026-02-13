cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

project(SAMDAQ LANGUAGES C CXX)

# Detect and configure conda environment automatically
if(DEFINED ENV{CONDA_PREFIX})
    message(STATUS "Conda environment detected: $ENV{CONDA_PREFIX}")
    set(CMAKE_PREFIX_PATH "$ENV{CONDA_PREFIX};${CMAKE_PREFIX_PATH}")
    set(Qt5_DIR "$ENV{CONDA_PREFIX}/lib/cmake/Qt5")
    message(STATUS "Using conda Qt5 from: ${Qt5_DIR}")
else()
    message(WARNING "No conda environment detected. You may need to install dependencies manually.")
endif()

# Get ROOT compile flags from root-config
execute_process(
    COMMAND root-config --cflags
    OUTPUT_VARIABLE ROOT_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(ROOT_CFLAGS)
add_compile_options(${ROOT_CFLAGS})

# Qt and ROOT
find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(ROOT REQUIRED)

# Your source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CLIInterface.cpp
    src/helpers/Logger.cpp
    src/model/Connection/Connection.cpp
    src/model/Board/Board.cpp
    src/model/SAMDecoder/SAMDecoder.cpp
    src/model/SAMDecoder/DelegateManager.cpp
    src/model/SAMDecoder/DelegateWorker.cpp
    src/model/Delegates/WaveDisplayDecoderDelegate.cpp
    src/model/Delegates/MakeTTreeDecoderDelegate.cpp
    src/model/Delegates/RootTHttpServerDecoderDelegate.cpp
)

set(CMAKE_AUTOMOC ON)

# Add fakernet client library
file(GLOB FAKERNET_CLIENT_SRC
     ${CMAKE_SOURCE_DIR}/lib/fakernet/client/*.c
)

add_library(fakernet_client STATIC ${FAKERNET_CLIENT_SRC})
target_include_directories(fakernet_client PUBLIC ${CMAKE_SOURCE_DIR}/lib/fakernet/client)

# Main executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link all necessary libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Widgets
    ${ROOT_LIBRARIES}
    fakernet_client
    pthread
    ROOT::RHTTP
)

# Enable SSE2 instructions
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2 -mavx2")
