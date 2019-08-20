project(cppproj)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native")

add_executable(cppproj main.cpp libnebulabrotgen.cpp stdcomplexrenderer.hpp)
target_link_libraries(cppproj pthread)
#target_link_libraries(cppproj dl)