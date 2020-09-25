// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <common.h>

namespace skyline {
    namespace constant {
        constexpr auto IpcPaddingSum = 0x10; // The sum of the padding surrounding the data payload
        constexpr auto TlsIpcSize = 0x100; // The size of the IPC command buffer in a TLS slot
    }

    namespace kernel::ipc {
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
         * @brief This bit-field structure holds the header of an IPC command. (https://switchbrew.org/wiki/IPC_Marshalling#IPC_Command_Structure)
         */
        struct CommandHeader {
            CommandType type  : 16;
            u8 xNo            : 4;
            u8 aNo            : 4;
            u8 bNo            : 4;
            u8 wNo            : 4;
            u32 rawSize       : 10;
            BufferCFlag cFlag : 4;
            u32               : 17;
            bool handleDesc   : 1;
        };
        static_assert(sizeof(CommandHeader) == 8);

        /**
         * @brief This bit-field structure holds the handle descriptor of a received IPC command. (https://switchbrew.org/wiki/IPC_Marshalling#Handle_descriptor)
         */
        struct HandleDescriptor {
            bool sendPid  : 1;
            u32 copyCount : 4;
            u32 moveCount : 4;
            u32           : 23;
        };
        static_assert(sizeof(HandleDescriptor) == 4);

        /**
         * @brief This bit-field structure holds the domain's header of an IPC request command. (https://switchbrew.org/wiki/IPC_Marshalling#Domains)
         */
        struct DomainHeaderRequest {
            u8 command;
            u8 inputCount;
            u16 payloadSz;
            u32 objectId;
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
            u32 outputCount;
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
            u16 counter0_5   : 6; //!< The first 5 bits of the counter
            u16 address36_38 : 3; //!< Bit 36-38 of the address
            u16 counter9_11  : 3; //!< Bit 9-11 of the counter
            u16 address32_35 : 4; //!< Bit 32-35 of the address
            u16 size         : 16; //!< The 16 bit size of the buffer
            u32 address0_31  : 32; //!< The first 32-bits of the address

            BufferDescriptorX(u64 address, u16 counter, u16 size) : size(size) {
                address0_31 = static_cast<u32>(address & 0x7FFFFFFF80000000);
                address32_35 = static_cast<u16>(address & 0x78000000);
                address36_38 = static_cast<u16>(address & 0x7000000);
                counter0_5 = static_cast<u16>(address & 0x7E00);
                counter9_11 = static_cast<u16>(address & 0x38);
            }

            /**
             * @return The address of the buffer
             */
            inline u64 Address() {
                return static_cast<u64>(address0_31) | static_cast<u64>(address32_35) << 32 | static_cast<u64>(address36_38) << 36;
            }

            /**
             * @return The counter index of the buffer
             */
            inline u16 Counter() {
                return static_cast<u16>(counter0_5) | static_cast<u16>(counter9_11) << 9;
            }
        };
        static_assert(sizeof(BufferDescriptorX) == 8);

        /**
         * @brief This is a buffer descriptor for A/B/W buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_A.2FB.2FW_.22Send.22.2F.22Receive.22.2F.22Exchange.22
         */
        struct BufferDescriptorABW {
            u32 size0_31    : 32; //!< The first 32 bits of the size
            u32 address0_31 : 32; //!< The first 32 bits of the address
            u8 flags        : 2; //!< The buffer flags
            u8 address36_38 : 3; //!< Bit 36-38 of the address
            u32             : 19;
            u8 size32_35    : 4; //!< Bit 32-35 of the size
            u8 address32_35 : 4; //!< Bit 32-35 of the address

            BufferDescriptorABW(u64 address, u64 size) {
                address0_31 = static_cast<u32>(address & 0x7FFFFFFF80000000);
                address32_35 = static_cast<u8>(address & 0x78000000);
                address36_38 = static_cast<u8>(address & 0x7000000);
                size0_31 = static_cast<u32>(size & 0x7FFFFFFF80000000);
                size32_35 = static_cast<u8>(size & 0x78000000);
            }

            /**
             * @return The address of the buffer
             */
            inline u64 Address() {
                return static_cast<u64>(address0_31) | static_cast<u64>(address32_35) << 32 | static_cast<u64>(address36_38) << 36;
            }

            /**
             * @return The size of the buffer
             */
            inline u64 Size() {
                return static_cast<u64>(size0_31) | static_cast<u64>(size32_35) << 32;
            }
        };
        static_assert(sizeof(BufferDescriptorABW) == 12);

        /**
         * @brief This is a buffer descriptor for C buffers: https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_C_.22ReceiveList.22
         */
        struct BufferDescriptorC {
            u64 address : 48; //!< The 48-bit address of the buffer
            u32 size    : 16; //!< The 16-bit size of the buffer

            BufferDescriptorC(u64 address, u16 size) : address(address), size(size) {}
        };
        static_assert(sizeof(BufferDescriptorC) == 8);

        /**
         * @brief This enumerates the types of IPC buffers
         */
        enum class IpcBufferType {
            X, //!< This is a type-X buffer
            A, //!< This is a type-A buffer
            B, //!< This is a type-B buffer
            W, //!< This is a type-W buffer
            C //!< This is a type-C buffer
        };

        /**
         * @brief This class encapsulates an IPC Request (https://switchbrew.org/wiki/IPC_Marshalling)
         */
        class IpcRequest {
          private:
            u8 *payloadOffset; //!< This is the offset of the data read from the payload

          public:
            CommandHeader *header{}; //!< The header of the request
            HandleDescriptor *handleDesc{}; //!< The handle descriptor in case CommandHeader::handle_desc is true in the header
            bool isDomain{}; //!< If this is a domain request
            DomainHeaderRequest *domain{}; //!< In case this is a domain request, this holds data regarding it
            PayloadHeader *payload{}; //!< This is the header of the payload
            u8 *cmdArg{}; //!< This is a pointer to the data payload (End of PayloadHeader)
            u64 cmdArgSz{}; //!< This is the size of the data payload
            std::vector<KHandle> copyHandles; //!< A vector of handles that should be copied from the server to the client process (The difference is just to match application expectations, there is no real difference b/w copying and moving handles)
            std::vector<KHandle> moveHandles; //!< A vector of handles that should be moved from the server to the client process rather than copied
            std::vector<KHandle> domainObjects; //!< A vector of all input domain objects
            std::vector<span<u8>> inputBuf; //!< This is a vector of input buffers
            std::vector<span<u8>> outputBuf; //!< This is a vector of output buffers

            /**
             * @param isDomain If the following request is a domain request
             * @param state The state of the device
             */
            IpcRequest(bool isDomain, const DeviceState &state);

            /**
             * @brief This returns a reference to an item from the top of the payload
             * @tparam ValueType The type of the object to read
             */
            template<typename ValueType>
            inline ValueType &Pop() {
                ValueType &value = *reinterpret_cast<ValueType *>(payloadOffset);
                payloadOffset += sizeof(ValueType);
                return value;
            }

            /**
             * @brief This returns a std::string_view from the payload
             * @param size The length of the string (0 means the string is null terminated)
             */
            inline std::string_view PopString(size_t size = 0) {
                auto view = size ? std::string_view(reinterpret_cast<const char *>(payloadOffset), size) : std::string_view(reinterpret_cast<const char *>(payloadOffset));
                payloadOffset += view.length();
                return view;
            }

            /**
             * @brief This skips an object to pop off the top
             * @tparam ValueType The type of the object to skip
             */
            template<typename ValueType>
            inline void Skip() {
                payloadOffset += sizeof(ValueType);
            }
        };

        /**
         * @brief This class encapsulates an IPC Response (https://switchbrew.org/wiki/IPC_Marshalling)
         */
        class IpcResponse {
          private:
            const DeviceState &state; //!< The state of the device
            std::vector<u8> payload; //!< This holds all of the contents to be pushed to the payload

          public:
            Result errorCode{}; //!< The error code to respond with, it is 0 (Success) by default
            std::vector<KHandle> copyHandles; //!< A vector of handles to copy
            std::vector<KHandle> moveHandles; //!< A vector of handles to move
            std::vector<KHandle> domainObjects; //!< A vector of domain objects to write

            /**
             * @param isDomain If the following request is a domain request
             * @param state The state of the device
             */
            IpcResponse(const DeviceState &state);

            /**
             * @brief Writes an object to the payload
             * @tparam ValueType The type of the object to write
             * @param value A reference to the object to be written
             */
            template<typename ValueType>
            inline void Push(const ValueType &value) {
                auto size = payload.size();
                payload.resize(size + sizeof(ValueType));

                std::memcpy(payload.data() + size, reinterpret_cast<const u8 *>(&value), sizeof(ValueType));
            }

            /**
             * @brief Writes a string to the payload
             * @param string The string to write to the payload
             */
            template<>
            inline void Push(const std::string &string) {
                auto size = payload.size();
                payload.resize(size + string.size());

                std::memcpy(payload.data() + size, string.data(), string.size());
            }

            /**
             * @brief Writes this IpcResponse object's contents into TLS
             * @param isDomain Indicates if this is a domain response
             */
            void WriteResponse(bool isDomain);
        };
    }
}
