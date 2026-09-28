# Project 05 builds one executable per experiment, following Projects 01-04's
# pattern. false_sharing.hpp is shared across every binary via a plain
# #include, the same way 04_cache_latency shares cache_lab.hpp: it is the
# fixed measurement apparatus this project builds (pinned threads racing on
# private counters, and the counter layouts under test), not part of the
# experimental progression being diffed.
foreach(exp exp1_layout_sanity exp2_packed exp3_padded exp4_compare
            exp5_thread_scaling exp6_offset_sweep exp7_placement_sweep)
  add_executable(05_${exp} src/${exp}.cpp)
  target_link_libraries(05_${exp} PRIVATE hft_common)
endforeach()
