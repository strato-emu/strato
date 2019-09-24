#include <vector>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(std::string filePath, const DeviceState &state) : Loader(filePath) {
        NroHeader header{};
        ReadOffset((u32 *) &header, 0x0, sizeof(NroHeader));
        if (header.magic != constant::NroMagic)
            throw exception(fmt::format("Invalid NRO magic! 0x{0:X}", header.magic));

        state.nce->MapSharedRegion(constant::BaseAddr, header.text.size, {true, true, true}, {true, true, true}, memory::Type::CodeStatic, memory::Region::Text); // R-X
        state.logger->Write(Logger::Debug, "Successfully mapped region .text @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr, header.text.size);

        auto rodata = state.nce->MapSharedRegion(constant::BaseAddr + header.text.size, header.ro.size, {true, true, false}, {true, false, false}, memory::Type::CodeReadOnly, memory::Region::RoData); // R--
        state.logger->Write(Logger::Debug, "Successfully mapped region .ro @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + header.text.size, header.ro.size);

        state.nce->MapSharedRegion(constant::BaseAddr + header.text.size + header.ro.size, header.data.size, {true, true, false}, {true, true, false}, memory::Type::CodeStatic, memory::Region::Data); // RW-
        state.logger->Write(Logger::Debug, "Successfully mapped region .data @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + header.text.size + header.ro.size, header.data.size);

        state.nce->MapSharedRegion(constant::BaseAddr + header.text.size + header.ro.size + header.data.size, header.bssSize, {true, true, true}, {true, true, true}, memory::Type::CodeMutable, memory::Region::Bss); // RWX
        state.logger->Write(Logger::Debug, "Successfully mapped region .bss @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + header.text.size + header.ro.size + header.data.size, header.bssSize);

        ReadOffset(reinterpret_cast<u8 *>(constant::BaseAddr), header.text.offset, header.text.size);
        ReadOffset(reinterpret_cast<u8 *>(constant::BaseAddr + header.text.size), header.ro.offset, header.ro.size);
        ReadOffset(reinterpret_cast<u8 *>(constant::BaseAddr + header.text.size + header.ro.size), header.data.offset, header.data.size);


        // Replace SVC & MRS with BRK
        auto address = (u32 *) constant::BaseAddr + header.text.offset;
        size_t textSize = header.text.size / sizeof(u32);
        for (size_t iter = 0; iter < textSize; iter++) {
            auto instrSvc = reinterpret_cast<instr::Svc *>(address + iter);
            auto instrMrs = reinterpret_cast<instr::Mrs *>(address + iter);

            if (instrSvc->Verify()) {
                instr::Brk brk(static_cast<u16>(instrSvc->value));
                address[iter] = *reinterpret_cast<u32 *>(&brk);
            } else if (instrMrs->Verify() && instrMrs->srcReg == constant::TpidrroEl0) {
                instr::Brk brk(static_cast<u16>(constant::SvcLast + 1 + instrMrs->dstReg));
                address[iter] = *reinterpret_cast<u32 *>(&brk);
            }
        }
    }
}
