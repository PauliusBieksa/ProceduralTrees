cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD 11)

project(Trees)

#### Setup Directories ####
#Main output directory
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/bin")
SET (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib")
SET (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib")

add_subdirectory("Lib/graphics_framework")

#Glew is dumb and won't listen to our build directories, so manually set it.
set_target_properties(libglew_shared PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

add_executable(Trees main.cpp)
#include_directories(${CMAKE_SOURCE_DIR})

#add_subdirectory("lib/graphics/labs/framework")
target_include_directories(Trees PUBLIC "Lib/graphics_framework")#dependencies
target_link_libraries(Trees PRIVATE enu_graphics_framework )


add_custom_target(copy_res ALL COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/res")


set_target_properties(enu_graphics_framework PROPERTIES FOLDER "DEPS")

#add_subdirectory("lib/something")
#target_include_directories(tset PUBLIC "lib/something")
