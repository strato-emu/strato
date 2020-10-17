// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include <jni.h>

namespace skyline {
    /**
     * @brief The JvmManager class is used to simplify transactions with the Java component
     */
    class JvmManager {
      public:
        jobject instance; //!< A reference to the activity
        jclass instanceClass; //!< The class of the activity

        /**
         * @param env A pointer to the JNI environment
         * @param instance A reference to the activity
         */
        JvmManager(JNIEnv *env, jobject instance);

        ~JvmManager();

        /**
         * @brief Returns a pointer to the JNI environment for the current thread
         */
        static JNIEnv *GetEnv();

        /**
         * @brief Retrieves a specific field of the given type from the activity
         * @tparam objectType The type of the object in the field
         * @param key The name of the field in the activity class
         * @return The contents of the field as objectType
         */
        template<typename objectType>
        inline objectType GetField(const char *key) {
            JNIEnv *env{GetEnv()};
            if constexpr(std::is_same<objectType, jboolean>())
                return env->GetBooleanField(instance, env->GetFieldID(instanceClass, key, "Z"));
            else if constexpr(std::is_same<objectType, jbyte>())
                return env->GetByteField(instance, env->GetFieldID(instanceClass, key, "B"));
            else if constexpr(std::is_same<objectType, jchar>())
                return env->GetCharField(instance, env->GetFieldID(instanceClass, key, "C"));
            else if constexpr(std::is_same<objectType, jshort>())
                return env->GetShortField(instance, env->GetFieldID(instanceClass, key, "S"));
            else if constexpr(std::is_same<objectType, jint>())
                return env->GetIntField(instance, env->GetFieldID(instanceClass, key, "I"));
            else if constexpr(std::is_same<objectType, jlong>())
                return env->GetLongField(instance, env->GetFieldID(instanceClass, key, "J"));
            else if constexpr(std::is_same<objectType, jfloat>())
                return env->GetFloatField(instance, env->GetFieldID(instanceClass, key, "F"));
            else if constexpr(std::is_same<objectType, jdouble>())
                return env->GetDoubleField(instance, env->GetFieldID(instanceClass, key, "D"));
        }

        /**
         * @brief Retrieves a specific field from the activity as a jobject
         * @param key The name of the field in the activity class
         * @param signature The signature of the field
         * @return A jobject of the contents of the field
         */
        jobject GetField(const char *key, const char *signature);

        /**
         * @brief Checks if a specific field from the activity is null or not
         * @param key The name of the field in the activity class
         * @param signature The signature of the field
         * @return If the field is null or not
         */
        bool CheckNull(const char *key, const char *signature);

        /**
         * @brief Checks if a specific jobject is null or not
         * @param object The jobject to check
         * @return If the object is null or not
         */
        static bool CheckNull(jobject &object);

        /**
         * @brief A call to EmulationActivity.initializeControllers in Kotlin
         */
        void InitializeControllers();

        /**
         * @brief A call to EmulationActivity.vibrateDevice in Kotlin
         */
        void VibrateDevice(jint index, const span<jlong> &timings, const span<jint> &amplitudes);

        /**
         * @brief A call to EmulationActivity.clearVibrationDevice in Kotlin
         */
        void ClearVibrationDevice(jint index);

      private:
        jmethodID initializeControllersId;
        jmethodID vibrateDeviceId;
        jmethodID clearVibrationDeviceId;
    };
}
