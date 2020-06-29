// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "skyline/vfs/os_backing.h"
#include "skyline/loader/nro.h"
#include "skyline/loader/nso.h"
#include "skyline/loader/nca.h"
#include "skyline/jvm.h"

extern "C" JNIEXPORT jlong JNICALL Java_emu_skyline_loader_RomFile_initialize(JNIEnv *env, jobject thiz, jint jformat, jint fd) {
    skyline::loader::RomFormat format = static_cast<skyline::loader::RomFormat>(jformat);

    try {
        auto backing = std::make_shared<skyline::vfs::OsBacking>(fd);

        switch (format) {
            case skyline::loader::RomFormat::NRO:
                return reinterpret_cast<jlong>(new skyline::loader::NroLoader(backing));
            case skyline::loader::RomFormat::NSO:
                return reinterpret_cast<jlong>(new skyline::loader::NsoLoader(backing));
            case skyline::loader::RomFormat::NCA:
                return reinterpret_cast<jlong>(new skyline::loader::NcaLoader(backing));
            default:
                return 0;
        }
    } catch (const std::exception &e) {
        return 0;
    }
}

extern "C" JNIEXPORT jboolean JNICALL Java_emu_skyline_loader_RomFile_hasAssets(JNIEnv *env, jobject thiz, jlong instance) {
    return reinterpret_cast<skyline::loader::Loader *>(instance)->nacp != nullptr;
}

extern "C" JNIEXPORT jbyteArray JNICALL Java_emu_skyline_loader_RomFile_getIcon(JNIEnv *env, jobject thiz, jlong instance) {
    std::vector<skyline::u8> buffer = reinterpret_cast<skyline::loader::Loader *>(instance)->GetIcon();

    jbyteArray result = env->NewByteArray(buffer.size());
    env->SetByteArrayRegion(result, 0, buffer.size(), reinterpret_cast<const jbyte *>(buffer.data()));

    return result;
}

extern "C" JNIEXPORT jstring JNICALL Java_emu_skyline_loader_RomFile_getApplicationName(JNIEnv *env, jobject thiz, jlong instance) {
    std::string applicationName = reinterpret_cast<skyline::loader::Loader *>(instance)->nacp->applicationName;

    return env->NewStringUTF(applicationName.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_emu_skyline_loader_RomFile_getApplicationPublisher(JNIEnv *env, jobject thiz, jlong instance) {
    std::string applicationPublisher = reinterpret_cast<skyline::loader::Loader *>(instance)->nacp->applicationPublisher;

    return env->NewStringUTF(applicationPublisher.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_loader_RomFile_destroy(JNIEnv *env, jobject thiz, jlong instance) {
    delete reinterpret_cast<skyline::loader::NroLoader *>(instance);
}
