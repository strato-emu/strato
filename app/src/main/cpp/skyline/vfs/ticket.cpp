// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ticket.h"

namespace skyline::vfs {
    /**
     * @url https://switchbrew.org/wiki/Ticket#Signature_type
     */
    enum class SignatureType : u32 {
        Rsa4096Sha1 = 0x010000,
        Rsa2048Sha1 = 0x010001,
        EcdsaSha1 = 0x010002,
        Rsa4096Sha256 = 0x010003,
        Rsa2048Sha256 = 0x010004,
        EcdsaSha256 = 0x010005,
    };

    Ticket::Ticket(const std::shared_ptr<vfs::Backing> &backing) {
        auto type{backing->Read<SignatureType>()};

        size_t offset;
        switch (type) {
            case SignatureType::Rsa4096Sha1:
            case SignatureType::Rsa4096Sha256:
                offset = 0x240;
                break;
            case SignatureType::Rsa2048Sha1:
            case SignatureType::Rsa2048Sha256:
                offset = 0x140;
                break;
            case SignatureType::EcdsaSha1:
            case SignatureType::EcdsaSha256:
                offset = 0x80;
                break;
            default:
                throw exception("Could not find valid signature type 0x{:X}", type);
        }

        *this = backing->Read<Ticket>(offset);
    }
}