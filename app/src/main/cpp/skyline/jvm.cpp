// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "jvm.h"

thread_local JNIEnv *env;

namespace skyline {
    JvmManager::JvmManager(JNIEnv *environ, jobject instance) : instance(environ->NewGlobalRef(instance)), instanceClass(reinterpret_cast<jclass>(environ->NewGlobalRef(environ->GetObjectClass(instance)))), initializeControllersId(environ->GetMethodID(instanceClass, "initializeControllers", "()V")), vibrateDeviceId(environ->GetMethodID(instanceClass, "vibrateDevice", "(I[J[I)V")), clearVibrationDeviceId(environ->GetMethodID(instanceClass, "clearVibrationDevice", "(I)V")) {
        env = environ;
        if (env->GetJavaVM(&vm) < 0)
            throw exception("Cannot get JavaVM from environment");
    }

    JvmManager::~JvmManager() {
        env->DeleteGlobalRef(instanceClass);
        env->DeleteGlobalRef(instance);
    }

    void JvmManager::AttachThread() {
        if (!env)
            vm->AttachCurrentThread(&env, nullptr);
    }

    void JvmManager::DetachThread() {
        if (env)
            vm->DetachCurrentThread();
    }

    JNIEnv *JvmManager::GetEnv() {
        return env;
    }

    jobject JvmManager::GetField(const char *key, const char *signature) {
        return env->GetObjectField(instance, env->GetFieldID(instanceClass, key, signature));
    }

    bool JvmManager::CheckNull(const char *key, const char *signature) {
        return env->IsSameObject(env->GetObjectField(instance, env->GetFieldID(instanceClass, key, signature)), nullptr);
    }

    bool JvmManager::CheckNull(jobject &object) {
        return env->IsSameObject(object, nullptr);
    }

    void JvmManager::InitializeControllers() {
        env->CallVoidMethod(instance, initializeControllersId);
    }

    void JvmManager::VibrateDevice(jint index, const span<jlong> &timings, const span<jint> &amplitudes) {
        auto jTimings = env->NewLongArray(timings.size());
        env->SetLongArrayRegion(jTimings, 0, timings.size(), timings.data());
        auto jAmplitudes = env->NewIntArray(amplitudes.size());
        env->SetIntArrayRegion(jAmplitudes, 0, amplitudes.size(), amplitudes.data());

        env->CallVoidMethod(instance, vibrateDeviceId, index, jTimings, jAmplitudes);

        env->DeleteLocalRef(jTimings);
        env->DeleteLocalRef(jAmplitudes);
    }

    void JvmManager::ClearVibrationDevice(jint index) {
        env->CallVoidMethod(instance, clearVibrationDeviceId, index);
    }
}
