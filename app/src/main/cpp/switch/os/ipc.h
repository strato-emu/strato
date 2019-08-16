#pragma once

#include <cstdint>
#include "switch/common.h"

namespace lightSwitch::os::ipc {
    class IpcRequest {
    private:
        uint8_t *data_ptr;
        uint32_t data_pos;
    public:
        struct command_struct {
            // https://switchbrew.org/wiki/IPC_Marshalling#IPC_Command_Structure
            uint16_t type : 16;
            uint8_t x_no : 4;
            uint8_t a_no : 4;
            uint8_t b_no : 4;
            uint8_t w_no : 4;
            uint16_t data_sz : 10;
            uint8_t c_flags : 4;
            uint32_t : 17;
            bool handle_desc : 1;
        } *req_info;

        IpcRequest(uint8_t *tlsPtr, device_state &state);

        template<typename T>
        T GetValue();
    };
}