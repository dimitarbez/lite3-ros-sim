cmake_minimum_required(VERSION 3.0.2)
project(emotion_bot_lite3_hw)

find_package(catkin REQUIRED COMPONENTS message_generation roscpp rospy sensor_msgs std_msgs std_srvs)

catkin_python_setup()
add_message_files(FILES
  DirectJointCommand.msg
  DirectJointFeedback.msg
  DirectJointSafety.msg
  RobotState.msg
)
generate_messages(DEPENDENCIES std_msgs)
catkin_package(CATKIN_DEPENDS message_runtime roscpp rospy sensor_msgs std_msgs std_srvs)

include_directories(include ${catkin_INCLUDE_DIRS})
add_executable(motion_sdk_bridge src/motion_sdk_bridge.cpp)
add_dependencies(motion_sdk_bridge ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_compile_features(motion_sdk_bridge PRIVATE cxx_std_14)
target_link_libraries(motion_sdk_bridge ${catkin_LIBRARIES})

if(CATKIN_ENABLE_TESTING)
  add_executable(test_motion_sdk_protocol test/test_motion_sdk_protocol.cpp)
  target_compile_features(test_motion_sdk_protocol PRIVATE cxx_std_14)
  add_test(NAME motion_sdk_protocol COMMAND test_motion_sdk_protocol)
endif()

catkin_install_python(PROGRAMS
  scripts/action_controller.py
  scripts/direct_gate_stream.py
  scripts/emotion_receiver.py
  scripts/hardware_mapper.py
  scripts/posture_bridge.py
  scripts/planted_joint_controller.py
  scripts/safety_supervisor.py
  scripts/stop_relay_receiver.py
  scripts/telemetry_tap.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY config launch DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})
install(TARGETS motion_sdk_bridge RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
install(DIRECTORY include/${PROJECT_NAME}/ DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION})
