# Project 06 builds one executable per experiment, following Projects 01-05's
# pattern. atomic_counter.hpp is shared across every binary via a plain
# #include, the same way 05_false_sharing shares false_sharing.hpp: it is the
# fixed measurement apparatus this project builds (pinned threads racing on a
# counter, and the counter strategies under test), not part of the
# experimental progression being diffed.
foreach(exp exp1_sanity exp2_mutex exp3_atomic exp4_compare exp5_thread_scaling
            exp6_false_sharing)
  add_executable(06_${exp} src/${exp}.cpp)
  target_link_libraries(06_${exp} PRIVATE hft_common)
endforeach()
