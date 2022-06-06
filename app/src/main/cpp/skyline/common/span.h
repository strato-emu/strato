// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <span>
#include "utils.h"

namespace skyline {
    /**
     * @brief A custom wrapper over span that adds several useful methods to it
     * @note This class is completely transparent, it implicitly converts from and to span
     */
    template<typename T, size_t Extent = std::dynamic_extent>
    class span : public std::span<T, Extent> {
      public:
        using std::span<T, Extent>::span;
        using std::span<T, Extent>::operator=;

        typedef typename std::span<T, Extent>::element_type element_type;
        typedef typename std::span<T, Extent>::size_type size_type;

        constexpr span(const std::span<T, Extent> &spn) : std::span<T, Extent>(spn) {}

        /**
         * @brief A single-element constructor for a span
         */
        constexpr span(T &spn) : std::span<T, Extent>(&spn, 1) {}

        constexpr span(nullptr_t) : std::span<T, Extent>() {}

        /**
         * @brief We want to support implicitly casting from std::string_view -> span as it's just a specialization of a data view which span is a generic form of, the opposite doesn't hold true as not all data held by a span is string data therefore the conversion isn't implicit there
         */
        template<typename Traits>
        constexpr span(const std::basic_string_view<T, Traits> &string) : std::span<T, Extent>(const_cast<T *>(string.data()), string.size()) {}

        template<typename Out, bool SkipSizeCheck = false>
        constexpr Out &as() {
            if constexpr (SkipSizeCheck || (Extent != std::dynamic_extent && sizeof(T) * Extent >= sizeof(Out)))
                return *reinterpret_cast<Out *>(span::data());

            if (span::size_bytes() >= sizeof(Out))
                return *reinterpret_cast<Out *>(span::data());
            throw exception("Span size is less than Out type size (0x{:X}/0x{:X})", span::size_bytes(), sizeof(Out));
        }

        /**
         * @param nullTerminated If true and the string is null-terminated, a view of it will be returned (not including the null terminator itself), otherwise the entire span will be returned as a string view
         */
        constexpr std::string_view as_string(bool nullTerminated = false) {
            return std::string_view(reinterpret_cast<const char *>(span::data()), nullTerminated ? static_cast<size_t>(std::find(span::begin(), span::end(), 0) - span::begin()) : span::size_bytes());
        }

        template<typename Out, size_t OutExtent = std::dynamic_extent, bool SkipAlignmentCheck = false>
        constexpr span<Out> cast() {
            if (SkipAlignmentCheck || util::IsAligned(span::size_bytes(), sizeof(Out)))
                return span<Out, OutExtent>(reinterpret_cast<Out *>(span::data()), span::size_bytes() / sizeof(Out));
            throw exception("Span size not aligned with Out type size (0x{:X}/0x{:X})", span::size_bytes(), sizeof(Out));
        }

        /**
         * @brief Copies data from the supplied span into this one
         * @param amount The amount of elements that need to be copied (in terms of the supplied span), 0 will try to copy the entirety of the other span
         */
        template<typename In, size_t InExtent>
        constexpr void copy_from(const span<In, InExtent> spn, size_type amount = 0) {
            auto size{amount ? amount * sizeof(In) : spn.size_bytes()};
            if (span::size_bytes() < size)
                throw exception("Data being copied is larger than this span");
            std::memmove(span::data(), spn.data(), size);
        }

        /**
         * @brief Implicit type conversion for copy_from, this allows passing in std::vector/std::array in directly is automatically passed by reference which is important for any containers
         */
        template<typename In>
        constexpr void copy_from(const In &in, size_type amount = 0) {
            copy_from(span<typename std::add_const<typename In::value_type>::type>(in), amount);
        }

        /**
         * @return If a supplied span is located entirely inside this span and is effectively a subspan
         */
        constexpr bool contains(const span<T, Extent> &other) const {
            return this->begin() <= other.begin() && this->end() >= other.end();
        }

        /**
         * @return If a supplied address is located inside this span
         */
        constexpr bool contains(const T *address) const {
            return this->data() <= address && this->end().base() > address;
        }

        /**
         * @return If the span is valid by not being null
         */
        constexpr bool valid() const {
            return this->data() != nullptr;
        }

        /** Comparision operators for equality and binary searches **/

        constexpr bool operator==(const span<T, Extent> &other) const {
            return this->data() == other.data() && this->size() == other.size();
        }

        constexpr bool operator<(const span<T, Extent> &other) const {
            return this->data() < other.data();
        }

        constexpr bool operator<(T *pointer) const {
            return this->data() < pointer;
        }

        constexpr bool operator<(typename std::span<T, Extent>::iterator it) const {
            return this->begin() < it;
        }

        /** Base Class Functions that return an instance of it, we upcast them **/
        template<size_t Count>
        constexpr span<T, Count> first() const noexcept {
            return std::span<T, Extent>::template first<Count>();
        }

        template<size_t Count>
        constexpr span<T, Count> last() const noexcept {
            return std::span<T, Extent>::template last<Count>();
        }

        constexpr span<element_type, std::dynamic_extent> first(size_type count) const noexcept {
            return std::span<T, Extent>::first(count);
        }

        constexpr span<element_type, std::dynamic_extent> last(size_type count) const noexcept {
            return std::span<T, Extent>::last(count);
        }

        template<size_t Offset, size_t Count = std::dynamic_extent>
        constexpr auto subspan() const noexcept -> span<T, Count != std::dynamic_extent ? Count : Extent - Offset> {
            return std::span<T, Extent>::template subspan<Offset, Count>();
        }

        constexpr span<T, std::dynamic_extent> subspan(size_type offset, size_type count = std::dynamic_extent) const noexcept {
            return std::span<T, Extent>::subspan(offset, count);
        }
    };

    /**
     * @brief Deduction guides required for arguments to span, CTAD will fail for iterators, arrays and containers without this
     */
    template<typename It, typename End, size_t Extent = std::dynamic_extent>
    span(It, End) -> span<typename std::iterator_traits<It>::value_type, Extent>;
    template<typename T, size_t Size>
    span(T (&)[Size]) -> span<T, Size>;
    template<typename T, size_t Size>
    span(std::array<T, Size> &) -> span<T, Size>;
    template<typename T, size_t Size>
    span(const std::array<T, Size> &) -> span<const T, Size>;
    template<typename Container>
    span(Container &) -> span<typename Container::value_type>;
    template<typename Container>
    span(const Container &) -> span<const typename Container::value_type>;

    /**
     * @return If the contents of the two spans are byte-for-byte equal
     */
    template<typename T, size_t Extent = std::dynamic_extent>
    struct SpanEqual {
        constexpr bool operator()(const span<T, Extent> &lhs, const span<T, Extent> &rhs) const {
            if (lhs.size_bytes() != rhs.size_bytes())
                return false;
            return std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }
    };

    /**
     * @return A hash of the contents of the span
     */
    template<typename T, size_t Extent = std::dynamic_extent>
    struct SpanHash {
        constexpr size_t operator()(const skyline::span<T, Extent> &x) const {
            return XXH64(x.data(), x.size_bytes(), 0);
        }
    };
}
