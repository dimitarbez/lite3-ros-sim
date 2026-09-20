cmake_minimum_required(VERSION 3.10)
project(emotion_bot_lite3_motion_sdk_tools CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release CACHE STRING "build type")

add_executable(test_motion_sdk_safety test_motion_sdk_safety.cpp)
add_executable(test_motion_sdk_expression test_motion_sdk_expression.cpp)
add_executable(test_motion_sdk_expression_integration
  test_motion_sdk_expression_integration.cpp)
add_executable(test_motion_sdk_contact_estimator
  test_motion_sdk_contact_estimator.cpp)
add_executable(test_motion_sdk_paw_lift test_motion_sdk_paw_lift.cpp)

enable_testing()
add_test(NAME motion_sdk_safety COMMAND test_motion_sdk_safety)
add_test(NAME motion_sdk_expression COMMAND test_motion_sdk_expression)
add_test(NAME motion_sdk_expression_integration
  COMMAND test_motion_sdk_expression_integration)
add_test(NAME motion_sdk_contact_estimator
  COMMAND test_motion_sdk_contact_estimator)
add_test(NAME motion_sdk_paw_lift COMMAND test_motion_sdk_paw_lift)

option(BUILD_EXPRESSION_RUNTIME "Build against the official Lite3 MotionSDK" OFF)
set(MOTION_SDK_DIR "" CACHE PATH "Checked-out official Lite3_MotionSDK root")
if(BUILD_EXPRESSION_RUNTIME)
  if(NOT EXISTS "${MOTION_SDK_DIR}/include/sender.h")
    message(FATAL_ERROR "MOTION_SDK_DIR must point to official Lite3_MotionSDK")
  endif()
  file(GLOB MOTION_SDK_SOURCES "${MOTION_SDK_DIR}/src/*.cpp")
  add_executable(motion_sdk_expression_runner
    run_motion_sdk_neutral_breathing.cpp ${MOTION_SDK_SOURCES})
  target_include_directories(motion_sdk_expression_runner PRIVATE
    "${MOTION_SDK_DIR}/include"
    "${MOTION_SDK_DIR}/include/common"
    "${MOTION_SDK_DIR}/lib/eigen3"
    "${CMAKE_CURRENT_SOURCE_DIR}")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    target_link_libraries(motion_sdk_expression_runner PRIVATE
      "${MOTION_SDK_DIR}/lib/libdeeprobotics_legged_sdk_aarch64.so")
  else()
    target_link_libraries(motion_sdk_expression_runner PRIVATE
      "${MOTION_SDK_DIR}/lib/libdeeprobotics_legged_sdk_x86_64.so")
  endif()
  target_link_libraries(motion_sdk_expression_runner PRIVATE pthread m rt dl)
endif()
