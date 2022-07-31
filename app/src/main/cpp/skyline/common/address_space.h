// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/small_vector.hpp>
#include <concepts>
#include <common.h>

namespace skyline {
    template<typename VaType, size_t AddressSpaceBits>
    concept AddressSpaceValid = std::is_unsigned_v<VaType> && sizeof(VaType) * 8 >= AddressSpaceBits;

    using TranslatedAddressRange = boost::container::small_vector<span<u8>, 1>;

    struct EmptyStruct {};

    /**
     * @brief FlatAddressSpaceMap provides a generic VA->PA mapping implementation using a sorted vector
     */
    template<typename VaType, VaType UnmappedVa, typename PaType, PaType UnmappedPa, bool PaContigSplit, size_t AddressSpaceBits, typename ExtraBlockInfo = EmptyStruct> requires AddressSpaceValid<VaType, AddressSpaceBits>
    class FlatAddressSpaceMap {
      private:
        std::function<void(VaType, VaType)> unmapCallback{}; //!< Callback called when the mappings in an region have changed

      protected:
        /**
         * @brief Represents a block of memory in the AS, the physical mapping is contiguous until another block with a different phys address is hit
         */
        struct Block {
            VaType virt{UnmappedVa}; //!< VA of the block
            PaType phys{UnmappedPa}; //!< PA of the block, will increase 1-1 with VA until a new block is encountered
            [[no_unique_address]] ExtraBlockInfo extraInfo;

            Block() = default;

            Block(VaType virt, PaType phys, ExtraBlockInfo extraInfo) : virt(virt), phys(phys), extraInfo(extraInfo) {}

            constexpr bool Valid() {
                return virt != UnmappedVa;
            }

            constexpr bool Mapped() {
                return phys != UnmappedPa;
            }

            constexpr bool Unmapped() {
                return phys == UnmappedPa;
            }

            bool operator<(const VaType &pVirt) const {
                return virt < pVirt;
            }
        };

        std::mutex blockMutex;
        std::vector<Block> blocks{Block{}};

        /**
         * @brief Maps a PA range into the given AS region
         * @note blockMutex MUST be locked when calling this
         */
        void MapLocked(VaType virt, PaType phys, VaType size, ExtraBlockInfo extraInfo);

        /**
         * @brief Unmaps the given range and merges it with other unmapped regions
         * @note blockMutex MUST be locked when calling this
         */
        void UnmapLocked(VaType virt, VaType size);

      public:
        static constexpr VaType VaMaximum{(1ULL << (AddressSpaceBits - 1)) + ((1ULL << (AddressSpaceBits - 1)) - 1)}; //!< The maximum VA that this AS can technically reach

        VaType vaLimit{VaMaximum}; //!< A soft limit on the maximum VA of the AS

        FlatAddressSpaceMap(VaType vaLimit, std::function<void(VaType, VaType)> unmapCallback = {});

        FlatAddressSpaceMap() = default;

        void Map(VaType virt, PaType phys, VaType size, ExtraBlockInfo extraInfo = {}) {
            std::scoped_lock lock(blockMutex);
            MapLocked(virt, phys, size, extraInfo);
        }

        void Unmap(VaType virt, VaType size) {
            std::scoped_lock lock(blockMutex);
            UnmapLocked(virt, size);
        }
    };

    /**
     * @brief Hold memory manager specific block info
     */
    struct MemoryManagerBlockInfo {
        bool sparseMapped;
    };

    /**
     * @brief FlatMemoryManager specialises FlatAddressSpaceMap to focus on pointers as PAs, adding read/write functions and sparse mapping support
     */
    template<typename VaType, VaType UnmappedVa, size_t AddressSpaceBits> requires AddressSpaceValid<VaType, AddressSpaceBits>
    class FlatMemoryManager : public FlatAddressSpaceMap<VaType, UnmappedVa, u8 *, nullptr, true, AddressSpaceBits, MemoryManagerBlockInfo> {
      private:
        static constexpr u64 SparseMapSize{0x400000000}; //!< 16GiB pool size for sparse mappings returned by TranslateRange, this number is arbritary and should be large enough to fit the largest sparse mapping in the AS
        u8 *sparseMap; //!< Pointer to a zero filled memory region that is returned by TranslateRange for sparse mappings

      public:
        FlatMemoryManager();

        ~FlatMemoryManager();

        /**
         * @return A placeholder address for sparse mapped regions, this means nothing
         */
        static u8 *SparsePlaceholderAddress() {
            return reinterpret_cast<u8 *>(0xCAFEBABE);
        }

        /**
         * @return A vector of all physical ranges inside of the given virtual range
         */
        TranslatedAddressRange TranslateRange(VaType virt, VaType size, std::function<void(span<u8>)> cpuAccessCallback = {});

        void Read(u8 *destination, VaType virt, VaType size, std::function<void(span<u8>)> cpuAccessCallback = {});

        template<typename T>
        void Read(span <T> destination, VaType virt, std::function<void(span<u8>)> cpuAccessCallback = {}) {
            Read(reinterpret_cast<u8 *>(destination.data()), virt, destination.size_bytes(), cpuAccessCallback);
        }

        template<typename T>
        T Read(VaType virt, std::function<void(span<u8>)> cpuAccessCallback = {}) {
            T obj;
            Read(reinterpret_cast<u8 *>(&obj), virt, sizeof(T), cpuAccessCallback);
            return obj;
        }

        /**
         * @brief Writes contents starting from the virtual address till the end of the span or an unmapped block has been hit or when `function` returns a non-nullopt value
         * @param function A function that is called on every block where it should return an end offset into the block when it wants to end reading or std::nullopt when it wants to continue reading
         * @return A span into the supplied container with the contents of the memory region
         * @note The function will **NOT** be run on any sparse block
         * @note The function will provide no feedback on if the end has been reached or if there was an early exit
         */
        template<typename Function, typename Container>
        span<u8> ReadTill(Container& destination, VaType virt, Function function, std::function<void(span<u8>)> cpuAccessCallback = {}) {
            //TRACE_EVENT("containers", "FlatMemoryManager::ReadTill");

            std::scoped_lock lock(this->blockMutex);

            auto successor{std::upper_bound(this->blocks.begin(), this->blocks.end(), virt, [](auto virt, const auto &block) {
                return virt < block.virt;
            })};

            auto predecessor{std::prev(successor)};

            auto pointer{destination.data()};
            auto remainingSize{destination.size()};

            u8 *blockPhys{predecessor->phys + (virt - predecessor->virt)};
            VaType blockReadSize{std::min(successor->virt - virt, remainingSize)};

            while (remainingSize) {
                if (predecessor->phys == nullptr) {
                    return {destination.data(), destination.size() - remainingSize};
                } else {
                    if (predecessor->extraInfo.sparseMapped) {
                        std::memset(pointer, 0, blockReadSize);
                    } else {
                        span<u8> cpuBlock{blockPhys, blockReadSize};
                        if (cpuAccessCallback)
                            cpuAccessCallback(cpuBlock);

                        auto end{function(cpuBlock)};
                        std::memcpy(pointer, blockPhys, end ? *end : blockReadSize);
                        if (end)
                            return {destination.data(), (destination.size() - remainingSize) + *end};
                    }
                }

                pointer += blockReadSize;
                remainingSize -= blockReadSize;

                if (remainingSize) {
                    predecessor = successor++;
                    blockPhys = predecessor->phys;
                    blockReadSize = std::min(successor->virt - predecessor->virt, remainingSize);
                }
            }

            return {destination.data(), destination.size()};
        }

        void Write(VaType virt, u8 *source, VaType size, std::function<void(span<u8>)> cpuAccessCallback = {});

        template<typename T>
        void Write(VaType virt, span<T> source, std::function<void(span<u8>)> cpuAccessCallback = {}) {
            Write(virt, reinterpret_cast<u8 *>(source.data()), source.size_bytes());
        }

        void Write(VaType virt, util::TrivialObject auto source, std::function<void(span<u8>)> cpuAccessCallback = {}) {
            Write(virt, reinterpret_cast<u8 *>(&source), sizeof(source), cpuAccessCallback);
        }

        void Copy(VaType dst, VaType src, VaType size, std::function<void(span<u8>)> cpuAccessCallback = {});
    };

    /**
     * @brief FlatMemoryManager specialises FlatAddressSpaceMap to work as an allocator, with an initial, fast linear pass and a subsequent slower pass that iterates until it finds a free block
     */
    template<typename VaType, VaType UnmappedVa, size_t AddressSpaceBits> requires AddressSpaceValid<VaType, AddressSpaceBits>
    class FlatAllocator : public FlatAddressSpaceMap<VaType, UnmappedVa, bool, false, false, AddressSpaceBits> {
      private:
        using Base = FlatAddressSpaceMap<VaType, UnmappedVa, bool, false, false, AddressSpaceBits>;

        VaType currentLinearAllocEnd; //!< The end address for the initial linear allocation pass, once this reaches the AS limit the slower allocation path will be used

      public:
        VaType vaStart; //!< The base VA of the allocator, no allocations will be below this

        FlatAllocator(VaType vaStart, VaType vaLimit = Base::VaMaximum);

        /**
         * @brief Allocates a region in the AS of the given size and returns its address
         */
        VaType Allocate(VaType size);

        /**
         * @brief Marks the given region in the AS as allocated
         */
        void AllocateFixed(VaType virt, VaType size);

        /**
         * @brief Frees an AS region so it can be used again
         */
        void Free(VaType virt, VaType size);
    };
}
