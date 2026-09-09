# Project 01 builds one executable per experiment, mirroring the four
# experiments in docs/reference/"Project 01 — CPU-Pinned Worker.docx".
#
# The workload loop is intentionally duplicated across the four files rather
# than factored into a shared header: each experiment is a frozen lab-notebook
# entry meant to be read on its own and diffed against the previous one
# (exp1 -> exp2 adds CPU detection, exp2 -> exp3 adds affinity).
foreach(exp exp1_unpinned exp2_detect_cpu exp3_pinned exp4_compare)
  add_executable(01_${exp} src/${exp}.cpp)
  target_link_libraries(01_${exp} PRIVATE hft_common)
endforeach()
