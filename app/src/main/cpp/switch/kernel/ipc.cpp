#include <syslog.h>
#include <cstdlib>
#include "ipc.h"
#include "types/KProcess.h"

namespace lightSwitch::kernel::ipc {
    IpcRequest::IpcRequest(bool is_domain, device_state &state) : is_domain(is_domain), state(state), tls() {
        u8 *curr_ptr = tls.data();
        state.this_process->ReadMemory(curr_ptr, state.this_thread->tls, constant::tls_ipc_size);
        header = reinterpret_cast<CommandHeader *>(curr_ptr);
        curr_ptr += sizeof(CommandHeader);
        if (header->handle_desc) {
            handle_desc = reinterpret_cast<HandleDescriptor *>(curr_ptr);
            curr_ptr += sizeof(HandleDescriptor) + (handle_desc->send_pid ? sizeof(u8) : 0);
            for (uint index = 0; handle_desc->copy_count > index; index++) {
                copy_handles.push_back(*reinterpret_cast<handle_t *>(curr_ptr));
                curr_ptr += sizeof(handle_t);
            }
            for (uint index = 0; handle_desc->move_count > index; index++) {
                move_handles.push_back(*reinterpret_cast<handle_t *>(curr_ptr));
                curr_ptr += sizeof(handle_t);
            }
        }
        for (uint index = 0; header->x_no > index; index++) {
            vec_buf_x.push_back(reinterpret_cast<BufferDescriptorX *>(curr_ptr));
            curr_ptr += sizeof(BufferDescriptorX);
        }
        for (uint index = 0; header->a_no > index; index++) {
            vec_buf_a.push_back(reinterpret_cast<BufferDescriptorABW *>(curr_ptr));
            curr_ptr += sizeof(BufferDescriptorABW);
        }
        for (uint index = 0; header->b_no > index; index++) {
            vec_buf_b.push_back(reinterpret_cast<BufferDescriptorABW *>(curr_ptr));
            curr_ptr += sizeof(BufferDescriptorABW);
        }
        for (uint index = 0; header->w_no > index; index++) {
            vec_buf_w.push_back(reinterpret_cast<BufferDescriptorABW *>(curr_ptr));
            curr_ptr += sizeof(BufferDescriptorABW);
        }
        auto raw_ptr = reinterpret_cast<u8 *>((((reinterpret_cast<u64>(curr_ptr) - reinterpret_cast<u64>(tls.data())) - 1U) & ~(constant::padding_sum - 1U)) + constant::padding_sum + reinterpret_cast<u64>(tls.data())); // Align to 16 bytes relative to start of TLS
        if (is_domain) {
            domain = reinterpret_cast<DomainHeaderRequest *>(raw_ptr);
            payload = reinterpret_cast<PayloadHeader *>(raw_ptr + sizeof(DomainHeaderRequest));
            cmd_arg_sz = domain->payload_sz - sizeof(PayloadHeader);
        } else {
            payload = reinterpret_cast<PayloadHeader *>(raw_ptr);
            cmd_arg_sz = (header->raw_sz * sizeof(u32)) - (constant::padding_sum + sizeof(PayloadHeader));
        }
        if (payload->magic != constant::sfci_magic) throw exception(fmt::format("Unexpected Magic in PayloadHeader: 0x{:X}", reinterpret_cast<u32>(payload->magic)));
        cmd_arg = reinterpret_cast<u8 *>(payload) + sizeof(PayloadHeader);
        curr_ptr += header->raw_sz * sizeof(u32);
        if (header->c_flag == static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            vec_buf_c.push_back(reinterpret_cast<BufferDescriptorC *>(curr_ptr));
        } else if (header->c_flag > static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            for (uint index = 0; (header->c_flag - 2) > index; index++) { // (c_flag - 2) C descriptors are present
                vec_buf_c.push_back(reinterpret_cast<BufferDescriptorC *>(curr_ptr));
                curr_ptr += sizeof(BufferDescriptorC);
            }
        }
    }

    IpcResponse::IpcResponse(bool is_domain, device_state &state) : is_domain(is_domain), state(state) {}

    void IpcResponse::WriteTls() {
        std::array<u8, constant::tls_ipc_size> tls{};
        u8 *curr_ptr = tls.data();
        auto header = reinterpret_cast<CommandHeader *>(curr_ptr);
        header->x_no = static_cast<u8>(vec_buf_x.size());
        header->a_no = static_cast<u8>(vec_buf_a.size());
        header->b_no = static_cast<u8>(vec_buf_b.size());
        header->w_no = static_cast<u8>(vec_buf_w.size());
        header->raw_sz = static_cast<u32>((sizeof(PayloadHeader) + arg_vec.size() + constant::padding_sum + (is_domain ? sizeof(DomainHeaderRequest) : 0)) / sizeof(u32)); // Size is in 32-bit units because Nintendo
        if (!vec_buf_c.empty())
            header->c_flag = (vec_buf_c.size() == 1) ? static_cast<u8>(BufferCFlag::SingleDescriptor) : static_cast<u8>(vec_buf_c.size() + static_cast<u8>(BufferCFlag::SingleDescriptor));
        header->handle_desc = (!copy_handles.empty() || !move_handles.empty());
        curr_ptr += sizeof(CommandHeader);
        if (header->handle_desc) {
            auto handle_desc = reinterpret_cast<HandleDescriptor *>(curr_ptr);
            handle_desc->send_pid = false; // TODO: Figure this out ?
            handle_desc->copy_count = static_cast<u8>(copy_handles.size());
            handle_desc->move_count = static_cast<u8>(move_handles.size());
            curr_ptr += sizeof(HandleDescriptor);
            for (uint index = 0; handle_desc->copy_count > index; index++) {
                *reinterpret_cast<handle_t *>(curr_ptr) = copy_handles[index];
                curr_ptr += sizeof(handle_t);
            }
            for (uint index = 0; handle_desc->move_count > index; index++) {
                *reinterpret_cast<handle_t *>(curr_ptr) = move_handles[index];
                curr_ptr += sizeof(handle_t);
            }
        }
        for (uint index = 0; header->x_no > index; index++) {
            *reinterpret_cast<BufferDescriptorX *>(curr_ptr) = vec_buf_x[index];
            curr_ptr += sizeof(BufferDescriptorX);
        }
        for (uint index = 0; header->a_no > index; index++) {
            *reinterpret_cast<BufferDescriptorABW *>(curr_ptr) = vec_buf_a[index];
            curr_ptr += sizeof(BufferDescriptorABW);
        }
        for (uint index = 0; header->b_no > index; index++) {
            *reinterpret_cast<BufferDescriptorABW *>(curr_ptr) = vec_buf_b[index];
            curr_ptr += sizeof(BufferDescriptorABW);
        }
        for (uint index = 0; header->w_no > index; index++) {
            *reinterpret_cast<BufferDescriptorABW *>(curr_ptr) = vec_buf_w[index];
            curr_ptr += sizeof(BufferDescriptorABW);
        }
        u64 padding = ((((reinterpret_cast<u64>(curr_ptr) - reinterpret_cast<u64>(tls.data())) - 1U) & ~(constant::padding_sum - 1U)) + constant::padding_sum + (reinterpret_cast<u64>(tls.data()) - reinterpret_cast<u64>(curr_ptr))); // Calculate the amount of padding at the front
        curr_ptr += padding;
        PayloadHeader *payload;
        if (is_domain) {
            auto domain = reinterpret_cast<DomainHeaderResponse *>(curr_ptr);
            domain->output_count = 0; // TODO: Figure this out
            payload = reinterpret_cast<PayloadHeader *>(curr_ptr + sizeof(DomainHeaderResponse));
        } else {
            payload = reinterpret_cast<PayloadHeader *>(curr_ptr);
        }
        payload->magic = constant::sfco_magic;
        payload->version = 1;
        payload->value = error_code;
        curr_ptr += sizeof(PayloadHeader);
        if (!arg_vec.empty()) memcpy(curr_ptr, arg_vec.data(), arg_vec.size());
        curr_ptr += arg_vec.size() + (constant::padding_sum - padding);
        if (header->c_flag == static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            *reinterpret_cast<BufferDescriptorC *>(curr_ptr) = vec_buf_c[0];
        } else if (header->c_flag > static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            for (uint index = 0; (header->c_flag - 2) > index; index++) {
                *reinterpret_cast<BufferDescriptorC *>(curr_ptr) = vec_buf_c[index];
                curr_ptr += sizeof(BufferDescriptorC);
            }
        }
    }
}
