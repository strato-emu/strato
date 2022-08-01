// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <sys/mman.h>
#include "span.h"

namespace skyline {
    /**
     * @brief A two-level range table implementation that utilizes kernel-backed demand paging, the multi-level aspect is to allow for a RangeType that applies to a large amount of ranges to be mapped at a lower granularity (L2) and go to a higher granuality (L1) as needed
     * @tparam RangeType The type of range to use for the range table entries, this must be a trivial type as it'll be returned filled with 0s when a range is unset
     * @tparam Size The size of the table in terms of units (not ranges unless L1Bits = 1), this represents size in bytes for a range table covering address space
     * @tparam L1Bits The size of an L1 range as a power of 2, this should be lower than L2 and will determine the minimum granularity of the table
     * @tparam L2Bits The size of an L2 range as a power of 2, this should be higher than L2 and will determine the maximum granularity of the table
     * @tparam EnablePointerAccess Whether or not to enable pointer access to the table, this is useful when host addresses are used as the key for the table
     * @note This class is **NOT** thread-safe, any access to the table must be protected by a mutex
     */
    template<typename RangeType, size_t Size, size_t L1Bits, size_t L2Bits, bool EnablePointerAccess = false> requires std::is_trivial_v<RangeType>
    class RangeTable {
      private:
        static constexpr size_t L1Size{1 << L1Bits}, L1Entries{util::DivideCeil(Size, L1Size)};
        span<RangeType, L1Entries> level1Table; //!< The first level of the range table, this is the highest granularity of the table and contains only the range

        /**
         * @brief An entry in a range table level aside from the lowest level which directly holds the type, this has an associated range and a flag if the lookup should move to a higher granularity (and the corresponding lower level)
         */
        struct alignas(8) RangeEntry {
            RangeType range; //!< The range associated with the entry, this is 0'd out if the entry is unset
            bool valid; //!< If the associated range is valid, the entry must not be accessed without checking validity first
            bool level1Set; //!< If to ignore this level and look in the next level table instead
        };

        static constexpr size_t L2Size{1 << L2Bits}, L2Entries{util::DivideCeil(Size, L2Size)}, L1inL2Count{L1Size / L2Size};
        span<RangeEntry, L2Entries> level2Table; //!< The second level of the range table, this is the lowest granularity of the table

        template<typename Type, size_t Amount>
        static span<Type, Amount> AllocateTable() {
            void *ptr{mmap(nullptr, Amount * sizeof(Type), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0)};
            if (ptr == MAP_FAILED)
                throw exception{"Failed to allocate 0x{:X} bytes of memory for range table: {}", Amount * sizeof(Type), strerror(errno)};
            return span<Type, Amount>(static_cast<Type *>(ptr), Amount);
        }

      public:
        RangeTable() : level1Table{AllocateTable<RangeType, L1Entries>()}, level2Table{AllocateTable<RangeEntry, L2Entries>()} {}

        RangeTable(const RangeTable &other) : level1Table{AllocateTable<RangeType, L1Entries>()}, level2Table{AllocateTable<RangeEntry, L2Entries>()} {
            level1Table.copy_from(other.level1Table);
            level2Table.copy_from(other.level2Table);
        }

        RangeTable &operator=(const RangeTable &other) {
            level1Table = AllocateTable<RangeType, L1Entries>();
            level2Table = AllocateTable<RangeEntry, L2Entries>();

            level1Table.copy_from(other.level1Table);
            level2Table.copy_from(other.level2Table);
            return *this;
        }

        RangeTable(RangeTable &&other) : level1Table{std::exchange(other.level1Table, nullptr)}, level2Table{std::exchange(other.level2Table, nullptr)} {}

        RangeTable &operator=(RangeTable &&other) {
            level1Table = std::exchange(other.level1Table, nullptr);
            level2Table = std::exchange(other.level2Table, nullptr);
            return *this;
        }

        ~RangeTable() {
            if (level1Table.valid())
                munmap(level1Table.data(), level1Table.size_bytes());
            if (level2Table.valid())
                munmap(level2Table.data(), level2Table.size_bytes());
        }

        /**
         * @return A read-only reference to the range at the given index, this'll return a 0'd out range if the range is unset
         */
        const RangeType &operator[](size_t index) const {
            auto &l2Entry{level2Table[index >> L2Bits]};
            if (l2Entry.valid)
                return l2Entry.range;
            return level1Table[index >> L1Bits];
        }

        /**
         * @brief Sets a single range at the specified index to the supplied value
         */
        void Set(size_t index, RangeType range) {
            auto &l2Entry{level2Table[index >> L2Bits]};
            if (l2Entry.valid || l2Entry.level1Set) {
                if (l2Entry.range == range)
                    return;

                l2Entry.valid = false;
                l2Entry.level1Set = true;
                size_t l1L2Start{(index >> L2Bits) << (L2Bits - L1Bits)};
                for (size_t i{l1L2Start}; i < l1L2Start + L1inL2Count; i++)
                    level1Table[i] = l2Entry.range;

                level1Table[index >> L1Bits] = range;
            } else if (l2Entry.level1Set) {
                level1Table[index >> L1Bits] = range;
            } else {
                l2Entry.range = range;
                l2Entry.valid = true;
                l2Entry.level1Set = true;
            }
        }

        /**
         * @brief Sets a range of ranges between the start and end to the supplied value
         */
        void Set(size_t start, size_t end, RangeType range) {
            size_t l2AlignedAddress{util::AlignUp(start, L2Size)};

            size_t l1StartPaddingStart{start >> L1Bits};
            size_t l1StartPaddingEnd{l2AlignedAddress >> L1Bits};
            if (l1StartPaddingStart != l1StartPaddingEnd) {
                auto &l2Entry{level2Table[start >> L2Bits]};
                if (l2Entry.valid) {
                    l2Entry.valid = false;
                    l2Entry.level1Set = true;

                    size_t l1L2Start{(start >> L2Bits) << (L2Bits - L1Bits)};
                    for (size_t i{l1L2Start}; i < l1StartPaddingStart; i++)
                        level1Table[i] = l2Entry.range;

                    for (size_t i{l1StartPaddingStart}; i < l1StartPaddingEnd; i++)
                        level1Table[i] = range;
                } else if (!l2Entry.level1Set) {
                    l2Entry.range = range;
                    l2Entry.valid = true;
                    l2Entry.level1Set = false;
                } else {
                    for (size_t i{l1StartPaddingStart}; i < l1StartPaddingEnd; i++)
                        level1Table[i] = range;
                }
            }

            size_t l2IndexStart{l2AlignedAddress >> L2Bits};
            size_t l2IndexEnd{end >> L2Bits};
            for (size_t i{l2IndexStart}; i < l2IndexEnd; i++) {
                auto &l2Entry{level2Table[i]};
                l2Entry.range = range;
                l2Entry.valid = true;
                l2Entry.level1Set = false;
            }

            size_t l1EndPaddingStart{l2IndexEnd << (L2Bits - L1Bits)};
            size_t l1EndPaddingEnd{end >> L1Bits};
            if (l1EndPaddingStart != l1EndPaddingEnd) {
                auto &l2Entry{level2Table[l2IndexEnd]};
                if (l2Entry.valid) {
                    l2Entry.valid = false;
                    l2Entry.level1Set = true;

                    for (size_t i{l1EndPaddingStart}; i < l1EndPaddingEnd; i++)
                        level1Table[i] = range;

                    for (size_t i{l1EndPaddingEnd}; i < l1EndPaddingStart + L1inL2Count; i++)
                        level1Table[i] = l2Entry.range;
                } else if (!l2Entry.level1Set) {
                    l2Entry.range = range;
                    l2Entry.valid = true;
                    l2Entry.level1Set = false;
                } else {
                    for (size_t i{l1EndPaddingStart}; i < l1EndPaddingEnd; i++)
                        level1Table[i] = range;
                }
            }
        }

        /* Helpers for pointer-based access */

        template<typename T>
        requires std::is_pointer_v<T>
        const RangeType &operator[](T pointer) const {
            return (*this)[reinterpret_cast<size_t>(pointer)];
        }

        template<typename T>
        requires std::is_pointer_v<T>
        void Set(T pointer, RangeType range) {
            Set(reinterpret_cast<size_t>(pointer), range);
        }

        template<typename T>
        requires std::is_pointer_v<T>
        void Set(T start, T end, RangeType range) {
            Set(reinterpret_cast<size_t>(start), reinterpret_cast<size_t>(end), range);
        }

        void Set(span<u8> span, RangeType range) {
            Set(reinterpret_cast<size_t>(span.begin().base()), reinterpret_cast<size_t>(span.end().base()));
        }
    };
}
