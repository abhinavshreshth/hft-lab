# Template build file. scripts/new_project.sh substitutes the placeholder below
# with the project directory name (e.g. 01_cpu_pinned_worker).
add_executable(__TARGET__
  src/main.cpp
)
target_link_libraries(__TARGET__ PRIVATE hft_common)
