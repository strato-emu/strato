// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "jvm.h"

namespace skyline {
    std::string JniString::GetJString(JNIEnv *env, jstring jString) {
        auto utf{env->GetStringUTFChars(jString, nullptr)};
        std::string string{utf};
        env->ReleaseStringUTFChars(jString, utf);
        return string;
    }

    /*
     * @brief A thread-local wrapper over JNIEnv and JavaVM which automatically handles attaching and detaching threads
     */
    struct JniEnvironment {
        JNIEnv *env{};
        static inline JavaVM *vm{};
        bool attached{};

        void Initialize(JNIEnv *environment) {
            env = environment;
            if (env->GetJavaVM(&vm) < 0)
                throw exception("Cannot get JavaVM from environment");
            attached = true;
        }

        JniEnvironment() {
            if (vm && !attached) {
                vm->AttachCurrentThread(&env, nullptr);
                attached = true;
            }
        }

        ~JniEnvironment() {
            if (vm && attached)
                vm->DetachCurrentThread();
        }

        operator JNIEnv *() {
            if (!attached)
                throw exception("Not attached");
            return env;
        }

        JNIEnv *operator->() {
            if (!attached)
                throw exception("Not attached");
            return env;
        }
    };

    thread_local inline JniEnvironment env;

    JvmManager::JvmManager(JNIEnv *environ, jobject instance)
        : instance{environ->NewGlobalRef(instance)},
          instanceClass{reinterpret_cast<jclass>(environ->NewGlobalRef(environ->GetObjectClass(instance)))},
          initializeControllersId{environ->GetMethodID(instanceClass, "initializeControllers", "()V")},
          vibrateDeviceId{environ->GetMethodID(instanceClass, "vibrateDevice", "(I[J[I)V")},
          clearVibrationDeviceId{environ->GetMethodID(instanceClass, "clearVibrationDevice", "(I)V")},
          showKeyboardId{environ->GetMethodID(instanceClass, "showKeyboard", "(Ljava/nio/ByteBuffer;Ljava/lang/String;)Lemu/skyline/applet/swkbd/SoftwareKeyboardDialog;")},
          waitForSubmitOrCancelId{environ->GetMethodID(instanceClass, "waitForSubmitOrCancel", "(Lemu/skyline/applet/swkbd/SoftwareKeyboardDialog;)[Ljava/lang/Object;")},
          closeKeyboardId{environ->GetMethodID(instanceClass, "closeKeyboard", "(Lemu/skyline/applet/swkbd/SoftwareKeyboardDialog;)V")},
          showValidationResultId{environ->GetMethodID(instanceClass, "showValidationResult", "(Lemu/skyline/applet/swkbd/SoftwareKeyboardDialog;ILjava/lang/String;)I")},
          getVersionCodeId{environ->GetMethodID(instanceClass, "getVersionCode", "()I")},
          getIntegerValueId{environ->GetMethodID(environ->FindClass("java/lang/Integer"), "intValue", "()I")} {
        env.Initialize(environ);
    }

    JvmManager::~JvmManager() {
        env->DeleteGlobalRef(instanceClass);
        env->DeleteGlobalRef(instance);
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
        auto jTimings{env->NewLongArray(static_cast<jsize>(timings.size()))};
        env->SetLongArrayRegion(jTimings, 0, static_cast<jsize>(timings.size()), timings.data());
        auto jAmplitudes{env->NewIntArray(static_cast<jsize>(amplitudes.size()))};
        env->SetIntArrayRegion(jAmplitudes, 0, static_cast<jsize>(amplitudes.size()), amplitudes.data());

        env->CallVoidMethod(instance, vibrateDeviceId, index, jTimings, jAmplitudes);

        env->DeleteLocalRef(jTimings);
        env->DeleteLocalRef(jAmplitudes);
    }

    void JvmManager::ClearVibrationDevice(jint index) {
        env->CallVoidMethod(instance, clearVibrationDeviceId, index);
    }

    jobject JvmManager::ShowKeyboard(KeyboardConfig &config, std::u16string initialText) {
        auto buffer{env->NewDirectByteBuffer(&config, sizeof(KeyboardConfig))};
        auto str{env->NewString(reinterpret_cast<const jchar *>(initialText.data()), static_cast<int>(initialText.length()))};
        jobject localKeyboardDialog{env->CallObjectMethod(instance, showKeyboardId, buffer, str)};
        env->DeleteLocalRef(buffer);
        env->DeleteLocalRef(str);
        auto keyboardDialog{env->NewGlobalRef(localKeyboardDialog)};

        env->DeleteLocalRef(localKeyboardDialog);
        return keyboardDialog;
    }

    std::pair<JvmManager::KeyboardCloseResult, std::u16string> JvmManager::WaitForSubmitOrCancel(jobject keyboardDialog) {
        auto returnArray{reinterpret_cast<jobjectArray>(env->CallObjectMethod(instance, waitForSubmitOrCancelId, keyboardDialog))};
        auto buttonInteger{env->GetObjectArrayElement(returnArray, 0)};
        auto inputJString{reinterpret_cast<jstring>(env->GetObjectArrayElement(returnArray, 1))};
        auto stringChars{env->GetStringChars(inputJString, nullptr)};
        std::u16string input{stringChars, stringChars + env->GetStringLength(inputJString)};
        env->ReleaseStringChars(inputJString, stringChars);

        return {static_cast<KeyboardCloseResult>(env->CallIntMethod(buttonInteger, getIntegerValueId)), input};
    }

    void JvmManager::CloseKeyboard(jobject dialog) {
        env->CallVoidMethod(instance, closeKeyboardId, dialog);
        env->DeleteGlobalRef(dialog);
    }

    i32 JvmManager::GetVersionCode() {
        return env->CallIntMethod(instance, getVersionCodeId);
    }

    JvmManager::KeyboardCloseResult JvmManager::ShowValidationResult(jobject dialog, KeyboardTextCheckResult checkResult, std::u16string message) {
        auto str{env->NewString(reinterpret_cast<const jchar *>(message.data()), static_cast<int>(message.length()))};
        auto result{static_cast<KeyboardCloseResult>(env->CallIntMethod(instance, showValidationResultId, dialog, checkResult, str))};
        env->DeleteLocalRef(str);
        return result;
    }
}
