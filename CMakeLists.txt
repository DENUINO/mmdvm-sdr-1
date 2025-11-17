cmake_minimum_required(VERSION 3.10)
project(mmdvm-sdr VERSION 1.0.0 LANGUAGES CXX C)

# ==================== Build Options ====================

# Core build modes
option(STANDALONE_MODE "Build standalone SDR version (no GNU Radio required)" ON)
option(USE_NEON "Enable ARM NEON SIMD optimizations" ON)
option(USE_TEXT_UI "Enable ncurses-based text UI" ON)
option(PLUTO_SDR "Target ADALM PlutoSDR" ON)
option(BUILD_TESTS "Build unit tests" ON)

# Protocol version (1 or 2)
set(PROTOCOL_VERSION "2" CACHE STRING "MMDVM protocol version (1 or 2)")
set_property(CACHE PROTOCOL_VERSION PROPERTY STRINGS "1" "2")

# Transport layer options
option(USE_UDP_MODEM "Use UDP socket instead of virtual PTY for modem communication" OFF)
option(USE_SHARED_MEMORY_IPC "Enable shared memory IPC (experimental)" OFF)

# Mode support options
option(ENABLE_FM_MODE "Enable FM analog mode support" ON)
option(ENABLE_POCSAG "Enable POCSAG paging mode support" OFF)

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

if(USE_TEXT_UI)
  add_definitions(-DTEXT_UI)
  message(STATUS "Text UI enabled")
endif()

# Protocol version
add_definitions(-DPROTOCOL_VERSION=${PROTOCOL_VERSION})
message(STATUS "MMDVM Protocol Version: ${PROTOCOL_VERSION}")

# UDP modem transport
if(USE_UDP_MODEM)
  add_definitions(-DUSE_UDP_MODEM)
  message(STATUS "UDP modem transport: ENABLED")
else()
  message(STATUS "UDP modem transport: DISABLED (using PTY)")
endif()

# Shared memory IPC
if(USE_SHARED_MEMORY_IPC)
  add_definitions(-DUSE_SHARED_MEMORY_IPC)
  message(STATUS "Shared memory IPC: ENABLED (experimental)")
endif()

# FM mode
if(ENABLE_FM_MODE)
  add_definitions(-DENABLE_FM_MODE)
  message(STATUS "FM analog mode: ENABLED")
endif()

# POCSAG mode
if(ENABLE_POCSAG)
  add_definitions(-DENABLE_POCSAG)
  message(STATUS "POCSAG paging mode: ENABLED")
endif()

# ==================== Dependencies ====================

# Required: pthread, math, realtime
find_package(Threads REQUIRED)

# Check for POSIX support (required for shared memory and UDP)
include(CheckIncludeFile)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/mman.h HAVE_SYS_MMAN_H)

if(USE_UDP_MODEM AND NOT HAVE_SYS_SOCKET_H)
  message(FATAL_ERROR "UDP modem requires POSIX socket support (sys/socket.h not found)")
endif()

if(USE_SHARED_MEMORY_IPC AND NOT HAVE_SYS_MMAN_H)
  message(FATAL_ERROR "Shared memory IPC requires POSIX mman support (sys/mman.h not found)")
endif()

# Check for PlutoSDR dependencies BEFORE adding the definition
if(STANDALONE_MODE AND PLUTO_SDR)
  # Find libiio for PlutoSDR
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBIIO libiio)
  if(LIBIIO_FOUND)
    message(STATUS "Found libiio: ${LIBIIO_VERSION}")
    include_directories(${LIBIIO_INCLUDE_DIRS})
    add_definitions(-DHAVE_LIBIIO)
    add_definitions(-DPLUTO_SDR)
    message(STATUS "PlutoSDR target enabled")
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
  SerialRPI.cpp
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

# FM mode sources (conditional)
if(ENABLE_FM_MODE)
  # Note: FMRX/FMTX not yet implemented, using FMModem for now
  # When implemented, add:
  # list(APPEND MODE_SOURCES FMRX.cpp FMTX.cpp FMDefines.h)
  message(STATUS "FM mode will use existing FMModem component")
endif()

# POCSAG mode sources (conditional)
if(ENABLE_POCSAG)
  # Check if POCSAG files exist
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/POCSAGRX.cpp")
    list(APPEND MODE_SOURCES
      POCSAGRX.cpp
      POCSAGTX.cpp
      POCSAGDefines.h
    )
    message(STATUS "POCSAG mode sources found and added")
  else()
    message(WARNING "POCSAG mode enabled but source files not found. Will build without POCSAG support.")
    set(ENABLE_POCSAG OFF CACHE BOOL "Enable POCSAG paging mode support" FORCE)
  endif()
endif()

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
    FMModem.cpp
    FMModem.h
    Resampler.cpp
    Resampler.h
  )
  # Add PlutoSDR sources only if libiio is available
  if(PLUTO_SDR)
    list(APPEND SDR_SOURCES PlutoSDR.cpp PlutoSDR.h)
  endif()
else()
  set(SDR_SOURCES "")
endif()

# UDP socket layer sources (conditional)
if(USE_UDP_MODEM)
  # Check if UDP files exist
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/UDPSocket.cpp")
    set(UDP_SOURCES
      UDPSocket.cpp
      UDPSocket.h
      UDPModemPort.cpp
      UDPModemPort.h
    )
    message(STATUS "UDP modem sources found and added")
  else()
    message(WARNING "UDP modem enabled but source files not found. Please implement UDPSocket and UDPModemPort.")
    message(WARNING "See docs/mmdvmhost/UDP_IMPLEMENTATION_PLAN.md for implementation details.")
    set(UDP_SOURCES "")
  endif()
else()
  set(UDP_SOURCES "")
endif()

# Shared memory IPC sources (conditional)
if(USE_SHARED_MEMORY_IPC)
  # Check if SharedMemoryIPC files exist
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SharedMemoryIPC.cpp")
    set(SHMEM_SOURCES
      SharedMemoryIPC.cpp
      SharedMemoryIPC.h
    )
    message(STATUS "Shared memory IPC sources found and added")
  else()
    message(WARNING "Shared memory IPC enabled but source files not found.")
    set(SHMEM_SOURCES "")
  endif()
else()
  set(SHMEM_SOURCES "")
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
  ${UDP_SOURCES}
  ${SHMEM_SOURCES}
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

# Additional libraries for shared memory
if(USE_SHARED_MEMORY_IPC)
  # RT library is already linked above, which includes shm_* functions
  message(STATUS "Shared memory support: using librt")
endif()

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

# Install configuration files
install(FILES Config.h
  DESTINATION /etc/mmdvm-sdr
  OPTIONAL
)

# Install documentation
install(FILES
  README.md
  STATUS.md
  DESTINATION share/doc/mmdvm-sdr
  OPTIONAL
)

# Install build options documentation
install(FILES
  BUILD_OPTIONS.md
  DESTINATION share/doc/mmdvm-sdr
  OPTIONAL
)

# ==================== Unit Tests ====================

if(BUILD_TESTS)
  enable_testing()

  # Core component tests
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

  # UDP socket tests (if UDP enabled and files exist)
  if(USE_UDP_MODEM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_udpsocket.cpp")
    add_executable(test_udpsocket tests/test_udpsocket.cpp UDPSocket.cpp)
    target_include_directories(test_udpsocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_udpsocket Threads::Threads)
    add_test(NAME UDPSocket COMMAND test_udpsocket)

    add_executable(test_udpmodemport tests/test_udpmodemport.cpp UDPModemPort.cpp UDPSocket.cpp ISerialPort.cpp)
    target_include_directories(test_udpmodemport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_udpmodemport Threads::Threads)
    add_test(NAME UDPModemPort COMMAND test_udpmodemport)

    message(STATUS "UDP modem tests added")
  endif()

  # Shared memory tests (if enabled and files exist)
  if(USE_SHARED_MEMORY_IPC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sharedmemory.cpp")
    add_executable(test_sharedmemory tests/test_sharedmemory.cpp SharedMemoryIPC.cpp)
    target_include_directories(test_sharedmemory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_sharedmemory rt Threads::Threads)
    add_test(NAME SharedMemoryIPC COMMAND test_sharedmemory)

    message(STATUS "Shared memory IPC tests added")
  endif()

  # POCSAG tests (if enabled and files exist)
  if(ENABLE_POCSAG AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pocsag.cpp")
    add_executable(test_pocsag tests/test_pocsag.cpp POCSAGRX.cpp POCSAGTX.cpp)
    target_include_directories(test_pocsag PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_pocsag m)
    add_test(NAME POCSAG COMMAND test_pocsag)

    message(STATUS "POCSAG tests added")
  endif()

  message(STATUS "Unit tests enabled")
endif()

# ==================== Build Summary ====================

message(STATUS "")
message(STATUS "========== MMDVM-SDR Build Configuration ==========")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "--- Core Options ---")
message(STATUS "Standalone mode:   ${STANDALONE_MODE}")
message(STATUS "PlutoSDR target:   ${PLUTO_SDR}")
message(STATUS "NEON optimization: ${USE_NEON}")
message(STATUS "Text UI:           ${USE_TEXT_UI}")
message(STATUS "Unit tests:        ${BUILD_TESTS}")
message(STATUS "")
message(STATUS "--- Protocol & Transport ---")
message(STATUS "Protocol version:  ${PROTOCOL_VERSION}")
message(STATUS "UDP modem:         ${USE_UDP_MODEM}")
message(STATUS "Shared memory IPC: ${USE_SHARED_MEMORY_IPC}")
message(STATUS "")
message(STATUS "--- Mode Support ---")
message(STATUS "FM mode:           ${ENABLE_FM_MODE}")
message(STATUS "POCSAG mode:       ${ENABLE_POCSAG}")
message(STATUS "D-Star:            ON (always)")
message(STATUS "DMR:               ON (always)")
message(STATUS "YSF:               ON (always)")
message(STATUS "P25:               ON (always)")
message(STATUS "NXDN:              ON (always)")
message(STATUS "")
message(STATUS "--- Compiler ---")
message(STATUS "C++ compiler:      ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags:         ${CMAKE_CXX_FLAGS}")
message(STATUS "===================================================")
message(STATUS "")
