# Project 02 builds one executable per experiment, following Project 01's
# pattern. Unlike Project 01, one file (latency_stats.hpp) IS shared across
# experiments on purpose: it is the fixed measurement apparatus this project
# exists to build, not part of the experimental progression being diffed
# (see src/latency_stats.hpp for the full rationale).
foreach(exp exp1_clock_resolution exp2_naive_mean exp3_percentile_capture exp4_compare)
  add_executable(02_${exp} src/${exp}.cpp)
  target_link_libraries(02_${exp} PRIVATE hft_common)
endforeach()
