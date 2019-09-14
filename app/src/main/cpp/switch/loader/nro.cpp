#include <vector>
#include "nro.h"

namespace lightSwitch::loader {
    NroLoader::NroLoader(std::string file_path, const device_state &state) : Loader(file_path) {
        NroHeader header{};
        ReadOffset((u32 *) &header, 0x0, sizeof(NroHeader));
        if (header.magic != constant::nro_magic)
            throw exception(fmt::format("Invalid NRO magic! 0x{0:X}", header.magic));

        state.nce->MapSharedRegion(constant::base_addr, header.text.size, {true, true, true}, {true, true, true}, Memory::Type::CodeStatic, Memory::Region::text); // R-X
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .text @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr, header.text.size);

        auto rodata = state.nce->MapSharedRegion(constant::base_addr + header.text.size, header.ro.size, {true, true, false}, {true, false, false}, Memory::Type::CodeReadOnly, Memory::Region::rodata); // R--
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .ro @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr + header.text.size, header.ro.size);

        state.nce->MapSharedRegion(constant::base_addr + header.text.size + header.ro.size, header.data.size, {true, true, false}, {true, true, false}, Memory::Type::CodeStatic, Memory::Region::data); // RW-
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .data @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr + header.text.size + header.ro.size, header.data.size);

        state.nce->MapSharedRegion(constant::base_addr + header.text.size + header.ro.size + header.data.size, header.bssSize, {true, true, true}, {true, true, true}, Memory::Type::CodeMutable, Memory::Region::bss); // RWX
        state.logger->Write(Logger::DEBUG, "Successfully mapped region .bss @ 0x{0:X}, Size = 0x{1:X}", constant::base_addr + header.text.size + header.ro.size + header.data.size, header.bssSize);

        ReadOffset(reinterpret_cast<u8 *>(constant::base_addr), header.text.offset, header.text.size);
        ReadOffset(reinterpret_cast<u8 *>(constant::base_addr + header.text.size), header.ro.offset, header.ro.size);
        ReadOffset(reinterpret_cast<u8 *>(constant::base_addr + header.text.size + header.ro.size), header.data.offset, header.data.size);


        // Replace SVC & MRS with BRK
        auto address = (u32 *) constant::base_addr + header.text.offset;
        size_t text_size = header.text.size / sizeof(u32);
        for (size_t iter = 0; iter < text_size; iter++) {
            auto instr_svc = reinterpret_cast<instr::svc *>(address + iter);
            auto instr_mrs = reinterpret_cast<instr::mrs *>(address + iter);

            if (instr_svc->verify()) {
                instr::brk brk(static_cast<u16>(instr_svc->value));
                address[iter] = *reinterpret_cast<u32 *>(&brk);
            } else if (instr_mrs->verify() && instr_mrs->src_reg == constant::tpidrro_el0) {
                instr::brk brk(static_cast<u16>(constant::svc_last + 1 + instr_mrs->dst_reg));
                address[iter] = *reinterpret_cast<u32 *>(&brk);
            }
        }
    }
}
