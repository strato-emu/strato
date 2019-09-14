#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "switch/common.h"

namespace lightSwitch::kernel::ipc {
    /**
     * This bit-field structure holds the header of an IPC command.
     * https://switchbrew.org/wiki/IPC_Marshalling#IPC_Command_Structure
     */
    struct CommandHeader {
        u16 type    : 16;
        u8 x_no     : 4;
        u8 a_no     : 4;
        u8 b_no     : 4;
        u8 w_no     : 4;
        u32 raw_sz  : 10;
        u8 c_flag   : 4;
        u32         : 17;
        bool handle_desc : 1;
    };
    static_assert(sizeof(CommandHeader) == 8);

    /**
     * This reflects the value in CommandStruct::type
     */
    enum class CommandType : u16 {
        Invalid = 0, LegacyRequest = 1, Close = 2, LegacyControl = 3, Request = 4, Control = 5, RequestWithContext = 6, ControlWithContext = 7
    };

    /**
     * This reflects the value in CommandStruct::c_flags
     */
    enum class BufferCFlag : u8 {
        None = 0, InlineDescriptor = 1, SingleDescriptor = 2
    };

    /**
     * This bit-field structure holds the handle descriptor of a recieved IPC command.
     * https://switchbrew.org/wiki/IPC_Marshalling#Handle_descriptor
     */
    struct HandleDescriptor {
        bool send_pid  : 1;
        u32 copy_count : 4;
        u32 move_count : 4;
        u32            : 23;
    };
    static_assert(sizeof(HandleDescriptor) == 4);

    /**
     * This bit-field structure holds the domain's header of an IPC request command.
     * https://switchbrew.org/wiki/IPC_Marshalling#Domains
     */
    struct DomainHeaderRequest {
        u8 command     : 8;
        u8 input_count : 8;
        u16 payload_sz : 16;
        u32 object_id  : 32;
        u32            : 32;
        u32 token      : 32;
    };
    static_assert(sizeof(DomainHeaderRequest) == 16);

    /**
     * This bit-field structure holds the domain's header of an IPC response command.
     * https://switchbrew.org/wiki/IPC_Marshalling#Domains
     */
    struct DomainHeaderResponse {
        u64 output_count : 32;
        u64              : 32;
        u64              : 64;
    };
    static_assert(sizeof(DomainHeaderResponse) == 16);

    /**
     * This bit-field structure holds the data payload of an IPC command.
     * https://switchbrew.org/wiki/IPC_Marshalling#Data_payload
     */
    struct PayloadHeader {
        u32 magic   : 32;
        u32 version : 32;
        u32 value   : 32;
        u32 token   : 32;
    };
    static_assert(sizeof(PayloadHeader) == 16);

    /**
     * This is a buffer descriptor for X buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_X_.22Pointer.22
     */
    struct BufferDescriptorX {
        u16 counter_0_5   : 6;
        u16 address_36_38 : 3;
        u16 counter_9_11  : 3;
        u16 address_32_35 : 4;
        u16 size          : 16;
        u32 address_0_31  : 32;

        BufferDescriptorX(u64 address, u16 counter, u16 size) : size(size) {
            // Test: The AND mask might be the other way around
            address_0_31 = static_cast<u32>(address & 0x7FFFFFFF80000000);
            address_32_35 = static_cast<u16>(address & 0x78000000);
            address_36_38 = static_cast<u16>(address & 0x7000000);
            counter_0_5 = static_cast<u16>(address & 0x7E00);
            counter_9_11 = static_cast<u16>(address & 0x38);
        }

        inline u64 Address() const {
            return static_cast<u64>(address_0_31) | static_cast<u64>(address_32_35) << 32 | static_cast<u64>(address_36_38) << 36;
        }

        inline u16 Counter() const {
            return static_cast<u16>(counter_0_5) | static_cast<u16>(counter_9_11) << 9;
        }
    };

    static_assert(sizeof(BufferDescriptorX) == 8);

    /**
     * This is a buffer descriptor for A/B/W buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_A.2FB.2FW_.22Send.22.2F.22Receive.22.2F.22Exchange.22
     */
    struct BufferDescriptorABW {
        u32 size_0_31    : 32;
        u32 address_0_31 : 32;
        u8 flags         : 2;
        u8 address_36_38 : 3;
        u32              : 19;
        u8 size_32_35    : 4;
        u8 address_32_35 : 4;

        BufferDescriptorABW(u64 address, u64 size) {
            address_0_31 = static_cast<u32>(address & 0x7FFFFFFF80000000);
            address_32_35 = static_cast<u8>(address & 0x78000000);
            address_36_38 = static_cast<u8>(address & 0x7000000);
            size_0_31 = static_cast<u32>(size & 0x7FFFFFFF80000000);
            size_32_35 = static_cast<u8>(size & 0x78000000);
        }

        inline u64 Address() const {
            return static_cast<u64>(address_0_31) | static_cast<u64>(address_32_35) << 32 | static_cast<u64>(address_36_38) << 36;
        }

        inline u64 Size() const {
            return static_cast<u64>(size_0_31) | static_cast<u64>(size_32_35) << 32;
        }
    };

    static_assert(sizeof(BufferDescriptorABW) == 12);

    /**
     * This is a buffer descriptor for C buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_C_.22ReceiveList.22
     */
    struct BufferDescriptorC {
        u32 address_0_31  : 32;
        u16 address_32_48 : 16;
        u16 size          : 16;

        BufferDescriptorC(u64 address, u16 size) : size(size) {
            address_0_31 = static_cast<u32>(address & 0x7FFFFFFF80000000);
            address_32_48 = static_cast<u16>(address & 0x7FFFC000);
        }

        inline u64 Address() const {
            return static_cast<u64>(address_0_31) | static_cast<u64>(address_32_48) << 32;
        }
    };

    static_assert(sizeof(BufferDescriptorC) == 8);

    class IpcRequest {
      private:
        device_state &state;

      public:
        std::array<u8, constant::tls_ipc_size> tls;
        CommandHeader *header{};
        HandleDescriptor *handle_desc{};
        bool is_domain{};
        DomainHeaderRequest *domain{};
        PayloadHeader *payload{};
        u8 *cmd_arg{};
        u64 cmd_arg_sz{};
        std::vector<handle_t> copy_handles;
        std::vector<handle_t> move_handles;
        std::vector<BufferDescriptorX *> vec_buf_x;
        std::vector<BufferDescriptorABW *> vec_buf_a;
        std::vector<BufferDescriptorABW *> vec_buf_b;
        std::vector<BufferDescriptorABW *> vec_buf_w;
        std::vector<BufferDescriptorC *> vec_buf_c;

        IpcRequest(bool is_domain, device_state &state);
    };

    class IpcResponse {
      private:
        std::vector<u8> arg_vec;
        device_state &state;

      public:
        bool is_domain{};
        u32 error_code{};
        std::vector<handle_t> copy_handles;
        std::vector<handle_t> move_handles;
        std::vector<BufferDescriptorX> vec_buf_x;
        std::vector<BufferDescriptorABW> vec_buf_a;
        std::vector<BufferDescriptorABW> vec_buf_b;
        std::vector<BufferDescriptorABW> vec_buf_w;
        std::vector<BufferDescriptorC> vec_buf_c;

        IpcResponse(bool is_domain, device_state &state);

        template<typename T>
        void WriteValue(const T &value) {
            arg_vec.reserve(arg_vec.size() + sizeof(T));
            auto item = reinterpret_cast<const u8 *>(&value);
            for (uint index = 0; sizeof(T) > index; index++) {
                arg_vec.push_back(*item);
                item++;
            }
        }

        void WriteTls();
    };
}
