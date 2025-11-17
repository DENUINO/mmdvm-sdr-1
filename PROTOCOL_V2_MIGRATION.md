# MMDVM Protocol V2 Migration Guide

**Version:** 1.0
**Date:** 2025-01-17
**Target:** mmdvm-sdr users and developers

---

## Overview

This document describes the upgrade of mmdvm-sdr from MMDVM Serial Protocol Version 1 to Version 2. This upgrade improves compatibility with modern MMDVMHost versions while maintaining backward compatibility through compile-time configuration.

## What Changed

### Protocol Version 1 (Legacy)

- Basic status reporting
- Simple version exchange
- Supported modes: D-Star, DMR, YSF, P25, NXDN
- Compatible with MMDVMHost versions up to ~2020

### Protocol Version 2 (Current)

- **Enhanced version reporting** - Mode capability flags indicate which modes are supported
- **Extended status reporting** - Additional buffer space fields for POCSAG and FM
- **Improved error handling** - New error code `ERR_NOT_SUPPORTED` (5)
- **Graceful unsupported command handling** - FM and POCSAG commands return NAK instead of being ignored
- **Better MMDVMHost compatibility** - Works with latest MMDVMHost versions (2021+)

## Key Features

### Mode Capability Flags

Protocol V2 includes capability flags in the `GET_VERSION` response:

```
Byte 4: Mode Capability Flags
  0x01 - D-Star support
  0x02 - DMR support
  0x04 - YSF support
  0x08 - P25 support
  0x10 - NXDN support
  0x20 - POCSAG support (NOT set in mmdvm-sdr)
  0x40 - FM support (NOT set in mmdvm-sdr)

Byte 5: Extended Capabilities (reserved for future use)
```

**mmdvm-sdr capability flags:** `0x1F` (D-Star + DMR + YSF + P25 + NXDN)

### Extended Status Fields

Protocol V2 `GET_STATUS` response includes:

- Bytes 0-11: Same as Protocol V1
- **Byte 12:** POCSAG TX buffer space (always 0 in mmdvm-sdr)
- **Byte 13:** FM TX buffer space (always 0 in mmdvm-sdr)

### Unsupported Mode Handling

When MMDVMHost sends FM or POCSAG commands:

**Protocol V1 behavior:**
- Returns generic NAK error code 1

**Protocol V2 behavior:**
- Returns NAK with error code 5 (`ERR_NOT_SUPPORTED`)
- Logs debug message: "Unsupported command: 0xXX (FM/POCSAG not implemented)"

Supported commands:
- `MMDVM_FM_PARAMS1` (0x60)
- `MMDVM_FM_PARAMS2` (0x61)
- `MMDVM_FM_PARAMS3` (0x62)
- `MMDVM_FM_DATA` (0x65)
- `MMDVM_POCSAG_DATA` (0x50)

All return `ERR_NOT_SUPPORTED` in Protocol V2.

## Configuration

### Selecting Protocol Version

Edit `/home/user/mmdvm-sdr/Config.h`:

```cpp
// ==================== Protocol Version Configuration ====================
// MMDVM Serial Protocol Version Selection
//
// Protocol Version 1 (default): Compatible with older MMDVMHost versions
// Protocol Version 2: Enhanced status reporting, capability flags, extended error handling
//
// Uncomment one of the following:
// #define PROTOCOL_VERSION 1
#define PROTOCOL_VERSION 2
```

### Build Configuration

**For Protocol V2 (default):**
```bash
cd /home/user/mmdvm-sdr
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

**For Protocol V1 (legacy compatibility):**
```bash
# Edit Config.h: Set PROTOCOL_VERSION to 1
cd /home/user/mmdvm-sdr
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

**Verification:**
```bash
./mmdvm
# Look in the log output:
# Protocol V1: "MMDVM protocol version: 1"
# Protocol V2: "MMDVM protocol version: 2"
```

## MMDVMHost Compatibility

### Protocol V2 (Recommended)

**Compatible MMDVMHost versions:**
- 2021 and later (native Protocol V2 support)
- Supports enhanced status reporting
- Recognizes capability flags

**MMDVM.ini configuration:**
```ini
[Modem]
Port=/home/user/mmdvm-sdr/build/ttyMMDVM0
TXInvert=1
RXInvert=1
# ... other settings ...

[POCSAG]
Enable=0  # Must be disabled (not supported)

[FM]
Enable=0  # Must be disabled (not supported)
```

### Protocol V1 (Legacy)

**Compatible MMDVMHost versions:**
- 2018-2020 era versions
- May show "protocol version mismatch" warning with newer versions

**MMDVM.ini configuration:** Same as Protocol V2

### Testing Compatibility

**Step 1: Start mmdvm-sdr**
```bash
cd /home/user/mmdvm-sdr/build
./mmdvm
```

**Step 2: Check protocol version**
```
Expected output:
[INFO] getVersion() invoked
[INFO] Protocol V2: Capabilities = 0x1F
```

**Step 3: Start MMDVMHost**
```bash
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini
```

**Step 4: Verify connection**
```
Expected output:
M: 2025-01-17 12:00:00.000 MMDVM protocol version: 2, description: MMDVM-SDR 20250117 (D-Star/DMR/System Fusion/P25/NXDN)
```

## Migration Steps

### From Protocol V1 to V2

**1. Backup current build**
```bash
cd /home/user/mmdvm-sdr/build
cp mmdvm mmdvm.v1.backup
```

**2. Update Config.h**
```bash
cd /home/user/mmdvm-sdr
# Edit Config.h: Change PROTOCOL_VERSION to 2
```

**3. Rebuild**
```bash
cd build
make clean
cmake ..
make -j$(nproc)
```

**4. Test with MMDVMHost**
```bash
# Terminal 1
./mmdvm

# Terminal 2
cd /path/to/MMDVMHost
./MMDVMHost MMDVM.ini
```

**5. Verify modes work**
- Test D-Star RX/TX
- Test DMR RX/TX
- Test YSF RX/TX
- Test P25 RX/TX
- Test NXDN RX/TX

**6. Check for errors**
```bash
# In mmdvm-sdr logs, look for:
# - "ADC Overflow" (should be rare)
# - "RX Overflow" (should be rare)
# - "TX Overflow" (should be rare)
# - "Unsupported command" (expected if FM/POCSAG enabled in MMDVMHost)
```

### Rollback to Protocol V1

If issues occur:

**1. Stop mmdvm-sdr and MMDVMHost**
```bash
killall mmdvm MMDVMHost
```

**2. Restore Config.h**
```bash
cd /home/user/mmdvm-sdr
# Edit Config.h: Change PROTOCOL_VERSION to 1
```

**3. Rebuild**
```bash
cd build
make clean
cmake ..
make -j$(nproc)
```

**4. Restart**
```bash
./mmdvm
```

## Technical Details

### GET_VERSION Command (0x00)

**Protocol V1 response:**
```
Byte 0: 0xE0 (MMDVM_FRAME_START)
Byte 1: Length
Byte 2: 0x00 (MMDVM_GET_VERSION)
Byte 3: 0x01 (Protocol version 1)
Byte 4+: Hardware description string
```

**Protocol V2 response:**
```
Byte 0: 0xE0 (MMDVM_FRAME_START)
Byte 1: Length
Byte 2: 0x00 (MMDVM_GET_VERSION)
Byte 3: 0x02 (Protocol version 2)
Byte 4: 0x1F (Capability flags: D-Star|DMR|YSF|P25|NXDN)
Byte 5: 0x00 (Extended capabilities)
Byte 6+: Hardware description string
```

### GET_STATUS Command (0x01)

**Protocol V1 response (12 bytes):**
```
Byte 0: 0xE0 (MMDVM_FRAME_START)
Byte 1: 0x0C (Length = 12)
Byte 2: 0x01 (MMDVM_GET_STATUS)
Byte 3: Mode enable flags
Byte 4: Current state
Byte 5: Status flags
Byte 6: D-Star TX space
Byte 7: DMR TX space 1
Byte 8: DMR TX space 2
Byte 9: YSF TX space
Byte 10: P25 TX space
Byte 11: NXDN TX space
```

**Protocol V2 response (14 bytes):**
```
Bytes 0-11: Same as Protocol V1
Byte 12: POCSAG TX space (0x00 in mmdvm-sdr)
Byte 13: FM TX space (0x00 in mmdvm-sdr)
```

### Error Codes

**Protocol V1:**
```
1 - ERR_INVALID_COMMAND
2 - ERR_WRONG_MODE
3 - ERR_BUFFER_OVERFLOW
4 - ERR_INVALID_PARAMS
```

**Protocol V2 (adds):**
```
5 - ERR_NOT_SUPPORTED (new in V2)
6 - ERR_TX_OVERFLOW (new in V2)
```

## Known Issues and Limitations

### 1. FM Mode Not Supported

**Issue:** mmdvm-sdr does not implement FM mode (analog FM repeater functionality).

**Impact:**
- MMDVMHost with `[FM] Enable=1` will receive NAK responses
- FM commands (0x60-0x65) return `ERR_NOT_SUPPORTED`

**Workaround:** Set `[FM] Enable=0` in MMDVM.ini

**Future:** FM support may be added using SDR native analog FM capabilities (not upstream MMDVM code).

### 2. POCSAG Not Supported

**Issue:** mmdvm-sdr does not implement POCSAG (pager) mode.

**Impact:**
- MMDVMHost with `[POCSAG] Enable=1` will receive NAK responses
- POCSAG command (0x50) returns `ERR_NOT_SUPPORTED`

**Workaround:** Set `[POCSAG] Enable=0` in MMDVM.ini

**Future:** POCSAG support is not planned (specialty mode).

### 3. Backward Compatibility

**Issue:** Protocol V2 adds extra bytes to status response.

**Impact:**
- Older MMDVMHost versions expecting V1 may misinterpret status
- Version mismatch warning may appear in logs

**Workaround:** Use Protocol V1 build for older MMDVMHost versions.

### 4. RTS/CTS Flow Control

**Issue:** Virtual PTY doesn't support hardware flow control (existing issue, not related to protocol version).

**Impact:** Requires MMDVMHost modification (see MIGRATION_NOTES.md).

**Workaround:**
```cpp
// In MMDVMHost/Modem.cpp line 120:
m_serial = new CSerialController(port, false, 115200);
//                                     ^^^^^ Disable RTS check
```

## Testing Checklist

Before deploying Protocol V2 in production:

- [ ] Compile succeeds with PROTOCOL_VERSION=2
- [ ] Compile succeeds with PROTOCOL_VERSION=1 (verify backward compatibility)
- [ ] mmdvm-sdr starts without errors
- [ ] MMDVMHost connects successfully
- [ ] Protocol version 2 reported in logs
- [ ] Capability flags correct (0x1F)
- [ ] D-Star mode functional (RX and TX)
- [ ] DMR mode functional (RX and TX, both slots)
- [ ] YSF mode functional (RX and TX)
- [ ] P25 mode functional (RX and TX)
- [ ] NXDN mode functional (RX and TX)
- [ ] FM command returns ERR_NOT_SUPPORTED
- [ ] POCSAG command returns ERR_NOT_SUPPORTED
- [ ] No memory leaks (valgrind test)
- [ ] CPU usage acceptable (<30% on target platform)
- [ ] Extended status fields populated correctly
- [ ] MMDVMHost status display correct

## Performance Considerations

Protocol V2 adds minimal overhead:

**Memory:**
- V1: 12-byte status response
- V2: 14-byte status response (+2 bytes)

**CPU:**
- Negligible difference (<0.1%)
- Capability flag calculation is simple bitwise OR

**Network:**
- +2 bytes per status query
- +2 bytes per version query
- Insignificant for typical operation

## Troubleshooting

### Problem: "Unknown command: 0xXX" in logs

**Diagnosis:** MMDVMHost sending unsupported command

**Solution:**
1. Check if command is FM (0x60-0x65) or POCSAG (0x50)
2. Disable that mode in MMDVM.ini
3. If different command, may be new upstream feature

### Problem: MMDVMHost shows "Protocol version mismatch"

**Diagnosis:** MMDVMHost expects different protocol version

**Solution:**
- If expecting V2: Check PROTOCOL_VERSION in Config.h
- If expecting V1: Use Protocol V1 build
- Warning is informational; modes should still work

### Problem: Status display in MMDVMHost incorrect

**Diagnosis:** Possible protocol version mismatch

**Solution:**
1. Check mmdvm-sdr logs for protocol version
2. Verify MMDVMHost version compatibility
3. Try with Debug=1 in MMDVM.ini

### Problem: Build fails with "ERR_NOT_SUPPORTED undefined"

**Diagnosis:** ProtocolV2.h not included

**Solution:**
```bash
# Ensure ProtocolV2.h exists
ls /home/user/mmdvm-sdr/ProtocolV2.h

# Clean rebuild
cd build
make clean
cmake ..
make
```

## References

**Documentation:**
- `UPSTREAM_COMPARISON.md` - Detailed comparison with upstream MMDVM
- `MIGRATION_NOTES.md` - Integration and backporting guide
- `ProtocolV2.h` - Protocol V2 command definitions

**Upstream MMDVM:**
- Repository: https://github.com/g4klx/MMDVM
- Protocol V2 introduced: ~2021
- Current version: 20240113

**MMDVMHost:**
- Repository: https://github.com/g4klx/MMDVMHost
- Recommended version: Latest (supports Protocol V2)

## Changelog

### Version 1.0 (2025-01-17)

**Added:**
- Protocol V2 support with capability flags
- Extended status reporting (14 bytes vs 12 bytes)
- `ERR_NOT_SUPPORTED` error code
- Graceful handling of FM/POCSAG commands
- Compile-time protocol version selection in Config.h
- `handleUnsupportedCommand()` method

**Changed:**
- `PROTOCOL_VERSION` now configurable in Config.h (was hardcoded)
- Hardware description updated to "MMDVM-SDR 20250117"
- `getVersion()` includes capability flags in V2
- `getStatus()` includes POCSAG and FM buffer space in V2

**Fixed:**
- Proper NAK response for unsupported modes
- Better error logging for unknown commands

**Backward Compatibility:**
- Protocol V1 still supported via Config.h
- All existing functionality preserved

---

## Summary

Protocol V2 upgrade provides:

1. **Better Compatibility** - Works with latest MMDVMHost versions
2. **Clear Capability Reporting** - MMDVMHost knows which modes are supported
3. **Improved Error Handling** - Specific error codes for unsupported features
4. **Backward Compatible** - Can still build as Protocol V1 if needed
5. **Future Ready** - Extended capabilities field for future enhancements

**Recommended Action:** Use Protocol V2 for new installations. Keep Protocol V1 option for legacy MMDVMHost compatibility if needed.

---

**Document Version:** 1.0
**Last Updated:** 2025-01-17
**Maintainer:** mmdvm-sdr development team
**Next Review:** When upstream protocol changes
