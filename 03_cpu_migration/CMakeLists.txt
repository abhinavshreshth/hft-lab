# Project 03 builds one executable per experiment, following Project 01/02's
# pattern. cpu_topology.hpp is shared across every binary that needs it, via
# a plain #include, the same way 02_latency_timer shares latency_stats.hpp:
# it is the fixed measurement apparatus this project builds (SMT-sibling
# classification), not part of the experimental progression being diffed.
foreach(exp exp1_topology_map exp2_coarse_detector exp3_fine_detector exp4_compare)
  add_executable(03_${exp} src/${exp}.cpp)
  target_link_libraries(03_${exp} PRIVATE hft_common)
endforeach()
