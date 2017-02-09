project(MqttSample_c)


set (MQTTSAMPLE_SOURCE sample.c)
set (MQTTSAMPLE_DEPLIBS mqtt)

link_directories(${LIBRARY_OUTPUT_PATH})

add_executable(MqttSample ${MQTTSAMPLE_SOURCE})
target_link_libraries(MqttSample
  ${MQTTSAMPLE_DEPLIBS}
  )