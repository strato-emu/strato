// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "types/KSession.h"
#include "types/KProcess.h"

namespace skyline {
    namespace constant {
        constexpr u8 IpcPaddingSum{0x10}; // The sum of the padding surrounding the data payload
        constexpr u16 TlsIpcSize{0x100}; // The size of the IPC command buffer in a TLS slot
    }

    namespace kernel::ipc {
        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Type
         */
        enum class CommandType : u16 {
            Invalid = 0,
            LegacyRequest = 1,
            Close = 2,              //!< Closes the IPC Session
            LegacyControl = 3,
            Request = 4,            //!< A normal IPC transaction between the server and client process
            Control = 5,            //!< A transaction between the client and IPC Manager
            RequestWithContext = 6, //!< Request with Token
            ControlWithContext = 7, //!< Control with Token
            TipcCloseSession = 0xF,
        };

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_C_.22ReceiveList.22
         * @note Any values beyond this are the amount of C-Buffers present (Which is calculated as value - 2)
         */
        enum class BufferCFlag : u8 {
            None = 0,             //!< No C-Buffers present
            InlineDescriptor = 1, //!< An inlined C-Buffer which is written after the raw data section
            SingleDescriptor = 2, //!< A single C-Buffer
        };

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#IPC_Command_Structure
         */
        struct CommandHeader {
            CommandType type : 16;
            u8 xNo : 4;
            u8 aNo : 4;
            u8 bNo : 4;
            u8 wNo : 4;
            u32 rawSize : 10;
            BufferCFlag cFlag : 4;
            u32               : 17;
            bool handleDesc : 1;
        };
        static_assert(sizeof(CommandHeader) == 8);

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Handle_descriptor
         */
        struct HandleDescriptor {
            bool sendPid : 1;
            u32 copyCount : 4;
            u32 moveCount : 4;
            u32           : 23;
        };
        static_assert(sizeof(HandleDescriptor) == 4);

        /**
         * @brief Commands which can be issued via a domain request
         */
        enum class DomainCommand : u8 {
            SendMessage = 1,
            CloseVHandle = 2,
        };

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Domains
         */
        struct DomainHeaderRequest {
            DomainCommand command;
            u8 inputCount;
            u16 payloadSz;
            u32 objectId;
            u32 : 32;
            u32 token;
        };
        static_assert(sizeof(DomainHeaderRequest) == 16);

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Domains
         */
        struct DomainHeaderResponse {
            u32 outputCount;
            u32 : 32;
            u64 : 64;
        };
        static_assert(sizeof(DomainHeaderResponse) == 16);

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Data_payload
         */
        struct PayloadHeader {
            u32 magic;
            u32 version;
            u32 value;
            u32 token;
        };
        static_assert(sizeof(PayloadHeader) == 16);


        /**
         * @brief The IPC Control commands as encoded into the payload value
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Control
         */
        enum class ControlCommand : u32 {
            ConvertCurrentObjectToDomain = 0, //!< Converts a regular IPC session into a domain session
            CopyFromCurrentDomain = 1,
            CloneCurrentObject = 2,           //!< Creates a duplicate of the current session
            QueryPointerBufferSize = 3,       //!< The size of the X buffers written by the servers (and by extension C-buffers supplied by the client)
            CloneCurrentObjectEx = 4,         //!< Same as CloneCurrentObject
        };

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_X_.22Pointer.22
         */
        struct BufferDescriptorX {
            u16 counter0_5 : 6; //!< The first 5 bits of the counter
            u16 address36_38 : 3; //!< Bit 36-38 of the address
            u16 counter9_11 : 3; //!< Bit 9-11 of the counter
            u16 address32_35 : 4; //!< Bit 32-35 of the address
            u16 size : 16; //!< The 16 bit size of the buffer
            u32 address0_31 : 32; //!< The first 32-bits of the address

            u8 *Pointer() {
                return reinterpret_cast<u8 *>(static_cast<u64>(address0_31) | static_cast<u64>(address32_35) << 32 | static_cast<u64>(address36_38) << 36);
            }

            u16 Counter() {
                return static_cast<u16>(counter0_5) | static_cast<u16>(static_cast<u16>(counter9_11) << 9);
            }
        };
        static_assert(sizeof(BufferDescriptorX) == 8);

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_A.2FB.2FW_.22Send.22.2F.22Receive.22.2F.22Exchange.22
         */
        struct BufferDescriptorABW {
            u32 size0_31 : 32; //!< The first 32 bits of the size
            u32 address0_31 : 32; //!< The first 32 bits of the address
            u8 flags : 2; //!< The buffer flags
            u8 address36_38 : 3; //!< Bit 36-38 of the address
            u32             : 19;
            u8 size32_35 : 4; //!< Bit 32-35 of the size
            u8 address32_35 : 4; //!< Bit 32-35 of the address

            u8 *Pointer() {
                return reinterpret_cast<u8 *>(static_cast<u64>(address0_31) | static_cast<u64>(address32_35) << 32 | static_cast<u64>(address36_38) << 36);
            }

            u64 Size() {
                return static_cast<u64>(size0_31) | static_cast<u64>(size32_35) << 32;
            }
        };
        static_assert(sizeof(BufferDescriptorABW) == 12);

        /**
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Buffer_descriptor_C_.22ReceiveList.22
         */
        struct BufferDescriptorC {
            u64 address : 48; //!< The 48-bit address of the buffer
            u32 size : 16; //!< The 16-bit size of the buffer

            u8 *Pointer() {
                return reinterpret_cast<u8 *>(address);
            }
        };
        static_assert(sizeof(BufferDescriptorC) == 8);

        enum class IpcBufferType {
            X, //!< Type-X buffer (BufferDescriptorX)
            A, //!< Type-A buffer (BufferDescriptorABW)
            B, //!< Type-B buffer (BufferDescriptorABW)
            W, //!< Type-W buffer (BufferDescriptorABW)
            C, //!< Type-C buffer (BufferDescriptorC)
        };

        /**
         * @brief A wrapper over an IPC Request which allows it to be parsed and used effectively
         * @url https://switchbrew.org/wiki/IPC_Marshalling
         */
        class IpcRequest {
          private:
            u8 *payloadOffset; //!< The offset of the data read from the payload

          public:
            CommandHeader *header{};
            HandleDescriptor *handleDesc{};
            bool isDomain{}; //!< If this is a domain request
            bool isTipc; //!< If this is request uses the TIPC protocol
            DomainHeaderRequest *domain{};
            PayloadHeader *payload{};
            u8 *cmdArg{}; //!< A pointer to the data payload
            u64 cmdArgSz{}; //!< The size of the data payload
            boost::container::small_vector<KHandle, 2> copyHandles; //!< The handles that should be copied from the server to the client process (The difference is just to match application expectations, there is no real difference b/w copying and moving handles)
            boost::container::small_vector<KHandle, 2> moveHandles; //!< The handles that should be moved from the server to the client process rather than copied
            boost::container::small_vector<KHandle, 2> domainObjects;
            boost::container::small_vector<span<u8>, 3> inputBuf;
            boost::container::small_vector<span<u8>, 3> outputBuf;

            IpcRequest(bool isDomain, const DeviceState &state);

            /**
             * @brief Returns a reference to an item from the top of the payload
             */
            template<typename ValueType>
            ValueType &Pop() {
                ValueType &value{*reinterpret_cast<ValueType *>(payloadOffset)};
                payloadOffset += sizeof(ValueType);
                return value;
            }

            /**
             * @brief Returns a std::string_view from the payload
             * @param size The length of the string (0 should only be used with nullTerminated and automatically determines size)
             * @param nullTerminated If the returned view should only encapsulate a null terminated substring
             */
            std::string_view PopString(size_t size = 0, bool nullTerminated = true) {
                size = size ? size : cmdArgSz - reinterpret_cast<u64>(payloadOffset);
                auto view{span(payloadOffset, size).as_string(nullTerminated)};
                if (nullTerminated)
                    payloadOffset += size;
                else
                    payloadOffset += view.length();
                return view;
            }

            /**
             * @brief Pops a Service object from the response as a domain or kernel handle
             */
            template<typename ServiceType>
            std::shared_ptr<ServiceType> PopService(u32 id, type::KSession &session) {
                std::shared_ptr<service::BaseService> serviceObject;
                if (session.isDomain)
                    serviceObject = session.domains.at(domainObjects.at(id));
                else
                    serviceObject = session.state.process->GetHandle<kernel::type::KSession>(moveHandles.at(id))->serviceObject;

                return std::static_pointer_cast<ServiceType>(serviceObject);
            }

            /**
             * @brief Skips an object to pop off the top
             */
            template<typename ValueType>
            void Skip() {
                payloadOffset += sizeof(ValueType);
            }
        };

        /**
         * @brief A wrapper over an IPC Response which allows it to be defined and serialized efficiently
         * @url https://switchbrew.org/wiki/IPC_Marshalling
         */
        class IpcResponse {
          private:
            const DeviceState &state;
            std::vector<u8> payload; //!< The contents to be pushed to the data payload

          public:
            Result errorCode{}; //!< The error code to respond with, it's 0 (Success) by default
            boost::container::small_vector<KHandle, 2> copyHandles;
            boost::container::small_vector<KHandle, 2> moveHandles;
            boost::container::small_vector<KHandle, 2> domainObjects;

            IpcResponse(const DeviceState &state);

            /**
             * @brief Writes an object to the payload
             * @tparam ValueType The type of the object to write
             * @param value A reference to the object to be written
             */
            template<typename ValueType>
            void Push(const ValueType &value) {
                auto size{payload.size()};
                payload.resize(size + sizeof(ValueType));
                std::memcpy(payload.data() + size, reinterpret_cast<const u8 *>(&value), sizeof(ValueType));
            }

            /**
             * @brief Writes a string to the payload
             * @param string The string to write to the payload
             */
            void Push(std::string_view string) {
                auto size{payload.size()};
                payload.resize(size + string.size());
                std::memcpy(payload.data() + size, string.data(), string.size());
            }

            /**
             * @brief Writes this IpcResponse object's contents into TLS
             * @param isDomain Indicates if this is a domain response
             * @param isTipc Indicates if this is a TIPC response
             */
            void WriteResponse(bool isDomain, bool isTipc = false);
        };
    }
}
