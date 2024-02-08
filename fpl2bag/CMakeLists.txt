# GENERAL ==============================================================================================================
cmake_minimum_required(VERSION 3.5)
project(fpl2bag
    LANGUAGES CXX
    VERSION 0.0.1
    DESCRIPTION "Fixposition fpl2bag utility"
)

# COMPILER SETUP =======================================================================================================
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic\
    -Wshadow -Wunused-parameter -Wformat -Wpointer-arith\
    -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# DEPENDENCIES =========================================================================================================
find_package(catkin REQUIRED COMPONENTS
    roscpp
    rosbag
    topic_tools
)
include_directories(include ${catkin_INCLUDE_DIRS})

# BUILD EXECUTABLES =================================================================================================
add_executable(fpl2bag src/fpl2bag.cpp)
target_link_libraries(fpl2bag ${catkin_LIBRARIES})

# INSTALL ==============================================================================================================
install(TARGETS fpl2bag RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
