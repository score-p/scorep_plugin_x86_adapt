cmake_minimum_required(VERSION 2.8)
project(x86_adapt_plugin)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/common;${CMAKE_MODULE_PATH}")

# Intialize git submodules if not done already
file(GLOB SUBMODULE_FILES "${CMAKE_SOURCE_DIR}/common/*")
list(LENGTH SUBMODULE_FILES COUNT_COMMON)
if(${COUNT_COMMON} EQUAL 0)
    message(STATUS "Initializing git submodule")
    execute_process(COMMAND "git" "submodule" "init" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    execute_process(COMMAND "git" "submodule" "update" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

include(common/DefaultBuildType.cmake)

find_package(X86Adapt REQUIRED)
if (NOT X86Adapt_FOUND)
    message(SEND_ERROR "Missing required libx86_adapt.")
endif()

include_directories(SYSTEM ${X86_ADAPT_INCLUDE_DIRS})

find_package(Scorep REQUIRED)
include(scorep/ScorepCXXPlugin.cmake)

include_directories(include)

add_library(${PROJECT_NAME} SHARED src/main.cpp src/main.cpp)

target_link_libraries(${PROJECT_NAME} ${X86_ADAPT_LIBRARIES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
