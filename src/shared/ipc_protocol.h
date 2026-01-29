#pragma once

#include <cstdint>

namespace suyan {

#define SUYAN_PIPE_NAME L"\\\\.\\pipe\\SuYanInputMethod"

enum IPCCommand : uint32_t {
    IPC_ECHO = 1,
    IPC_START_SESSION,
    IPC_END_SESSION,
    IPC_PROCESS_KEY,
    IPC_TEST_KEY,
    IPC_FOCUS_IN,
    IPC_FOCUS_OUT,
    IPC_UPDATE_POSITION,
    IPC_COMMIT,
    IPC_CLEAR,
    IPC_SELECT_CANDIDATE,
    IPC_SHUTDOWN
};

struct IPCMessage {
    IPCCommand cmd;
    uint32_t sessionId;
    uint32_t param1;
    uint32_t param2;
};

struct IPCResponse {
    uint32_t result;
    uint32_t dataSize;
};

enum ModifierFlags : uint32_t {
    SUYAN_MOD_NONE    = 0x00,
    SUYAN_MOD_SHIFT   = 0x01,
    SUYAN_MOD_CONTROL = 0x02,
    SUYAN_MOD_ALT     = 0x04
};

inline uint32_t packCoordinates(int16_t x, int16_t y) {
    return static_cast<uint32_t>(static_cast<uint16_t>(x)) |
           (static_cast<uint32_t>(static_cast<uint16_t>(y)) << 16);
}

inline void unpackCoordinates(uint32_t packed, int16_t& x, int16_t& y) {
    x = static_cast<int16_t>(packed & 0xFFFF);
    y = static_cast<int16_t>(packed >> 16);
}

} // namespace suyan
