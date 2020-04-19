// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "jvm.h"

thread_local JNIEnv *env;

namespace skyline {
    JvmManager::JvmManager(JNIEnv *environ, jobject instance) : instance(instance), instanceClass(reinterpret_cast<jclass>(environ->NewGlobalRef(environ->GetObjectClass(instance)))) {
        env = environ;
        if (env->GetJavaVM(&vm) < 0)
            throw exception("Cannot get JavaVM from environment");
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
}
