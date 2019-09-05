#include <vector>
#include "nro.h"

namespace lightSwitch::loader {
    NroLoader::NroLoader(std::string file_path, const device_state &state) : Loader(file_path) {
        NroHeader header{};
        ReadOffset((uint32_t *) &header, 0x0, sizeof(NroHeader));
        if (header.magic != constant::nro_magic)
            throw exception(fmt::format("Invalid NRO magic! 0x{0:X}", header.magic));

        state.nce->MapShared(constant::base_addr, header.text.size, {true, true, true}, memory::Region::text); // RWX
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .text @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr, header.text.size);

        state.nce->MapShared(constant::base_addr + header.text.size, header.ro.size, {true, true, false}, memory::Region::rodata); // RW- but should be R--
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .ro @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr + header.text.size, header.ro.size);

        state.nce->MapShared(constant::base_addr + header.text.size + header.ro.size, header.data.size, {true, true, false}, memory::Region::data); // RW-
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .data @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr + header.text.size + header.ro.size, header.data.size);

        state.nce->MapShared(constant::base_addr + header.text.size + header.ro.size + header.data.size, header.bssSize, {true, true, false}, memory::Region::bss); // RW-
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .bss @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr + header.text.size + header.ro.size + header.data.size, header.bssSize);

        ReadOffset(reinterpret_cast<uint8_t *>(constant::base_addr), header.text.offset, header.text.size);
        ReadOffset(reinterpret_cast<uint8_t *>(constant::base_addr + header.text.size), header.ro.offset, header.ro.size);
        ReadOffset(reinterpret_cast<uint8_t *>(constant::base_addr + header.text.size + header.ro.size), header.data.offset, header.data.size);

        // Make .ro read-only after writing to it
        state.nce->UpdatePermissionShared(memory::Region::rodata, {true, false, false});

        // Replace SVC & MRS with BRK
        auto address = (uint32_t *) constant::base_addr + header.text.offset;
        size_t text_size = header.text.size / sizeof(uint32_t);
        for (size_t iter = 0; iter < text_size; iter++) {
            auto instr_svc = reinterpret_cast<instr::svc *>(address + iter);
            auto instr_mrs = reinterpret_cast<instr::mrs *>(address + iter);

            if (instr_svc->verify()) {
                instr::brk brk(static_cast<uint16_t>(instr_svc->value));
                address[iter] = *reinterpret_cast<uint32_t *>(&brk);
            } else if (instr_mrs->verify() && instr_mrs->src_reg == constant::tpidrro_el0) {
                instr::brk brk(static_cast<uint16_t>(constant::svc_last + 1 + instr_mrs->dst_reg));
                address[iter] = *reinterpret_cast<uint32_t *>(&brk);
            }
        }
    }
}