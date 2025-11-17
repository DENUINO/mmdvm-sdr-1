# MMDVM-SDR Integration Tests
# CMake configuration for comprehensive integration test suite

cmake_minimum_required(VERSION 3.10)

# Test executables with their dependencies

# Test 1: UDP Modem Communication
add_executable(test_udp_modem test_udp_modem.cpp)
target_include_directories(test_udp_modem PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_udp_modem pthread m)
add_test(NAME Integration_UDP_Modem COMMAND test_udp_modem)

# Test 2: FM Mode Encode/Decode
if(STANDALONE_MODE)
  add_executable(test_fm_mode test_fm_mode.cpp
    ${CMAKE_SOURCE_DIR}/FMModem.cpp
  )
  target_include_directories(test_fm_mode PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_link_libraries(test_fm_mode pthread m)
  add_test(NAME Integration_FM_Mode COMMAND test_fm_mode)

  # Add NEON optimized version if enabled
  if(USE_NEON)
    target_sources(test_fm_mode PRIVATE ${CMAKE_SOURCE_DIR}/arm_math_neon.cpp)
  else()
    target_sources(test_fm_mode PRIVATE ${CMAKE_SOURCE_DIR}/arm_math_rpi.cpp)
  endif()
endif()

# Test 3: POCSAG (Specification stub for future implementation)
add_executable(test_pocsag test_pocsag.cpp)
target_include_directories(test_pocsag PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_pocsag pthread m)
add_test(NAME Integration_POCSAG COMMAND test_pocsag)

# Test 4: Protocol V2 Commands
add_executable(test_protocol_v2 test_protocol_v2.cpp)
target_include_directories(test_protocol_v2 PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_protocol_v2 pthread m)
add_test(NAME Integration_Protocol_V2 COMMAND test_protocol_v2)

# Test 5: Shared Memory IPC
add_executable(test_shared_memory test_shared_memory.cpp)
target_include_directories(test_shared_memory PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_shared_memory pthread rt m)
add_test(NAME Integration_Shared_Memory COMMAND test_shared_memory)

# Test 6: Complete System End-to-End
add_executable(test_complete_system test_complete_system.cpp)
target_include_directories(test_complete_system PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test_complete_system pthread m)

# Add FM modem if in standalone mode
if(STANDALONE_MODE)
  target_sources(test_complete_system PRIVATE ${CMAKE_SOURCE_DIR}/FMModem.cpp)
  if(USE_NEON)
    target_sources(test_complete_system PRIVATE ${CMAKE_SOURCE_DIR}/arm_math_neon.cpp)
  else()
    target_sources(test_complete_system PRIVATE ${CMAKE_SOURCE_DIR}/arm_math_rpi.cpp)
  endif()
endif()

add_test(NAME Integration_Complete_System COMMAND test_complete_system)

# Custom test target to run all integration tests
add_custom_target(integration_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R "Integration_"
  DEPENDS
    test_udp_modem
    test_pocsag
    test_protocol_v2
    test_shared_memory
    test_complete_system
  COMMENT "Running all integration tests..."
)

# Add FM mode test to integration_tests target if available
if(STANDALONE_MODE)
  add_dependencies(integration_tests test_fm_mode)
endif()

# Verbose integration test target
add_custom_target(integration_tests_verbose
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "Integration_"
  DEPENDS
    test_udp_modem
    test_pocsag
    test_protocol_v2
    test_shared_memory
    test_complete_system
  COMMENT "Running all integration tests (verbose)..."
)

if(STANDALONE_MODE)
  add_dependencies(integration_tests_verbose test_fm_mode)
endif()

# Installation (optional - for system-wide test deployment)
install(TARGETS
  test_udp_modem
  test_pocsag
  test_protocol_v2
  test_shared_memory
  test_complete_system
  RUNTIME DESTINATION share/mmdvm-sdr/tests/integration
)

if(STANDALONE_MODE)
  install(TARGETS test_fm_mode
    RUNTIME DESTINATION share/mmdvm-sdr/tests/integration
  )
endif()

# Test summary
message(STATUS "")
message(STATUS "========== Integration Tests Configuration ==========")
message(STATUS "UDP Modem test:      ENABLED")
message(STATUS "FM Mode test:        ${STANDALONE_MODE}")
message(STATUS "POCSAG test:         ENABLED (stub)")
message(STATUS "Protocol V2 test:    ENABLED")
message(STATUS "Shared Memory test:  ENABLED")
message(STATUS "Complete System test: ENABLED")
message(STATUS "====================================================")
message(STATUS "")
