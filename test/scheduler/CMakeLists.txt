cmake_minimum_required(VERSION 3.28)
project(cornobs-scheduler-tests LANGUAGES C)

enable_testing()
set(scheduler_dir "${CMAKE_CURRENT_LIST_DIR}/../../libobs/util")
add_executable(scheduler-policy-test scheduler-policy-test.c "${scheduler_dir}/threading-scheduler-policy.c")
target_compile_features(scheduler-policy-test PRIVATE c_std_11)
add_test(NAME scheduler-policy COMMAND scheduler-policy-test)

if(WIN32)
  # Includes the production adapter to inject API failures without changing OBS
  # or the machine's scheduling state. No libobs/FFmpeg/Qt dependency needed.
  add_executable(scheduler-windows-test scheduler-windows-test.c "${scheduler_dir}/threading-scheduler-policy.c")
  target_compile_features(scheduler-windows-test PRIVATE c_std_11)
  add_test(NAME scheduler-windows COMMAND scheduler-windows-test)
endif()

if(MSVC)
  target_compile_options(scheduler-policy-test PRIVATE /W4 /WX)
  target_compile_options(scheduler-windows-test PRIVATE /W4 /WX)
else()
  target_compile_options(scheduler-policy-test PRIVATE -Wall -Wextra -Werror)
  if(WIN32)
    target_compile_options(scheduler-windows-test PRIVATE -Wall -Wextra -Werror)
  endif()
endif()
