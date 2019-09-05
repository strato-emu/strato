#include <syslog.h>
#include <cstdlib>
#include "ipc.h"
#include "types/KProcess.h"

namespace lightSwitch::kernel::ipc {
    IpcRequest::IpcRequest(uint8_t *tls_ptr, device_state &state) : req_info((CommandStruct *) tls_ptr) {
        state.logger->Write(Logger::DEBUG, "Enable handle descriptor: {0}", (bool) req_info->handle_desc);

        data_offset = 8;
        if (req_info->handle_desc) {
            HandleDescriptor handledesc = *(HandleDescriptor *) (&tls_ptr[8]);
            state.logger->Write(Logger::DEBUG, "Moving {} handles and copying {} handles (0x{:X})", uint8_t(handledesc.move_count), uint8_t(handledesc.copy_count), tls_ptr[2]);
            data_offset += 4 + handledesc.copy_count * 4 + handledesc.move_count * 4;
        }

        if (req_info->x_no || req_info->a_no || req_info->b_no || req_info->w_no)
            state.logger->Write(Logger::ERROR, "IPC - Descriptors");

        // Align to 16 bytes
        data_offset = ((data_offset - 1) & ~(15U)) + 16; // ceil data_offset with multiples 16
        data_ptr = &tls_ptr[data_offset + sizeof(req_info)];

        state.logger->Write(Logger::DEBUG, "Type: 0x{:X}", (uint8_t) req_info->type);
        state.logger->Write(Logger::DEBUG, "X descriptors: {}", (uint8_t) req_info->x_no);
        state.logger->Write(Logger::DEBUG, "A descriptors: {}", (uint8_t) req_info->a_no);
        state.logger->Write(Logger::DEBUG, "B descriptors: {}", (uint8_t) req_info->b_no);
        state.logger->Write(Logger::DEBUG, "W descriptors: {}", (uint8_t) req_info->w_no);
        state.logger->Write(Logger::DEBUG, "Raw data offset: 0x{:X}", data_offset);
        state.logger->Write(Logger::DEBUG, "Raw data size: {}", (uint16_t) req_info->data_sz);
        state.logger->Write(Logger::DEBUG, "Payload Command ID: {0} (0x{0:X})", *((uint32_t *) &tls_ptr[data_offset + 8]));
    }

    template<typename T>
    T IpcRequest::GetValue() {
        data_offset += sizeof(T);
        return *reinterpret_cast<T *>(&data_ptr[data_offset - sizeof(T)]);
    }

    IpcResponse::IpcResponse() : resp_info{0} {
        tls_ptr = reinterpret_cast<uint32_t *>(new uint8_t[constant::tls_ipc_size]);
    }

    IpcResponse::~IpcResponse() {
        delete[] tls_ptr;
    }

    void IpcResponse::Generate(device_state &state) {
        state.logger->Write(Logger::DEBUG, "Moving {} handles and copying {} handles", moved_handles.size(), copied_handles.size());
        resp_info.type = 0;
        data_offset = 8;
        if (!moved_handles.empty() || !copied_handles.empty()) {
            resp_info.handle_desc = true;
            HandleDescriptor handledesc{};
            handledesc.copy_count = static_cast<uint8_t>(copied_handles.size());
            handledesc.move_count = static_cast<uint8_t>(moved_handles.size());
            *(HandleDescriptor *) &tls_ptr[2] = handledesc;

            uint64_t handle_index = 0;
            for (auto& copied_handle : copied_handles) {
                state.logger->Write(Logger::DEBUG, "Copying handle to 0x{:X}", 12 + handle_index * 4);
                tls_ptr[3 + handle_index] = copied_handle;
                handle_index += 1;
            }
            for (auto& moved_handle : moved_handles) {
                state.logger->Write(Logger::DEBUG, "Moving handle to 0x{:X}", 12 + handle_index * 4);
                tls_ptr[3 + handle_index] = moved_handle;
                handle_index += 1;
            }

            data_offset += 4 + copied_handles.size() * 4 + moved_handles.size() * 4;
        }

        state.logger->Write(Logger::DEBUG, "Data offset: 0x{:X}", data_offset);

        // Align to 16 bytes
        data_offset = ((data_offset - 1) & ~(15U)) + 16; // ceil data_offset with multiples 16

        if (is_domain) {
            tls_ptr[data_offset >> 2] = static_cast<uint32_t>(moved_handles.size());
            data_offset += 0x10;
        }

        data_offset >>= 2;

        // TODO: Calculate data size
        resp_info.data_sz = static_cast<uint16_t>(16 + (is_domain ? 4 : 0)); // + data_words;
        tls_ptr[data_offset] = constant::ipc_sfco;
        tls_ptr[data_offset + 2] = error_code;
    }

    void IpcResponse::SetError(uint32_t _error_code) { error_code = _error_code; }

    template<typename T>
    void IpcResponse::WriteValue() {

    }

    void IpcResponse::CopyHandle(uint32_t handle) { copied_handles.push_back(handle); }

    void IpcResponse::MoveHandle(uint32_t handle) { moved_handles.push_back(handle); }
}
