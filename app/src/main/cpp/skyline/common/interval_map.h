// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "utils.h"
#include "span.h"

namespace skyline {
    /**
     * @brief An associative map with groups of overlapping intervals associated with a value with support for range-based lookups
     * @tparam AddressType The type of address used for lookups and insertions
     * @tparam EntryType The type of entry that is stored for a collection of intervals
     */
    template<typename AddressType, typename EntryType>
    class IntervalMap {
      public:
        struct Interval {
            AddressType start;
            AddressType end;

            Interval(AddressType start, AddressType end) : start(start), end(end) {}

            size_t Size() const {
                return static_cast<size_t>(end - start);
            }

            Interval Align(size_t alignment) const {
                return Interval(util::AlignDown(start, alignment), util::AlignUp(end, alignment));
            }

            bool operator==(const Interval &other) const {
                return start == other.start && end == other.end;
            }

            bool operator<(AddressType address) const {
                return start < address;
            }
        };

      private:
        struct EntryGroup {
            std::vector<Interval> intervals;
            EntryType value;

            EntryGroup(Interval interval, EntryType value) : intervals(1, interval), value(std::move(value)) {}

            EntryGroup(span<Interval> intervals, EntryType value) : intervals(intervals), value(std::move(value)) {}

            template<typename T>
            EntryGroup(span<span<T>> lIntervals, EntryType value) : value(std::move(value)) {
                intervals.reserve(lIntervals.size());
                for (const auto &interval : lIntervals)
                    intervals.emplace_back(interval.data(), interval.data() + interval.size());
            }
        };

        std::list<EntryGroup> groups;

      public:
        using GroupHandle = typename std::list<EntryGroup>::iterator;

      private:
        struct Entry : public Interval {
            GroupHandle group;

            Entry(AddressType start, AddressType end, GroupHandle group) : Interval{start, end}, group{group} {}

            bool operator==(const GroupHandle &pGroup) const {
                return group == pGroup;
            }
        };

        /**
         * @param entries References to entries inside an EntryGroup, it is UB to supply a reference to an entry that is not in an EntryGroup
         * @return If any entry in the supplied entries belongs to the specified group
         */
        static bool IsGroupInEntries(GroupHandle group, const std::vector<std::reference_wrapper<EntryType>> &entries) {
            for (const auto &entry : entries) {
                auto entryPtr{reinterpret_cast<u8 *>(&entry.get())};
                auto entryGroup{reinterpret_cast<EntryGroup *>(entryPtr - offsetof(EntryGroup, value))}; // We exploit the fact that the entry is stored in the EntryGroup structure to get a pointer to it
                if (entryGroup == &*group)
                    return true;
            }
            return false;
        }

        std::vector<Entry> entries;

      public:
        IntervalMap() = default;

        IntervalMap(const IntervalMap &) = delete;

        IntervalMap(IntervalMap &&) = delete;

        IntervalMap &operator=(const IntervalMap &) = delete;

        IntervalMap &operator=(IntervalMap &&) = delete;

        GroupHandle Insert(AddressType start, AddressType end, EntryType value) {
            GroupHandle group{groups.emplace(groups.begin(), Interval{start, end}, value)};
            entries.emplace(std::lower_bound(entries.begin(), entries.end(), end), start, end, group);
            return group;
        }

        GroupHandle Insert(span<Interval> intervals, EntryType value) {
            GroupHandle group{groups.emplace(groups.begin(), intervals, value)};
            for (const auto &interval : intervals)
                entries.emplace(std::lower_bound(entries.begin(), entries.end(), interval.end), interval.start, interval.end, group);
            return group;
        }

        template<typename T>
        GroupHandle Insert(span<span<T>> intervals, EntryType value) requires std::is_pointer_v<AddressType> {
            GroupHandle group{groups.emplace(groups.begin(), intervals, std::move(value))};
            for (const auto &interval : intervals)
                entries.emplace(std::lower_bound(entries.begin(), entries.end(), interval.data()), interval.data(), interval.data() + interval.size(), group);
            return group;
        }

        void Remove(GroupHandle group) {
            for (auto it{entries.begin()}; it != entries.end();) {
                if (it->group == group)
                    it = entries.erase(it);
                else
                    it++;
            }
            groups.erase(group);
        }

        /**
         * @return A nullable pointer to any entry overlapping with the given address
         */
        EntryType *Get(AddressType address) {
            for (auto entryIt{std::lower_bound(entries.begin(), entries.end(), address)}; entryIt != entries.begin() && (--entryIt)->start <= address;)
                if (entryIt->end > address)
                    return &entryIt->group->value;

            return nullptr;
        }

        /**
         * @return A vector of non-nullable pointers to entries overlapping with the given interval
         */
        std::vector<std::reference_wrapper<EntryType>> GetRange(Interval interval) {
            std::vector<std::reference_wrapper<EntryType>> result;
            for (auto entry{std::lower_bound(entries.begin(), entries.end(), interval.end)}; entry != entries.begin() && (--entry)->start < interval.end;)
                if (entry->end > interval.start && !IsGroupInEntries(entry->group, result))
                    result.emplace_back(entry->group->value);

            return result;
        }

        /**
         * @return All entries overlapping with the given interval and a list of intervals they recursively cover with alignment for page-based lookup semantics
         * @note This function is specifically designed for memory faulting lookups and has design-decisions that correspond to that which might not work for other uses
         */
        template<size_t Alignment>
        std::pair<std::vector<std::reference_wrapper<EntryType>>, std::vector<Interval>> GetAlignedRecursiveRange(Interval interval) {
            std::vector<std::reference_wrapper<EntryType>> queryEntries;
            std::vector<Interval> intervals;

            interval = interval.Align(Alignment);

            auto entry{std::lower_bound(entries.begin(), entries.end(), interval.end)};
            bool exclusiveEntry{entry == entries.begin() || std::prev(entry) == entries.begin() || std::prev(entry, 2)->start >= interval.end}; //!< If this entry exclusively occupies an aligned region
            while (entry != entries.begin() && (--entry)->start < interval.end) {
                if (entry->end > interval.start && !IsGroupInEntries(entry->group, queryEntries)) {
                    // We found a unique and overlapping entry in the supplied interval
                    queryEntries.emplace_back(entry->group->value);

                    for (const auto &entryInterval : entry->group->intervals) {
                        /* We need to find intervals that are covered by this entry and adding which will minimize future calls to this function, these are designed with memory faulting in mind. There's a few cases to consider:
                         * 1. The entry exclusively occupies the lookup region - Entries are assumed to be rarely accessed in a partial manner, so we want to get add all intervals covered by the entry which includes all entries on those intervals and all exclusive intervals covered by those entries recursively
                         * 2. The entry doesn't exclusively occupy the lookup region - We want to get all exclusive intervals covered by the entry where the entry is the only entry on those intervals, this is as we don't know what entry will be read in its entirety
                         * 3. The entry doesn't exclusively occupy the lookup region, but the interval matches the entry's interval - This case is implicitly the same as (1) as we want to add all entries overlapping with the current interval
                         */

                        auto alignedEntryInterval{entryInterval.Align(Alignment)};

                        if (exclusiveEntry || entryInterval == *entry) {
                            // Case (1)/(3) - We want to add all entries overlapping with the current interval and their exclusive intervals recursively
                            for (auto recursedEntry{std::lower_bound(entries.begin(), entries.end(), alignedEntryInterval.end)}; recursedEntry != entries.begin() && (--recursedEntry)->start < alignedEntryInterval.end;) {
                                if (recursedEntry->end > alignedEntryInterval.start && recursedEntry->group != entry->group && !IsGroupInEntries(recursedEntry->group, queryEntries)) {
                                    queryEntries.emplace_back(recursedEntry->group->value);

                                    for (const auto &entryInterval2 : recursedEntry->group->intervals) {
                                        // Similar to case (2) below but for the recursed entry
                                        bool exclusiveIntervalEntry{true};
                                        auto alignedEntryInterval2{entryInterval2.Align(Alignment)};

                                        auto recursedEntry2{std::lower_bound(entries.begin(), entries.end(), alignedEntryInterval2.end)};
                                        for (; recursedEntry2 != entries.begin() && (--recursedEntry2)->start < alignedEntryInterval2.end;) {
                                            if (recursedEntry2->end > alignedEntryInterval2.start && recursedEntry2->group != recursedEntry->group && recursedEntry2->group != entry->group) {
                                                exclusiveIntervalEntry = false;
                                                break;
                                            }
                                        }

                                        if (exclusiveIntervalEntry)
                                            intervals.emplace(std::lower_bound(intervals.begin(), intervals.end(), alignedEntryInterval2.end), alignedEntryInterval2);
                                    }
                                }
                            }

                            intervals.emplace(std::lower_bound(intervals.begin(), intervals.end(), alignedEntryInterval.start), alignedEntryInterval);
                        } else {
                            // Case (2) - We only want to add this interval if it only contains the entry
                            bool exclusiveIntervalEntry{true};

                            for (auto recursedEntry{std::lower_bound(entries.begin(), entries.end(), alignedEntryInterval.end)}; recursedEntry != entries.begin() && (--recursedEntry)->start < alignedEntryInterval.end;) {
                                if (recursedEntry->end > alignedEntryInterval.start && recursedEntry->group != entry->group) {
                                    exclusiveIntervalEntry = false;
                                    break;
                                }
                            }

                            if (exclusiveIntervalEntry)
                                intervals.emplace(std::lower_bound(intervals.begin(), intervals.end(), alignedEntryInterval.start), alignedEntryInterval);
                        }
                    }
                }
            }

            // Coalescing pass for combining all intervals that are adjacent to each other
            for (auto it{intervals.begin()}; it != intervals.end();) {
                auto next{std::next(it)};
                if (next != intervals.end() && it->end >= next->start) {
                    if (it->start > next->start)
                        it->start = next->start;
                    if (it->end < next->end)
                        it->end = next->end;
                    it = std::prev(intervals.erase(next));
                } else {
                    it++;
                }
            }

            return std::pair{queryEntries, intervals};
        }

        template<size_t Alignment>
        std::pair<std::vector<std::reference_wrapper<EntryType>>, std::vector<Interval>> GetAlignedRecursiveRange(AddressType address) {
            return GetAlignedRecursiveRange<Alignment>(Interval{address, address + 1});
        }
    };
}
