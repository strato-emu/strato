// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "skyline/crypto/key_store.h"
#include "skyline/vfs/nca.h"
#include "skyline/vfs/os_backing.h"
#include "skyline/loader/nro.h"
#include "skyline/loader/nso.h"
#include "skyline/loader/nca.h"
#include "skyline/loader/nsp.h"
#include "skyline/jvm.h"

extern "C" JNIEXPORT jint JNICALL Java_emu_skyline_loader_RomFile_populate(JNIEnv *env, jobject thiz, jint jformat, jint fd, jstring appFilesPathJstring) {
    skyline::loader::RomFormat format{static_cast<skyline::loader::RomFormat>(jformat)};

    auto appFilesPath{env->GetStringUTFChars(appFilesPathJstring, nullptr)};
    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(appFilesPath)};
    env->ReleaseStringUTFChars(appFilesPathJstring, appFilesPath);

    std::unique_ptr<skyline::loader::Loader> loader;
    try {
        auto backing{std::make_shared<skyline::vfs::OsBacking>(fd)};

        switch (format) {
            case skyline::loader::RomFormat::NRO:
                loader = std::make_unique<skyline::loader::NroLoader>(backing);
                break;
            case skyline::loader::RomFormat::NSO:
                loader = std::make_unique<skyline::loader::NsoLoader>(backing);
                break;
            case skyline::loader::RomFormat::NCA:
                loader = std::make_unique<skyline::loader::NcaLoader>(backing, keyStore);
                break;
            case skyline::loader::RomFormat::NSP:
                loader = std::make_unique<skyline::loader::NspLoader>(backing, keyStore);
                break;
            default:
                return static_cast<jint>(skyline::loader::LoaderResult::ParsingError);
        }
    } catch (const skyline::loader::loader_exception &e) {
        return static_cast<jint>(e.error);
    } catch (const std::exception &e) {
        return static_cast<jint>(skyline::loader::LoaderResult::ParsingError);
    }

    jclass clazz{env->GetObjectClass(thiz)};
    jfieldID applicationNameField{env->GetFieldID(clazz, "applicationName", "Ljava/lang/String;")};
    jfieldID applicationAuthorField{env->GetFieldID(clazz, "applicationAuthor", "Ljava/lang/String;")};
    jfieldID rawIconField{env->GetFieldID(clazz, "rawIcon", "[B")};

    if (loader->nacp) {
        env->SetObjectField(thiz, applicationNameField, env->NewStringUTF(loader->nacp->applicationName.c_str()));
        env->SetObjectField(thiz, applicationAuthorField, env->NewStringUTF(loader->nacp->applicationPublisher.c_str()));

        auto icon{loader->GetIcon()};
        jbyteArray iconByteArray{env->NewByteArray(icon.size())};
        env->SetByteArrayRegion(iconByteArray, 0, icon.size(), reinterpret_cast<const jbyte *>(icon.data()));
        env->SetObjectField(thiz, rawIconField, iconByteArray);
    }

    return static_cast<jint>(skyline::loader::LoaderResult::Success);
}
