# Project 05: a small library (src/lab/) that owns the apparatus and all
# console output, plus one thin executable per experiment. The experiment
# files hold only the concept — which layout, which CPUs, what is compared —
# and hand their results to lab/report for printing.
add_library(05_false_sharing_lab STATIC
  src/lab/cpu_placement.cpp
  src/lab/race_runner.cpp
  src/lab/trials.cpp
  src/lab/sanity.cpp
  src/lab/report.cpp
)
target_include_directories(05_false_sharing_lab PUBLIC src)
target_link_libraries(05_false_sharing_lab PUBLIC hft_common)

foreach(exp exp1_layout_sanity exp2_packed exp3_padded exp4_compare
            exp5_thread_scaling exp6_offset_sweep exp7_placement_sweep)
  add_executable(05_${exp} src/${exp}.cpp)
  target_link_libraries(05_${exp} PRIVATE 05_false_sharing_lab)
endforeach()
