#pragma once

#include <common.h>
#include <queue>
#include <gpu/devices/nvmap.h>
#include "parcel.h"

namespace skyline::gpu {
    /**
     * @brief A struct that encapsulates a resolution
     */
    struct Resolution {
        u32 width; //!< The width component of the resolution
        u32 height; //!< The height component of the resolution

        inline bool operator==(const Resolution &r) {
            return (width == r.width) && (height == r.height);
        }

        inline bool operator!=(const Resolution &r) {
            return !operator==(r);
        }
    };

    /**
     * @brief An enumeration of all the possible display IDs (https://switchbrew.org/wiki/Display_services#DisplayName)
     */
    enum class DisplayId : u64 {
        Default,
        External,
        Edid,
        Internal,
        Null
    };

    /**
     * @brief A mapping from a display's name to it's displayType entry
     */
    static const std::unordered_map<std::string, DisplayId> displayTypeMap{
        {"Default", DisplayId::Default},
        {"External", DisplayId::External},
        {"Edid", DisplayId::Edid},
        {"Internal", DisplayId::Internal},
        {"Null", DisplayId::Null},
    };

    /**
     * @brief The status of a specific layer
     */
    enum class LayerStatus {
        Uninitialized,
        Initialized
    };

    /**
     * @brief The status of a specific buffer
     */
    enum class BufferStatus {
        Free,
        Dequeued,
        Queued
    };

    /**
     * @brief This struct holds information about the graphics buffer (https://github.com/reswitched/libtransistor/blob/0f0c36227842c344d163922fc98ee76229e9f0ee/lib/display/graphic_buffer_queue.c#L66)
     */
    struct GbpBuffer {
        u32 magic; //!< The magic of the graphics buffer: 0x47424652
        u32 width; //!< The width of the buffer
        u32 height; //!< The height of the buffer
        u32 stride; //!< The stride of the buffer
        u32 format; //!< The format of the buffer, this corresponds to AHardwareBuffer_Format
        u32 usage; //!< The usage flags for the buffer
        u32 _pad0_;
        u32 index; //!< The index of the buffer
        u32 _pad1_[3];
        u32 nvmapId; //!< The ID of the buffer in regards to /dev/nvmap
        u32 _pad2_[8];
        u32 size; //!< The size of the buffer
        u32 _pad3_[8];
        u32 nvmapHandle; //!< The handle of the buffer in regards to /dev/nvmap
        u32 offset; //!< This is the offset of the pixel data in the GPU Buffer
        u32 _pad4_;
        u32 blockHeightLog2; //!< The log2 of the block height
        u32 _pad5_[58];
    };

    /**
     * @brief This represents conditions for the completion of an asynchronous graphics operation
     */
    struct Fence {
        u32 syncptId; //!< The ID of the syncpoint
        u32 syncptValue; //!< The value of the syncpoint
    };

    /**
     * @brief This holds the state and describes a single Buffer
     */
    class Buffer {
      public:
        const DeviceState &state; //!< The state of the device
        u32 slot; //!< The slot the buffer is in
        u32 bpp; //!< The amount of bytes per pixel
        Resolution resolution; //!< The resolution of this buffer
        GbpBuffer gbpBuffer; //!< The information about the underlying buffer
        BufferStatus status{BufferStatus::Free}; //!< The status of this buffer
        std::shared_ptr<device::NvMap::NvMapObject> nvBuffer{}; //!< A shared pointer to the buffer's nvmap object

        /**
         * @param state The state of the device
         * @param slot The slot this buffer is in
         * @param gpBuffer The GbpBuffer object for this buffer
         */
        Buffer(const DeviceState &state, u32 slot, GbpBuffer &gbpBuffer);

        /**
         * @return The address of the buffer on the kernel
         */
        u8* GetAddress();
    };

    /**
     * @brief This is used to manage and queue up all display buffers to be shown
     */
    class BufferQueue {
      private:
        const DeviceState &state; //!< The state of the device

      public:
        std::unordered_map<u32, std::shared_ptr<Buffer>> queue; //!< A vector of shared pointers to all the queued buffers
        std::queue<std::shared_ptr<Buffer>> displayQueue; //!< A queue of all the buffers to be posted to the display

        /**
         * @param state The state of the device
         */
        BufferQueue(const DeviceState &state);

        /**
         * @brief This the GbpBuffer struct of the specified buffer
         */
        void RequestBuffer(Parcel &in, Parcel &out);

        /**
         * @brief This returns the slot of a free buffer
         */
        void DequeueBuffer(Parcel &in, Parcel &out);

        /**
         * @brief This queues a buffer to be displayed
         */
        void QueueBuffer(Parcel &in, Parcel &out);

        /**
         * @brief This removes a previously queued buffer
         */
        void CancelBuffer(Parcel &parcel);

        /**
         * @brief This adds a pre-existing buffer to the queue
         */
        void SetPreallocatedBuffer(Parcel &parcel);

        /**
         * @brief This frees a buffer which is currently queued
         * @param slotNo The slot of the buffer
         */
        inline void FreeBuffer(u32 slotNo) {
            queue.at(slotNo)->status = BufferStatus::Free;
        }
    };
}
