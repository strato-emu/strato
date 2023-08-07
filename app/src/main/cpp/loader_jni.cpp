// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "skyline/crypto/key_store.h"
#include "skyline/vfs/nca.h"
#include "skyline/vfs/os_backing.h"
#include "skyline/vfs/os_filesystem.h"
#include "skyline/loader/nro.h"
#include "skyline/loader/nso.h"
#include "skyline/loader/nca.h"
#include "skyline/loader/xci.h"
#include "skyline/loader/nsp.h"
#include "skyline/jvm.h"

extern "C" JNIEXPORT jint JNICALL Java_emu_skyline_loader_RomFile_populate(JNIEnv *env, jobject thiz, jint jformat, jint fd, jstring appFilesPathJstring, jint systemLanguage) {
    skyline::signal::ScopedStackBlocker stackBlocker;

    skyline::loader::RomFormat format{static_cast<skyline::loader::RomFormat>(jformat)};

    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, appFilesPathJstring))};
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
            case skyline::loader::RomFormat::XCI:
                loader = std::make_unique<skyline::loader::XciLoader>(backing, keyStore);
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
    jfieldID applicationTitleIdField{env->GetFieldID(clazz, "applicationTitleId", "Ljava/lang/String;")};
    jfieldID applicationAuthorField{env->GetFieldID(clazz, "applicationAuthor", "Ljava/lang/String;")};
    jfieldID rawIconField{env->GetFieldID(clazz, "rawIcon", "[B")};
    jfieldID applicationVersionField{env->GetFieldID(clazz, "applicationVersion", "Ljava/lang/String;")};

    if (loader->nacp) {
        auto language{skyline::language::GetApplicationLanguage(static_cast<skyline::language::SystemLanguage>(systemLanguage))};
        if (((1 << static_cast<skyline::u32>(language)) & loader->nacp->supportedTitleLanguages) == 0)
            language = loader->nacp->GetFirstSupportedTitleLanguage();

        env->SetObjectField(thiz, applicationNameField, env->NewStringUTF(loader->nacp->GetApplicationName(language).c_str()));
        env->SetObjectField(thiz, applicationVersionField, env->NewStringUTF(loader->nacp->GetApplicationVersion().c_str()));
        env->SetObjectField(thiz, applicationTitleIdField, env->NewStringUTF(loader->nacp->GetSaveDataOwnerId().c_str()));
        env->SetObjectField(thiz, applicationAuthorField, env->NewStringUTF(loader->nacp->GetApplicationPublisher(language).c_str()));

        auto icon{loader->GetIcon(language)};
        jbyteArray iconByteArray{env->NewByteArray(static_cast<jsize>(icon.size()))};
        env->SetByteArrayRegion(iconByteArray, 0, static_cast<jsize>(icon.size()), reinterpret_cast<const jbyte *>(icon.data()));
        env->SetObjectField(thiz, rawIconField, iconByteArray);
    }

    return static_cast<jint>(skyline::loader::LoaderResult::Success);
}

extern "C" JNIEXPORT jstring Java_emu_skyline_preference_FirmwareImportPreference_fetchFirmwareVersion(JNIEnv *env, jobject thiz, jstring systemArchivesPathJstring, jstring keysPathJstring) {
    struct SystemVersion {
        unsigned char major;
        unsigned char minor;
        unsigned char micro;
        unsigned char _pad0_;
        unsigned char revisionMajor;
        unsigned char revisionMinor;
        unsigned char _pad1_[2];
        unsigned char platformString[0x20];
        unsigned char versionHash[0x40];
        unsigned char displayVersion[0x18];
        unsigned char displayTitle[0x80];
    };

    unsigned long systemVersionProgramId{0x0100000000000809};

    auto systemArchivesFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, systemArchivesPathJstring))};
    auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};
    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, keysPathJstring))};
    for (const auto &entry : systemArchives->Read()) {
        std::shared_ptr<skyline::vfs::Backing> backing{systemArchivesFileSystem->OpenFile(entry.name)};
        auto nca{skyline::vfs::NCA(backing, keyStore)};
        if (nca.header.programId == systemVersionProgramId && nca.romFs != nullptr) {
            auto controlRomFs = std::make_shared<skyline::vfs::RomFileSystem>(nca.romFs);
            auto file = controlRomFs->OpenFile("file");
            SystemVersion systemVersion{};
            file->Read<SystemVersion>(systemVersion);
            return env->NewStringUTF(reinterpret_cast<char *>(systemVersion.displayVersion));
        }
    }

    return env->NewStringUTF("");
}
