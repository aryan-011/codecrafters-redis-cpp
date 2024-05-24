cmake_minimum_required(VERSION 3.13)
project(redis-starter-cpp)

set(SOURCE_FILES src/Server.cpp src/Parser.cpp)
set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Include the FetchContent module
include(FetchContent)

# Declare the spdlog library as a dependency
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.10.0
)

# Configure the spdlog library
FetchContent_MakeAvailable(spdlog)

find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)

add_executable(server ${SOURCE_FILES})

# Include spdlog headers
target_include_directories(server PRIVATE ${spdlog_SOURCE_DIR}/include)

# Link against spdlog and other libraries
target_link_libraries(server PRIVATE
    asio::asio
    Threads::Threads
    spdlog::spdlog
)