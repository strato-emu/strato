#pragma once

#include "kernel/ipc.h"
#include "kernel/types/KEvent.h"
#include "gpu/display.h"
#include "gpu/devices/nvdevice.h"

namespace skyline::gpu {
    /**
     * @brief This is used to converge all of the interfaces to the GPU and send the results to a GPU API
     * @note We opted for just supporting a single layer and display as it's what basically all games use and wasting cycles on it is pointless
     */
    class GPU {
      private:
        ANativeWindow *window; //!< The ANativeWindow to render to
        const DeviceState &state; //!< The state of the device
        u32 fdIndex{}; //!< Holds the index of a file descriptor
        std::unordered_map<device::NvDeviceType, std::shared_ptr<device::NvDevice>> deviceMap; //!< A map from a NvDeviceType to the NvDevice object
        std::unordered_map<u32, std::shared_ptr<device::NvDevice>> fdMap; //!< A map from an FD to a shared pointer to it's NvDevice object
        bool surfaceUpdate{}; //!< If the surface needs to be updated
        double prevTime{};

      public:
        DisplayId displayId{DisplayId::Null}; //!< The ID of this display
        LayerStatus layerStatus{LayerStatus::Uninitialized}; //!< This is the status of the single layer the display has
        BufferQueue bufferQueue; //!< This holds all of the buffers to be pushed onto the display
        Resolution resolution{}; //!< The resolution of the display window
        i32 format{}; //!< The format of the display window
        std::shared_ptr<kernel::type::KEvent> vsyncEvent; //!< This KEvent is triggered every time a frame is drawn
        std::shared_ptr<kernel::type::KEvent> bufferEvent; //!< This KEvent is triggered every time a buffer is freed

        /**
         * @param window The ANativeWindow to render to
         */
        GPU(const DeviceState &state);

        /**
         * @brief The destructor for the GPU class
         */
        ~GPU();

        /**
         * @brief The loop that executes routine GPU functions
         */
        void Loop();

        /**
         * @brief Open a specific device and return a FD
         * @param path The path of the device to open an FD to
         * @return The file descriptor to the device
         */
        u32 OpenDevice(const std::string &path);

        /**
         * @brief Close the specified FD
         * @param fd The file descriptor to close
         */
        void CloseDevice(u32 fd);

        /**
         * @brief Returns a particular device with a specific FD
         * @tparam objectClass The class of the device to return
         * @param fd The file descriptor to retrieve
         * @return A shared pointer to the device
         */
        template<typename objectClass>
        std::shared_ptr<objectClass> GetDevice(u32 fd) {
            try {
                auto item = fdMap.at(fd);
                return std::static_pointer_cast<objectClass>(item);
            } catch (std::out_of_range) {
                throw exception("GetDevice was called with invalid file descriptor: 0x{:X}", fd);
            }
        }

        /**
         * @brief Returns a particular device with a specific type
         * @tparam objectClass The class of the device to return
         * @param type The type of the device to return
         * @return A shared pointer to the device
         */
        template<typename objectClass>
        std::shared_ptr<objectClass> GetDevice(device::NvDeviceType type) {
            try {
                auto item = deviceMap.at(type);
                return std::static_pointer_cast<objectClass>(item);
            } catch (std::out_of_range) {
                throw exception("GetDevice was called with invalid type: 0x{:X}", type);
            }
        }

        /**
         * @brief Process an Ioctl request to a device
         * @param fd The file descriptor the Ioctl was issued to
         * @param cmd The command of the Ioctl
         * @param request The IPC request object
         * @param response The IPC response object
         */
        void Ioctl(u32 fd, u32 cmd, kernel::ipc::IpcRequest &request, kernel::ipc::IpcResponse &response);

        /**
         * @brief This sets displayId to a specific display type
         * @param name The name of the display
         * @note displayId has to be DisplayId::Null or this will throw an exception
         */
        void SetDisplay(const std::string &name);

        /**
         * @brief This closes the display by setting displayId to DisplayId::Null
         */
        void CloseDisplay();
    };
}
