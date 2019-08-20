#include <syslog.h>
#include <cstdlib>
#include "ipc.h"

namespace lightSwitch::os::ipc {
    IpcRequest::IpcRequest(uint8_t *tls_ptr, device_state &state) : req_info((command_struct *) tls_ptr) {
        state.logger->write(Logger::DEBUG, "Enable handle descriptor: {0}", (bool) req_info->handle_desc);
        if (req_info->handle_desc)
            throw exception("IPC - Handle descriptor");

        // Align to 16 bytes
        data_pos = 8;
        data_pos = ((data_pos - 1) & ~(15U)) + 16; // ceil data_pos with multiples 16
        data_ptr = &tls_ptr[data_pos + sizeof(command_struct)];

        state.logger->write(Logger::DEBUG, "Type: 0x{:X}", (uint8_t) req_info->type);
        state.logger->write(Logger::DEBUG, "X descriptors: {}", (uint8_t) req_info->x_no);
        state.logger->write(Logger::DEBUG, "A descriptors: {}", (uint8_t) req_info->a_no);
        state.logger->write(Logger::DEBUG, "B descriptors: {}", (uint8_t) req_info->b_no);
        state.logger->write(Logger::DEBUG, "W descriptors: {}", (uint8_t) req_info->w_no);
        state.logger->write(Logger::DEBUG, "Raw data offset: 0x{:X}", data_pos);
        state.logger->write(Logger::DEBUG, "Raw data size: {}", (uint16_t) req_info->data_sz);
        state.logger->write(Logger::DEBUG, "Payload Command ID: {}", *((uint32_t *) &tls_ptr[data_pos + 8]));
    }

    template<typename T>
    T IpcRequest::GetValue() {
        data_pos += sizeof(T);
        return *reinterpret_cast<T *>(&data_ptr[data_pos - sizeof(T)]);
    }
}