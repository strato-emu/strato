// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "skyline/crypto/key_store.h"
#include "skyline/vfs/nca.h"
#include "skyline/vfs/os_backing.h"
#include "skyline/vfs/os_filesystem.h"
#include "skyline/vfs/cnmt.h"
#include "skyline/loader/nro.h"
#include "skyline/loader/nso.h"
#include "skyline/loader/nca.h"
#include "skyline/loader/xci.h"
#include "skyline/loader/nsp.h"
#include "skyline/jvm.h"

extern "C" JNIEXPORT jint JNICALL Java_org_stratoemu_strato_loader_RomFile_populate(JNIEnv *env, jobject thiz, jint jformat, jint fd, jstring appFilesPathJstring, jint systemLanguage) {
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
    jfieldID addOnContentBaseIdField{env->GetFieldID(clazz, "addOnContentBaseId", "Ljava/lang/String;")};
    jfieldID applicationAuthorField{env->GetFieldID(clazz, "applicationAuthor", "Ljava/lang/String;")};
    jfieldID rawIconField{env->GetFieldID(clazz, "rawIcon", "[B")};
    jfieldID applicationVersionField{env->GetFieldID(clazz, "applicationVersion", "Ljava/lang/String;")};
    jfieldID romType{env->GetFieldID(clazz, "romTypeInt", "I")};
    jfieldID parentTitleId{env->GetFieldID(clazz, "parentTitleId", "Ljava/lang/String;")};

    if (loader->nacp) {
        auto language{skyline::language::GetApplicationLanguage(static_cast<skyline::language::SystemLanguage>(systemLanguage))};
        if (((1 << static_cast<skyline::u32>(language)) & loader->nacp->supportedTitleLanguages) == 0)
            language = loader->nacp->GetFirstSupportedTitleLanguage();

        env->SetObjectField(thiz, applicationNameField, env->NewStringUTF(loader->nacp->GetApplicationName(language).c_str()));
        env->SetObjectField(thiz, applicationVersionField, env->NewStringUTF(loader->nacp->GetApplicationVersion().c_str()));
        env->SetObjectField(thiz, applicationTitleIdField, env->NewStringUTF(loader->nacp->GetSaveDataOwnerId().c_str()));
        env->SetObjectField(thiz, addOnContentBaseIdField, env->NewStringUTF(loader->nacp->GetAddOnContentBaseId().c_str()));
        env->SetObjectField(thiz, applicationAuthorField, env->NewStringUTF(loader->nacp->GetApplicationPublisher(language).c_str()));

        auto icon{loader->GetIcon(language)};
        jbyteArray iconByteArray{env->NewByteArray(static_cast<jsize>(icon.size()))};
        env->SetByteArrayRegion(iconByteArray, 0, static_cast<jsize>(icon.size()), reinterpret_cast<const jbyte *>(icon.data()));
        env->SetObjectField(thiz, rawIconField, iconByteArray);
    }

    if (loader->cnmt) {
        auto contentMetaType{loader->cnmt->GetContentMetaType()};
        env->SetIntField(thiz, romType, static_cast<skyline::u8>(contentMetaType));

        if (contentMetaType != skyline::vfs::ContentMetaType::Application)
            env->SetObjectField(thiz, parentTitleId, env->NewStringUTF(loader->cnmt->GetParentTitleId().c_str()));
    }

    return static_cast<jint>(skyline::loader::LoaderResult::Success);
}

extern "C" JNIEXPORT jstring Java_org_stratoemu_strato_preference_FirmwareImportPreference_fetchFirmwareVersion(JNIEnv *env, jobject thiz, jstring systemArchivesPathJstring, jstring keysPathJstring) {
    struct SystemVersion {
        skyline::u8 major;
        skyline::u8 minor;
        skyline::u8 micro;
        skyline::u8 _pad0_;
        skyline::u8 revisionMajor;
        skyline::u8 revisionMinor;
        skyline::u8 _pad1_[2];
        skyline::u8 platformString[0x20];
        skyline::u8 versionHash[0x40];
        skyline::u8 displayVersion[0x18];
        skyline::u8 displayTitle[0x80];
    };

    constexpr skyline::u64 systemVersionProgramId{0x0100000000000809};

    auto systemArchivesFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, systemArchivesPathJstring))};
    auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};
    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, keysPathJstring))};

    for (const auto &entry : systemArchives->Read()) {
        std::shared_ptr<skyline::vfs::Backing> backing{systemArchivesFileSystem->OpenFile(entry.name)};
        auto nca{skyline::vfs::NCA(backing, keyStore)};

        if (nca.header.titleId == systemVersionProgramId && nca.romFs != nullptr) {
            auto controlRomFs{std::make_shared<skyline::vfs::RomFileSystem>(nca.romFs)};
            auto file{controlRomFs->OpenFile("file")};
            SystemVersion systemVersion;
            file->Read<SystemVersion>(systemVersion);
            return env->NewStringUTF(reinterpret_cast<char *>(systemVersion.displayVersion));
        }
    }

    return env->NewStringUTF("");
}

std::vector<skyline::u8> decodeBfttfFont(const std::shared_ptr<skyline::vfs::Backing> bfttfFile){
    constexpr skyline::u32 fontKey{0x06186249};
    constexpr skyline::u32 BFTTFMagic{0x18029a7f};

    auto firstBytes{bfttfFile->Read<skyline::u32>()};
    auto firstBytesXor{firstBytes ^ fontKey};

    if (firstBytesXor == BFTTFMagic) {
        constexpr size_t initialOffset{8};
        std::vector<skyline::u8> font(bfttfFile->size - initialOffset);

        for (size_t offset = initialOffset; offset < bfttfFile->size; offset += 4) {
            skyline::u32 decodedData{bfttfFile->Read<skyline::u32>(offset) ^ fontKey};

            font[offset - 8] = static_cast<skyline::u8>(decodedData >> 0);
            font[offset - 7] = static_cast<skyline::u8>(decodedData >> 8);
            font[offset - 6] = static_cast<skyline::u8>(decodedData >> 16);
            font[offset - 5] = static_cast<skyline::u8>(decodedData >> 24);
        }

        return font;
    }
    return {};
}

extern "C" JNIEXPORT void Java_org_stratoemu_strato_preference_FirmwareImportPreference_extractFonts(JNIEnv *env, jobject thiz, jstring systemArchivesPathJstring, jstring keysPathJstring, jstring fontsPath) {
    // Fonts are stored in the following NCAs
    // 0x0100000000000810 -> "FontNintendoExtended"
    // 0x0100000000000811 -> "FontStandard"
    // 0x0100000000000812 -> "FontKorean"
    // 0x0100000000000813 -> "FontChineseTraditional"
    // 0x0100000000000814 -> "FontChineseSimplified"

    constexpr skyline::u64 firstFontProgramId{0x0100000000000810};
    constexpr skyline::u64 lastFontProgramId{0x0100000000000814};

    const std::map<std::string, std::string> sharedFontFilenameDictionary = {
        {"nintendo_ext_003.bfttf", "FontNintendoExtended"},
        {"nintendo_ext2_003.bfttf", "FontNintendoExtended2"},
        {"nintendo_udsg-r_std_003.bfttf", "FontStandard"},
        {"nintendo_udsg-r_ko_003.bfttf", "FontKorean"},
        {"nintendo_udjxh-db_zh-tw_003.bfttf", "FontChineseTraditional"},
        {"nintendo_udsg-r_org_zh-cn_003.bfttf", "FontChineseSimplified"},
        {"nintendo_udsg-r_ext_zh-cn_003.bfttf", "FontExtendedChineseSimplified"}
    };

    auto fontsFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, fontsPath))};
    auto systemArchivesFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, systemArchivesPathJstring))};
    auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};
    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, keysPathJstring))};

    for (const auto &entry : systemArchives->Read()) {
        std::shared_ptr<skyline::vfs::Backing> backing{systemArchivesFileSystem->OpenFile(entry.name)};
        auto nca{skyline::vfs::NCA(backing, keyStore)};

        if (nca.header.titleId >= firstFontProgramId && nca.header.titleId <= lastFontProgramId && nca.romFs != nullptr) {
            auto controlRomFs{std::make_shared<skyline::vfs::RomFileSystem>(nca.romFs)};

            for (auto fileEntry = controlRomFs->fileMap.begin(); fileEntry != controlRomFs->fileMap.end(); fileEntry++) {
                auto fileName{fileEntry->first};
                auto bfttfFile{controlRomFs->OpenFile(fileName)};

                auto decodedFont{decodeBfttfFont(bfttfFile)};
                if (decodedFont.empty())
                    continue;

                auto ttfFileName{sharedFontFilenameDictionary.at(fileName) + ".ttf"};
                if (fontsFileSystem->FileExists(ttfFileName))
                    fontsFileSystem->DeleteFile(ttfFileName);

                fontsFileSystem->CreateFile(ttfFileName, decodedFont.size());
                std::shared_ptr<skyline::vfs::Backing> ttfFile{fontsFileSystem->OpenFile(ttfFileName, {true, true, false})};

                ttfFile->Write(decodedFont);
            }
        }
    }
}
