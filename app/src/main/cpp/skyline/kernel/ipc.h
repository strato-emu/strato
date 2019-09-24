#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "../common.h"

namespace skyline::kernel::ipc {
    /**
     * @brief This bit-field structure holds the header of an IPC command. (https://switchbrew.org/wiki/IPC_Marshalling#IPC_Command_Structure)
     */
    struct CommandHeader {
        u16 type         : 16;
        u8 x_no          : 4;
        u8 a_no          : 4;
        u8 b_no          : 4;
        u8 w_no          : 4;
        u32 raw_sz       : 10;
        u8 c_flag        : 4;
        u32              : 17;
        bool handle_desc : 1;
    };
    static_assert(sizeof(CommandHeader) == 8);

    /**
     * @brief This reflects the value in CommandStruct::type
     */
    enum class CommandType : u16 {
        Invalid = 0, LegacyRequest = 1, Close = 2, LegacyControl = 3, Request = 4, Control = 5, RequestWithContext = 6, ControlWithContext = 7
    };

    /**
     * @brief This reflects the value in CommandStruct::c_flags
     */
    enum class BufferCFlag : u8 {
        None = 0, InlineDescriptor = 1, SingleDescriptor = 2
    };

    /**
     * @brief This bit-field structure holds the handle descriptor of a received IPC command. (https://switchbrew.org/wiki/IPC_Marshalling#Handle_descriptor)
     */
    struct HandleDescriptor {
        bool send_pid  : 1;
        u32 copy_count : 4;
        u32 move_count : 4;
        u32            : 23;
    };
    static_assert(sizeof(HandleDescriptor) == 4);

    /**
     * @brief This bit-field structure holds the domain's header of an IPC request command. (https://switchbrew.org/wiki/IPC_Marshalling#Domains)
     */
    struct DomainHeaderRequest {
        u8 command;
        u8 input_count;
        u16 payload_sz;
        u32 object_id;
        u32 : 32;
        u32 token;
    };
    static_assert(sizeof(DomainHeaderRequest) == 16);

    /**
     * @brief This reflects the value of DomainHeaderRequest::command
     */
    enum class DomainCommand : u8 {
        SendMessage = 1, CloseVHandle = 2
    };

    /**
     * @brief This bit-field structure holds the domain's header of an IPC response command. (https://switchbrew.org/wiki/IPC_Marshalling#Domains)
     */
    struct DomainHeaderResponse {
        u32 output_count;
        u32 : 32;
        u64 : 64;
    };
    static_assert(sizeof(DomainHeaderResponse) == 16);

    /**
     * @brief This bit-field structure holds the data payload of an IPC command. (https://switchbrew.org/wiki/IPC_Marshalling#Data_payload)
     */
    struct PayloadHeader {
        u32 magic;
        u32 version;
        u32 value;
        u32 token;
    };
    static_assert(sizeof(PayloadHeader) == 16);


    /**
     * @brief This reflects which function PayloadHeader::value refers to when a control request is sent (https://switchbrew.org/wiki/IPC_Marshalling#Control)
     */
    enum class ControlCommand : u32 {
        ConvertCurrentObjectToDomain = 0, CopyFromCurrentDomain = 1, CloneCurrentObject = 2, QueryPointerBufferSize = 3, CloneCurrentObjectEx = 4
    };

    /**
     * @brief This is a buffer descriptor for X buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_X_.22Pointer.22
     */
    struct BufferDescriptorX {
        u16 counter_0_5   : 6;
        u16 address_36_38 : 3;
        u16 counter_9_11  : 3;
        u16 address_32_35 : 4;
        u16 size          : 16;
        u32 address_0_31  : 32;

        BufferDescriptorX(u64 address, u16 counter, u16 size) : size(size) {
            // TODO: Test this, the AND mask might be the other way around
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
     * @brief This is a buffer descriptor for A/B/W buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_A.2FB.2FW_.22Send.22.2F.22Receive.22.2F.22Exchange.22
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
     * @brief This is a buffer descriptor for C buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_C_.22ReceiveList.22
     */
    struct BufferDescriptorC {
        u64 address : 48;
        u16 size    : 16;

        BufferDescriptorC(u64 address, u16 size) : address(address), size(size) {}
    };

    static_assert(sizeof(BufferDescriptorC) == 8);

    /**
     * @brief This class encapsulates an IPC Request (https://switchbrew.org/wiki/IPC_Marshalling)
     */
    class IpcRequest {
      private:
        const DeviceState &state; //!< The state of the device

      public:
        std::array<u8, constant::TlsIpcSize> tls; //!< A static-sized array where TLS data is actually copied to
        CommandHeader *header{}; //!< The header of the request
        HandleDescriptor *handleDesc{}; //!< The handle descriptor in case CommandHeader::handle_desc is true in the header
        bool isDomain{}; //!< If this is a domain request
        DomainHeaderRequest *domain{}; //!< In case this is a domain request, this holds data regarding it
        PayloadHeader *payload{}; //!< This is the header of the payload
        u8 *cmdArg{}; //!< This is a pointer to the data payload (End of PayloadHeader)
        u64 cmdArgSz{}; //!< This is the size of the data payload
        std::vector<handle_t> copyHandles; //!< A vector of handles that should be copied from the server to the client process (The difference is just to match application expectations, there is no real difference b/w copying and moving handles)
        std::vector<handle_t> moveHandles; //!< A vector of handles that should be moved from the server to the client process rather than copied
        std::vector<handle_t> domainObjects; //!< A vector of all input domain objects
        std::vector<BufferDescriptorX *> vecBufX; //!< This is a vector of pointers to X Buffer Descriptors
        std::vector<BufferDescriptorABW *> vecBufA; //!< This is a vector of pointers to A Buffer Descriptors
        std::vector<BufferDescriptorABW *> vecBufB; //!< This is a vector of pointers to B Buffer Descriptors
        std::vector<BufferDescriptorABW *> vecBufW; //!< This is a vector of pointers to W Buffer Descriptors
        std::vector<BufferDescriptorC *> vecBufC; //!< This is a vector of pointers to C Buffer Descriptors

        /**
         * @param isDomain If the following request is a domain request
         * @param state The state of the device
         */
        IpcRequest(bool isDomain, const DeviceState &state);
    };

    /**
     * @brief This class encapsulates an IPC Response (https://switchbrew.org/wiki/IPC_Marshalling)
     */
    class IpcResponse {
      private:
        std::vector<u8> argVec; //!< This holds all of the contents to be pushed to the payload
        const DeviceState &state; //!< The state of the device

      public:
        bool isDomain{}; //!< If this is a domain request
        u32 errorCode{}; //!< The error code to respond with, it is 0 (Success) by default
        std::vector<handle_t> copyHandles; //!< A vector of handles to copy
        std::vector<handle_t> moveHandles; //!< A vector of handles to move
        std::vector<handle_t> domainObjects; //!< A vector of domain objects to write

        /**
         * @param isDomain If the following request is a domain request
         * @param state The state of the device
         */
        IpcResponse(bool isDomain, const DeviceState &state);

        /**
         * @brief Writes an object to the payload
         * @tparam ValueType The type of the object to write
         * @param value The object to be written
         */
        template<typename ValueType>
        void WriteValue(const ValueType &value) {
            argVec.reserve(argVec.size() + sizeof(ValueType));
            auto item = reinterpret_cast<const u8 *>(&value);
            for (uint index = 0; sizeof(ValueType) > index; index++) {
                argVec.push_back(*item);
                item++;
            }
        }

        /**
         * @brief Writes this IpcResponse object's contents into TLS
         */
        void WriteTls();
    };
}
