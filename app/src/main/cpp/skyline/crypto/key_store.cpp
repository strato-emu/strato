// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <vfs/os_filesystem.h>
#include "key_store.h"

namespace skyline::crypto {
    KeyStore::KeyStore(const std::string &rootPath) {
        vfs::OsFileSystem root(rootPath);
        if (root.FileExists("title.keys"))
            ReadPairs(root.OpenFile("title.keys"), &KeyStore::PopulateTitleKeys);
        if (root.FileExists("prod.keys"))
            ReadPairs(root.OpenFile("prod.keys"), &KeyStore::PopulateKeys);
    }

    void KeyStore::ReadPairs(const std::shared_ptr<vfs::Backing> &backing, ReadPairsCallback callback) {
        std::vector<char> fileContent(backing->size);
        backing->Read(span(fileContent).cast<u8>());

        auto lineStart{fileContent.begin()};
        std::vector<char>::iterator lineEnd;
        while ((lineEnd = std::find(lineStart, fileContent.end(), '\n')) != fileContent.end()) {
            auto keyEnd{std::find(lineStart, lineEnd, '=')};
            if (keyEnd == lineEnd) {
                throw exception("Invalid key file");
            }

            std::string_view key(&*lineStart, keyEnd - lineStart);
            std::string_view value(&*(keyEnd + 1), lineEnd - keyEnd - 1);
            (this->*callback)(key, value);

            lineStart = lineEnd + 1;
        }
    }

    void KeyStore::PopulateTitleKeys(std::string_view keyName, std::string_view value) {
        Key128 key{util::HexStringToArray<16>(keyName)};
        Key128 valueArray{util::HexStringToArray<16>(value)};
        titleKeys.insert({std::move(key), std::move(valueArray)});
    }

    void KeyStore::PopulateKeys(std::string_view keyName, std::string_view value) {
        {
            auto it{key256Names.find(keyName)};
            if (it != key256Names.end()) {
                it->second = headerKey = util::HexStringToArray<32>(value);
                return;
            }
        }

        if (keyName.size() > 2) {
            auto it{indexedKey128Names.find(keyName.substr(0, keyName.size() - 2))};
            if (it != indexedKey128Names.end()) {
                size_t index{std::stoul(std::string(keyName.substr(it->first.size())), nullptr, 16)};
                it->second[index] = util::HexStringToArray<16>(value);
            }
        }
    }
}
