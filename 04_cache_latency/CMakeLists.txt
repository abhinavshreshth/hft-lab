# Project 04 builds one executable per experiment, following Projects
# 01-03's pattern. cache_lab.hpp is shared across every binary that needs
# it, via a plain #include, the same way 02_latency_timer shares
# latency_stats.hpp and 03_cpu_migration shares cpu_topology.hpp: it is the
# fixed measurement apparatus this project builds (the pointer-chasing
# chain and timing loop), not part of the experimental progression being
# diffed.
foreach(exp exp1_chain_sanity exp2_sequential_sweep exp3_random_sweep exp4_compare)
  add_executable(04_${exp} src/${exp}.cpp)
  target_link_libraries(04_${exp} PRIVATE hft_common)
endforeach()
