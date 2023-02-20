// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "settings.h"
#include "jvm.h"

namespace skyline {
    /**
     * @brief Handles settings on the android platform
     * @note Lifetime of this class must not exceed the one of the JNIEnv contained inside ktSettings
     */
    class AndroidSettings final : public Settings {
      private:
        KtSettings ktSettings;

      public:
        /**
         * @note Will construct the underlying KtSettings object in-place
         */
        AndroidSettings(JNIEnv *env, jobject settingsInstance) : ktSettings(env, settingsInstance) {
            Update();
        }

        /**
         * @note Will take ownership of the passed KtSettings object
         */
        AndroidSettings(KtSettings &&ktSettings) : ktSettings(std::move(ktSettings)) {
            Update();
        }

        void Update() override {
            isDocked = ktSettings.GetBool("isDocked");
            usernameValue = std::move(ktSettings.GetString("usernameValue"));
            profilePictureValue = ktSettings.GetString("profilePictureValue");
            systemLanguage = ktSettings.GetInt<skyline::language::SystemLanguage>("systemLanguage");
            systemRegion = ktSettings.GetInt<skyline::region::RegionCode>("systemRegion");
            forceTripleBuffering = ktSettings.GetBool("forceTripleBuffering");
            disableFrameThrottling = ktSettings.GetBool("disableFrameThrottling");
            gpuDriver = ktSettings.GetString("gpuDriver");
            gpuDriverLibraryName = ktSettings.GetString("gpuDriverLibraryName");
            executorSlotCountScale = ktSettings.GetInt<u32>("executorSlotCountScale");
            executorFlushThreshold = ktSettings.GetInt<u32>("executorFlushThreshold");
            useDirectMemoryImport = ktSettings.GetBool("useDirectMemoryImport");
            forceMaxGpuClocks = ktSettings.GetBool("forceMaxGpuClocks");
            disableShaderCache = ktSettings.GetBool("disableShaderCache");
            freeGuestTextureMemory = ktSettings.GetBool("freeGuestTextureMemory");
            enableFastGpuReadbackHack = ktSettings.GetBool("enableFastGpuReadbackHack");
            enableFastReadbackWrites = ktSettings.GetBool("enableFastReadbackWrites");
            disableSubgroupShuffle = ktSettings.GetBool("disableSubgroupShuffle");
            isAudioOutputDisabled = ktSettings.GetBool("isAudioOutputDisabled");
            validationLayer = ktSettings.GetBool("validationLayer");
        };
    };
}
