// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u32 SupportedRevision{11}; //!< The audren revision our implementation supports
        constexpr u32 Rev0Magic{util::MakeMagic<u32>("REV0")}; //!< The HOS 1.0 revision magic
        constexpr u32 RevMagic{Rev0Magic + (SupportedRevision << 24)}; //!< The revision magic for our supported revision

        namespace supportTags {
            constexpr u32 Splitter{2}; //!< The revision splitter support was added
            constexpr u32 SplitterBugFix{5}; //!< The revision the splitter buffer was made aligned
            constexpr u32 PerformanceMetricsDataFormatV2{5}; //!< The revision a new performance metrics format is used
            constexpr u32 VaradicCommandBufferSize{5}; //!< The revision support for varying command buffer sizes was added
            constexpr u32 ElapsedFrameCount{5}; //!< The revision support for counting elapsed frames was added
        }
    }

    namespace service::audio::IAudioRenderer {
        /**
        * @brief Extracts the audren version from a revision magic
        * @param revision The revision magic
        * @return The revision number from the magic
        */
        inline u32 ExtractVersionFromRevision(u32 revision) {
            return (revision - constant::Rev0Magic) >> 24;
        }

        /**
         * @brief The RevisionInfo class is used to query the supported features of various audren revisions
         */
        class RevisionInfo {
          private:
            u32 userRevision; //!< The current audren revision of the guest

          public:
            /**
             * @brief Extracts the audren revision from the magic and sets the behaviour revision to it
             * @param revision The revision magic from guest
             */
            void SetUserRevision(u32 revision) {
                userRevision = ExtractVersionFromRevision(revision);

                if (userRevision > constant::SupportedRevision)
                    throw exception("Unsupported audren revision: {}", userRevision);
            }

            /**
             * @return Whether the splitter is supported
             */
            bool SplitterSupported() {
                return userRevision >= constant::supportTags::Splitter;
            }

            /**
             * @return Whether the splitter is fixed
             */
            bool SplitterBugFixed() {
                return userRevision >= constant::supportTags::SplitterBugFix;
            }

            /**
             * @return Whether the new performance metrics format is used
             */
            bool UsesPerformanceMetricDataFormatV2() {
                return userRevision >= constant::supportTags::PerformanceMetricsDataFormatV2;
            }

            /**
             * @return Whether varying command buffer sizes are supported
             */
            bool VaradicCommandBufferSizeSupported() {
                return userRevision >= constant::supportTags::VaradicCommandBufferSize;
            }

            /**
             * @return Whether elapsed frame counting is supported
             */
            bool ElapsedFrameCountSupported() {
                return userRevision >= constant::supportTags::ElapsedFrameCount;
            }
        };
    }
}
