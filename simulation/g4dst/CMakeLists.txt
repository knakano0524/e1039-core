# Setup the project
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(g4dst CXX C)

# source code
file(GLOB          sources ${PROJECT_SOURCE_DIR}/*.cc)
file(GLOB     dist_headers ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB non_dist_headers ${PROJECT_SOURCE_DIR}/LinkDef.h)
list(REMOVE_ITEM dist_headers ${non_dist_headers})

# ROOT dict generation
add_custom_command (
  OUTPUT g4dst_Dict.cc
  COMMAND rootcint
  ARGS -f g4dst_Dict.cc -noIncludePaths -inlineInputHeader -c -p
    -I${PROJECT_SOURCE_DIR}/ -I${ROOT_PREFIX}/include/
    ${dist_headers} ${non_dist_headers}
)

# ROOT
find_program(ROOTCONF "root-config")
if(ROOTCONF)
  message("-- Detecting ROOT:    found at ${ROOTCONF}")
else()
  message(FATAL_ERROR "-- Detecting ROOT:    not found")
endif()
execute_process(COMMAND root-config --prefix OUTPUT_VARIABLE ROOT_PREFIX  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND root-config --cflags OUTPUT_VARIABLE ROOT_CFLAGS  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND root-config --libs   OUTPUT_VARIABLE ROOT_LINK    OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x ${ROOT_CFLAGS}")

add_library(g4dst SHARED  ${sources} g4dst_Dict.cc)
target_link_libraries(g4dst -lfun4all -linterface_main -lphg4hit -lPHPythia8 -lg4detectors -lktracker_interface)

install(TARGETS g4dst         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES ${dist_headers} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${CMAKE_PROJECT_NAME}/)

# Install the pcm files in case of ROOT 6.
execute_process(COMMAND root-config --version OUTPUT_VARIABLE ROOT_VER)
string(SUBSTRING ${ROOT_VER} 0 1 ROOT_VER)
if (ROOT_VER GREATER 5)
   add_custom_target(install_pcm ALL COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib COMMAND cp -up *_rdict.pcm ${CMAKE_INSTALL_PREFIX}/lib)
   add_dependencies(install_pcm g4dst)
endif()

#add_library(g4dst SHARED
#  g4dst.C
#)
#
#add_custom_command(
#  OUTPUT g4dst.C
#  COMMAND echo
#  ARGS "//*** this is a generated empty file. Do not commit, do not edit" > g4dst.C
#)
#
#target_link_libraries(g4dst -lphg4hit -lg4detectors_io -lphgeom_io -lphhepmc_io -lphfield_io)
