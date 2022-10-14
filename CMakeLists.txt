#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(postprocessor)
set(CMAKE_CXX_STANDARD 14) # ???
set(CMAKE_CXX_FLAGS "-Wall -std=c++17") # ???

#----------------------------------------------------------------------------
# Find ROOT package (2018.04.13)
# find_package(ROOT REQUIRED COMPONENTS Hist RIO Physics Tree Gpad Graf3d Minuit Minuit2)


#----------------------------------------------------------------------------
# Setup ROOT include directories and compile definitions (2018.04.13)
#
# include(${ROOT_USE_FILE})


#----------------------------------------------------------------------------
# Locate sources and headers for this project (2018.04.13)
#
include_directories(${PROJECT_SOURCE_DIR}/include
		    ${ROOT_INCLUDE_DIRS})
		    #${GARFIELD_INCLUDE_DIRS})
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hpp)


#link_directories($ENV{GARFIELD_HOME}/Library)

#----------------------------------------------------------------------------
# Add the executable, and link it to the Geant4 libraries (2018.04.13)
#

add_executable(test_bench test_bench.cpp ${sources} ${headers})
# target_link_libraries(test_bench ${ROOT_LIBRARIES})
# target_link_libraries(test_bench sqlite3)
# target_link_libraries(test_bench uuid)
