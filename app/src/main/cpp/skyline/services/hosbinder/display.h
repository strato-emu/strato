// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::hosbinder {
    /**
     * @brief An enumeration of all the possible display IDs (https://switchbrew.org/wiki/Display_services#DisplayName)
     */
    enum class DisplayId : u64 {
        Default, //!< Refers to the default display used by most applications
        External, //!< Refers to an external display
        Edid, //!< Refers to an external display with EDID capabilities
        Internal, //!< Refers to the the internal display
        Null, //!< Refers to the null display which is used for discarding data
    };

    /**
     * @brief A mapping from a display's name to it's displayType entry
     */
    static const std::unordered_map<std::string, DisplayId> DisplayTypeMap{
        {"Default", DisplayId::Default},
        {"External", DisplayId::External},
        {"Edid", DisplayId::Edid},
        {"Internal", DisplayId::Internal},
        {"Null", DisplayId::Null},
    };

    /**
     * @brief The status of a display layer
     */
    enum class LayerStatus {
        Uninitialized, //!< The layer hasn't been initialized
        Stray, //!< The layer has been initialized as a stray layer
        Managed, //!< The layer has been initialized as a managed layer
    };
}
