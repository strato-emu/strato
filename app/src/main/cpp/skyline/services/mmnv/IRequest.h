// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::mmnv {
    /**
     * @brief IRequest or mm:u is used to control clocks and powergating of various hardware modules
     * @url https://switchbrew.org/wiki/Display_services#mm:u
     */
    class IRequest : public BaseService {
      private:
        /**
         * @brief Enumerates the modules that can be controlled by mmnv, these are passed directly to FGM services
         */
        enum class ModuleType : u32 {
            Ram = 2,
            NvEnc = 5,
            NvDec = 6,
            NvJpg = 7
        };

        /**
         * @brief Holds a single mmnv request, detailing its target module and current frequency
         */
        struct Request {
            ModuleType module;
            u32 freqHz;
        };

        std::mutex requestsMutex; // Protects accesses to requests
        std::vector<std::optional<Request>> requests; //!< Holds allocated requests with the index corresponding to the request ID

        /**
         * @brief Holds the result of a request allocation
         */
        struct Allocation {
            Request &request;
            u32 id;
        };

        /*
         * @note requestsMutex should be locked when calling this
         * @return A reference to an empty request
         */
        Allocation AllocateRequest();

      public:
        IRequest(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Initialises the request for the given module ID
         */
        Result InitializeOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Finalises the request for the given module ID
         */
        Result FinalizeOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the target frequency in HZ for the given module and waits for it to be applied
         */
        Result SetAndWaitOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Gets the frequency in HZ for the given module
         */
        Result GetOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
          * @brief Initialises a new request for the given module ID and returns a new request ID
          */
        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Finalises the request with the given ID
         */
        Result Finalize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the target frequency in HZ for the request with the given ID and waits for it to be applied
         */
        Result SetAndWait(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Gets the frequency in HZ for the request with the given ID
         */
        Result Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IRequest, InitializeOld),
            SFUNC(0x1, IRequest, FinalizeOld),
            SFUNC(0x2, IRequest, SetAndWaitOld),
            SFUNC(0x3, IRequest, GetOld),
            SFUNC(0x4, IRequest, Initialize),
            SFUNC(0x5, IRequest, Finalize),
            SFUNC(0x6, IRequest, SetAndWait),
            SFUNC(0x7, IRequest, Get)
        )
    };
}
