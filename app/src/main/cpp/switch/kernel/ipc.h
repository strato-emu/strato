#pragma once

#include <cstdint>
#include <vector>
#include "switch/common.h"

namespace lightSwitch::kernel::ipc {
    struct CommandStruct {
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
    };

    struct HandleDescriptor {
        bool send_pid : 1;
        uint8_t copy_count : 4;
        uint8_t move_count : 4;
    };

    class IpcRequest {
    private:
        uint8_t *data_ptr;
        uint32_t data_offset;
    public:
        CommandStruct *req_info;

        IpcRequest(uint8_t *tlsPtr, device_state &state);

        template<typename T>
        T GetValue();
    };

    class IpcResponse {
    private:
        uint32_t *tls_ptr{};
        uint32_t data_offset{}; // Offset to raw data relative to tls_ptr

        bool is_domain{}; // TODO
        uint32_t error_code{};

        CommandStruct resp_info;
        std::vector<uint32_t> copied_handles;
        std::vector<uint32_t> moved_handles;
        std::vector<uint8_t> data;
        uint16_t data_pos{}; // Position in raw data relative to data_offset
    public:
        IpcResponse();

        ~IpcResponse();

        void Generate(device_state &state);

        void SetError(uint32_t _error_code);

        template<typename T>
        void WriteValue(); // TODO

        void CopyHandle(uint32_t handle);

        void MoveHandle(uint32_t handle);
    };
}
