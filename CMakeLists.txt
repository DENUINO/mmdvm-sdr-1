cmake_minimum_required(VERSION 3.10)
project(mmdvm-sdr VERSION 1.0.0 LANGUAGES CXX C)

# ==================== Build Options ====================

option(STANDALONE_MODE "Build standalone SDR version (no GNU Radio required)" ON)
option(USE_NEON "Enable ARM NEON SIMD optimizations" ON)
option(USE_TEXT_UI "Enable ncurses-based text UI" ON)
option(PLUTO_SDR "Target ADALM PlutoSDR" ON)
option(BUILD_TESTS "Build unit tests" ON)

# ==================== Compiler Settings ====================

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O3")

# Add RPI definition for Linux/ARM builds
add_definitions(-DRPI)

# Debug build type
if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
  add_definitions(-DDEBUG)
endif()

# ==================== Platform-Specific Flags ====================

if(USE_NEON)
  # ARM NEON optimizations
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon -mfloat-abi=hard")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon -mfloat-abi=hard")
    add_definitions(-DUSE_NEON)
    message(STATUS "ARM NEON optimizations enabled")
  endif()
endif()

if(STANDALONE_MODE)
  add_definitions(-DSTANDALONE_MODE)
  message(STATUS "Standalone SDR mode enabled")
endif()

if(PLUTO_SDR AND STANDALONE_MODE)
  add_definitions(-DPLUTO_SDR)
  message(STATUS "PlutoSDR target enabled")
endif()

if(USE_TEXT_UI)
  add_definitions(-DTEXT_UI)
  message(STATUS "Text UI enabled")
endif()

# ==================== Dependencies ====================

# Required: pthread, math, realtime
find_package(Threads REQUIRED)

if(STANDALONE_MODE AND PLUTO_SDR)
  # Find libiio for PlutoSDR
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBIIO libiio)
  if(LIBIIO_FOUND)
    message(STATUS "Found libiio: ${LIBIIO_VERSION}")
    include_directories(${LIBIIO_INCLUDE_DIRS})
    add_definitions(-DHAVE_LIBIIO)
  else()
    message(WARNING "libiio not found. PlutoSDR support will be disabled.")
    message(WARNING "To enable PlutoSDR: sudo apt-get install libiio-dev libiio-utils")
    set(PLUTO_SDR OFF)
  endif()
endif()

if(NOT STANDALONE_MODE)
  # ZMQ mode requires ZeroMQ
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(ZMQ libzmq)
  endif()
  if(NOT ZMQ_FOUND)
    message(STATUS "ZeroMQ not found via pkg-config, trying find_library")
    find_library(ZMQ_LIBRARIES zmq)
    if(ZMQ_LIBRARIES)
      set(ZMQ_FOUND TRUE)
    endif()
  endif()
  if(ZMQ_FOUND)
    message(STATUS "Found ZeroMQ")
  else()
    message(WARNING "ZeroMQ not found. Install with: sudo apt-get install libzmq3-dev")
  endif()
endif()

if(USE_TEXT_UI)
  # Find ncurses for text UI
  find_package(Curses REQUIRED)
  if(CURSES_FOUND)
    message(STATUS "Found ncurses: ${CURSES_LIBRARIES}")
    include_directories(${CURSES_INCLUDE_DIRS})
  else()
    message(FATAL_ERROR "ncurses not found. Install with: sudo apt-get install libncurses5-dev")
  endif()
endif()

# ==================== Source Files ====================

# Core MMDVM sources (always compiled)
set(CORE_SOURCES
  MMDVM.cpp
  SerialPort.cpp
  SerialController.cpp
  ISerialPort.cpp
  Log.cpp
  Utils.cpp
  Config.h
  Globals.h
  Debug.h
)

# I/O layer (conditional based on mode)
if(STANDALONE_MODE)
  list(APPEND CORE_SOURCES IORPiSDR.cpp IO.cpp)
else()
  list(APPEND CORE_SOURCES IORPi.cpp IO.cpp)
endif()

# Mode-specific sources
set(MODE_SOURCES
  # D-Star
  DStarRX.cpp
  DStarTX.cpp
  DStarDefines.h

  # DMR
  DMRRX.cpp
  DMRTX.cpp
  DMRSlotRX.cpp
  DMRIdleRX.cpp
  DMRDMORX.cpp
  DMRDMOTX.cpp
  DMRSlotType.cpp
  DMRDefines.h

  # YSF
  YSFRX.cpp
  YSFTX.cpp
  YSFDefines.h

  # P25
  P25RX.cpp
  P25TX.cpp
  P25Defines.h

  # NXDN
  NXDNRX.cpp
  NXDNTX.cpp
  NXDNDefines.h

  # Calibration
  CalDStarRX.cpp
  CalDStarTX.cpp
  CalDMR.cpp
  CalP25.cpp
  CalNXDN.cpp
  CalRSSI.cpp

  # CW ID
  CWIdTX.cpp
)

# Ring buffers
set(BUFFER_SOURCES
  SampleRB.cpp
  SerialRB.cpp
  RSSIRB.cpp
  RingBuff.h
)

# ARM math library (conditional)
if(USE_NEON)
  set(MATH_SOURCES arm_math_neon.cpp arm_math_neon.h)
else()
  set(MATH_SOURCES arm_math_rpi.cpp arm_math_rpi.h)
endif()

# Standalone SDR sources
if(STANDALONE_MODE)
  set(SDR_SOURCES
    PlutoSDR.cpp
    PlutoSDR.h
    FMModem.cpp
    FMModem.h
    Resampler.cpp
    Resampler.h
  )
else()
  set(SDR_SOURCES "")
endif()

# Text UI sources
if(USE_TEXT_UI)
  set(UI_SOURCES
    # TextUI.cpp
    # TextUI.h
    UIStats.h
    # UIStats.cpp
  )
else()
  set(UI_SOURCES "")
endif()

# Combine all sources
set(ALL_SOURCES
  ${CORE_SOURCES}
  ${MODE_SOURCES}
  ${BUFFER_SOURCES}
  ${MATH_SOURCES}
  ${SDR_SOURCES}
  ${UI_SOURCES}
)

# ==================== Executable Target ====================

add_executable(mmdvm ${ALL_SOURCES})

# Include directories
target_include_directories(mmdvm PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBIIO_INCLUDE_DIRS}
  ${CURSES_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(mmdvm
  Threads::Threads
  m
  rt
  stdc++
)

# Link libiio for standalone mode
if(STANDALONE_MODE AND PLUTO_SDR)
  target_link_libraries(mmdvm ${LIBIIO_LIBRARIES})
endif()

# Link ZMQ for non-standalone mode
if(NOT STANDALONE_MODE AND ZMQ_FOUND)
  target_link_libraries(mmdvm ${ZMQ_LIBRARIES})
endif()

# Link ncurses for text UI
if(USE_TEXT_UI)
  target_link_libraries(mmdvm ${CURSES_LIBRARIES})
endif()

# ==================== Installation ====================

install(TARGETS mmdvm
  RUNTIME DESTINATION bin
)

# Install configuration file
# install(FILES mmdvm-sdr.conf
#   DESTINATION /etc
# )

# ==================== Unit Tests ====================

if(BUILD_TESTS)
  enable_testing()

  # Test executables
  add_executable(test_resampler tests/test_resampler.cpp Resampler.cpp)
  target_include_directories(test_resampler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(test_resampler m)
  add_test(NAME Resampler COMMAND test_resampler)

  if(USE_NEON)
    add_executable(test_neon tests/test_neon.cpp arm_math_neon.cpp)
    target_include_directories(test_neon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_neon m)
    add_test(NAME NEON_Math COMMAND test_neon)
  endif()

  # FMModem test
  add_executable(test_fmmodem tests/test_fmmodem.cpp FMModem.cpp)
  target_include_directories(test_fmmodem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(test_fmmodem m)
  add_test(NAME FMModem COMMAND test_fmmodem)

  message(STATUS "Unit tests enabled")
endif()

# ==================== Build Summary ====================

message(STATUS "")
message(STATUS "========== MMDVM-SDR Build Configuration ==========")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Standalone mode:   ${STANDALONE_MODE}")
message(STATUS "PlutoSDR target:   ${PLUTO_SDR}")
message(STATUS "NEON optimization: ${USE_NEON}")
message(STATUS "Text UI:           ${USE_TEXT_UI}")
message(STATUS "Unit tests:        ${BUILD_TESTS}")
message(STATUS "C++ compiler:      ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags:         ${CMAKE_CXX_FLAGS}")
message(STATUS "===================================================")
message(STATUS "")
