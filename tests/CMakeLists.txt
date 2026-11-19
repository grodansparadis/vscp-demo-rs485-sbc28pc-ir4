# Host tests of the node firmware (no PIC needed)
#
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
#
cmake_minimum_required(VERSION 3.16)
project(vscp-demo-rs485-sbc28pc-ir4-tests C)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../firmware)
set(VSCP_FW ${FW}/third-party/vscp-firmware/src)
if(NOT EXISTS ${VSCP_FW}/vscp-firmware.c)
  message(FATAL_ERROR "vscp-firmware is missing: run 'git submodule update --init'")
endif()

set(NODE_SOURCES
  ${FW}/blinky.c
  ${FW}/rs485-node.c
  ${FW}/persist.c
  ${FW}/vscp-rs485.c
  ${VSCP_FW}/vscp-firmware.c
  hw-stub.c
  test_node.c)

enable_testing()

# The default build (ninth bit), and the other frame options
foreach(variant default 8n1 fixed)
  add_executable(test_node_${variant} ${NODE_SOURCES})
  target_include_directories(test_node_${variant} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${FW} ${VSCP_FW})
  set_target_properties(test_node_${variant} PROPERTIES C_STANDARD 99)
  target_compile_options(test_node_${variant} PRIVATE -Wall -Wextra -Wno-unused-parameter)
  add_test(NAME node_${variant} COMMAND test_node_${variant})
endforeach()
target_compile_definitions(test_node_8n1 PRIVATE RS485_NINE_BIT=0 NODE_ADDRESS=9)
target_compile_definitions(test_node_fixed PRIVATE RS485_FIXED_LENGTH=1 RS485_CRC_ATM=1 NODE_ADDRESS=200)
