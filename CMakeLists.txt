cmake_minimum_required(VERSION 3.16)
project(raft_server LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -pthread)

#source files here
set(SOURCES
    ProjectFiles/Server.cpp
    ProjectFiles/Node.cpp
    ProjectFiles/ClientSession.cpp
)

# Create exe
add_executable(raft_server ${SOURCES})

if(WIN32)
    message(STATUS "Configuring for windows (Mingw)")

    #Link manually
    target_link_libraries(raft_server
    boost_log-mgw15-mt-x64-1_86
    boost_log_setup-mgw15-mt-x64-1_86
    boost_thread-mt
    boost_filesystem-mt
    boost_date_time-mt
    advapi32
    psapi
    user32
    ws2_32
    mswsock
)
elseif (UNIX)
    message(STATUS "Configuring for windows (Mingw)")

    #Use cmake boost finder
    find_package(Boost REQUIRED COMPONENTS log log_setup thread filesystem date_time)

    # Link Boost libraries (assuming system-installed)
    target_link_libraries(raft_server PRIVATE Boost::log Boost::log_setup Boost::thread Boost::filesystem Boost::date_time pthread)

endif()