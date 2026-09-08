# Template build file. scripts/new_project.sh substitutes the placeholder below
# with the project directory name (e.g. 01_cpu_pinned_worker).
add_executable(01_cpu_pinned_worker
  src/main.cpp
)
target_link_libraries(01_cpu_pinned_worker PRIVATE hft_common)
