cmake_minimum_required(VERSION 3.16)
project(discord-musicbot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find DPP
find_package(dpp REQUIRED)

# Find JsonCpp
pkg_check_modules(JSONCPP jsoncpp REQUIRED)

# Create executable
add_executable(discord-musicbot musicbot.cpp)

# Link libraries
target_link_libraries(discord-musicbot 
    dpp::dpp
    ${JSONCPP_LIBRARIES}
)

target_include_directories(discord-musicbot PRIVATE 
    ${JSONCPP_INCLUDE_DIRS}
)

target_compile_options(discord-musicbot PRIVATE 
    ${JSONCPP_CFLAGS_OTHER}
)