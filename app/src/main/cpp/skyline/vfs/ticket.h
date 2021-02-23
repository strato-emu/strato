// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/backing.h>
#include <crypto/key_store.h>

namespace skyline::vfs {
    /**
     * @brief The Ticket struct allows easy access to ticket files, a format used to store an encrypted title keys
     * @url https://switchbrew.org/wiki/Ticket
     */
    struct Ticket {
        enum class TitleKeyType : u8 {
            Common = 0x0,   //!< The title key is stored as a 16-byte block
            Personal = 0x1, //!< The title key is stored as a personalized RSA-2048 message
        };

        std::array<u8, 0x40> issuer;
        std::array<u8, 0x100> titleKeyBlock;
        u8 _pad0_[0x1];
        TitleKeyType titleKeyType;
        u8 _pad1_[0x3];
        u8 masterKeyRevision;
        u8 _pad2_[0xA];
        u64 ticketId;
        u64 deviceId;
        crypto::KeyStore::Key128 rightsId;
        u32 accountId;
        u8 _pad3_[0xC];

        Ticket() = default;

        Ticket(const std::shared_ptr<vfs::Backing> &backing);
    };
    static_assert(sizeof(Ticket) == 0x180);
}