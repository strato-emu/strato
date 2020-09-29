// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/backing.h>

namespace skyline::crypto {
    /**
     * @brief The KeyStore class looks for title.keys and prod.keys files in rootPath
     * @note Both files are created on kotlin side, prod.keys contains keys that are used to decrypt ROMs and title key, decrypted title keys are used for ctr backing.
     */
    class KeyStore {
      public:
        KeyStore(const std::string &rootPath);

        using Key128 = std::array<u8, 16>;
        using Key256 = std::array<u8, 32>;
        using IndexedKeys128 = std::array<std::optional<Key128>, 20>;

        std::optional<Key256> headerKey;

        IndexedKeys128 titleKek;
        IndexedKeys128 areaKeyApplication;
        IndexedKeys128 areaKeyOcean;
        IndexedKeys128 areaKeySystem;
      private:
        std::map<Key128, Key128> titleKeys;

        std::unordered_map<std::string_view, std::optional<Key256> &> key256Names{
            {"header_key", headerKey},
        };

        std::unordered_map<std::string_view, IndexedKeys128 &> indexedKey128Names{
            {"titlekek_", titleKek},
            {"key_area_key_application_", areaKeyApplication},
            {"key_area_key_ocean_", areaKeyOcean},
            {"key_area_key_system_", areaKeySystem},
        };

        using ReadPairsCallback = void (skyline::crypto::KeyStore::*)(std::string_view, std::string_view);

        void ReadPairs(const std::shared_ptr<vfs::Backing> &backing, ReadPairsCallback callback);

        void PopulateTitleKeys(std::string_view keyName, std::string_view value);

        void PopulateKeys(std::string_view keyName, std::string_view value);

      public:
        inline std::optional<Key128> GetTitleKey(const Key128 &title) {
            auto it{titleKeys.find(title)};
            if (it == titleKeys.end())
                return std::nullopt;
            return it->second;
        }
    };
}
