cmake_minimum_required(VERSION 3.20)
project(hft_lab LANGUAGES CXX)

# ---- Global standards -------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # -std=c++20, not gnu++20
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for clangd / tooling

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Latency work is meaningless on unoptimized builds. Release is -O2 by default;
# individual projects may raise this locally, but never silently.
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")   # -g kept: perf needs symbols

# ---- Shared interface target ------------------------------------------------
# Every project links this. It carries warnings + the shared header path.
# It deliberately pulls in NO third-party dependency.
add_library(hft_common INTERFACE)
target_include_directories(hft_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_compile_options(hft_common INTERFACE -Wall -Wextra -Wpedantic)
find_package(Threads REQUIRED)
target_link_libraries(hft_common INTERFACE Threads::Threads)

# ---- Project auto-discovery -------------------------------------------------
# Any directory matching NN_* that has its own CMakeLists.txt is built.
# Nothing to register by hand when a new project is added.
file(GLOB _hft_project_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/[0-9][0-9]_*)
list(SORT _hft_project_dirs)

set(_hft_enabled "")
foreach(_dir ${_hft_project_dirs})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/CMakeLists.txt)
    add_subdirectory(${_dir})
    list(APPEND _hft_enabled ${_dir})
  endif()
endforeach()

if(_hft_enabled)
  message(STATUS "hft-lab projects: ${_hft_enabled}")
else()
  message(STATUS "hft-lab: no projects yet — scaffold one with scripts/new_project.sh")
endif()
