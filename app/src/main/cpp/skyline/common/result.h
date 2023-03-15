// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "base.h"

namespace skyline {
    /**
     * @brief The result of an operation in HOS
     * @url https://switchbrew.org/wiki/Error_codes
     */
    union Result {
        u32 raw{};
        struct __attribute__((packed)) {
            u16 module : 9;
            u16 id : 12;
        };

        /**
         * @note Success is 0, it's the only result that's not specific to a module
         */
        constexpr Result() = default;

        constexpr Result(u16 module, u16 id) : module{module}, id{id} {}

        constexpr explicit Result(u32 raw) : raw{raw} {}

        constexpr operator u32() const {
            return raw;
        }
    };

    /**
     * @brief A wrapper around std::optional that also stores a HOS result code
     */
    template<typename ValueType, typename ResultType = Result>
    class ResultValue {
        static_assert(!std::is_same<ValueType, ResultType>::value);

      private:
        std::optional<ValueType> value;

      public:
        ResultType result{};

        constexpr ResultValue(ValueType value) : value(value) {};

        constexpr ResultValue(ResultType result) : result(result) {};

        template<typename U>
        constexpr ResultValue(ResultValue<U> result) : result(result) {};

        constexpr operator ResultType() const {
            return result;
        }

        explicit constexpr operator bool() const {
            return value.has_value();
        }

        constexpr ValueType &operator*() {
            return *value;
        }

        constexpr ValueType *operator->() {
            return &*value;
        }
    };
}
