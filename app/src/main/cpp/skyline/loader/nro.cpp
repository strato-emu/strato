#include <vector>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(std::string filePath) : Loader(filePath) {
        ReadOffset((u32 *) &header, 0x0, sizeof(NroHeader));
        if (header.magic != constant::NroMagic)
            throw exception(fmt::format("Invalid NRO magic! 0x{0:X}", header.magic));
    }

    void NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        process->MapPrivateRegion(constant::BaseAddr, header.text.size, {true, true, true}, memory::Type::CodeStatic, memory::Region::Text); // R-X
        state.logger->Write(Logger::Debug, "Successfully mapped region .text @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr, header.text.size);

        process->MapPrivateRegion(constant::BaseAddr + header.text.size, header.ro.size, {true, false, false}, memory::Type::CodeReadOnly, memory::Region::RoData); // R--
        state.logger->Write(Logger::Debug, "Successfully mapped region .ro @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + header.text.size, header.ro.size);

        process->MapPrivateRegion(constant::BaseAddr + header.text.size + header.ro.size, header.data.size, {true, true, false}, memory::Type::CodeStatic, memory::Region::Data); // RW-
        state.logger->Write(Logger::Debug, "Successfully mapped region .data @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + header.text.size + header.ro.size, header.data.size);

        process->MapPrivateRegion(constant::BaseAddr + header.text.size + header.ro.size + header.data.size, header.bssSize, {true, true, true}, memory::Type::CodeMutable, memory::Region::Bss); // RWX
        state.logger->Write(Logger::Debug, "Successfully mapped region .bss @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + header.text.size + header.ro.size + header.data.size, header.bssSize);

        std::vector<u8> text(header.text.size);
        std::vector<u8> rodata(header.ro.size);
        std::vector<u8> data(header.data.size);

        ReadOffset(text.data(), header.text.offset, header.text.size);
        ReadOffset(rodata.data(), header.ro.offset, header.ro.size);
        ReadOffset(data.data(), header.data.offset, header.data.size);

        // Replace SVC & MRS with BRK
        for (auto address = reinterpret_cast<u32*>(text.data()); address <= reinterpret_cast<u32*>(text.data() + header.text.size); address++) {
            auto instrSvc = reinterpret_cast<instr::Svc *>(address);
            auto instrMrs = reinterpret_cast<instr::Mrs *>(address);

            if (instrSvc->Verify()) {
                instr::Brk brk(static_cast<u16>(instrSvc->value));
                *address = *reinterpret_cast<u32 *>(&brk);
            } else if (instrMrs->Verify() && instrMrs->srcReg == constant::TpidrroEl0) {
                instr::Brk brk(static_cast<u16>(constant::SvcLast + 1 + instrMrs->dstReg));
                *address = *reinterpret_cast<u32 *>(&brk);
            }
        }

        process->WriteMemory(text.data(), constant::BaseAddr, header.text.size);
        process->WriteMemory(rodata.data(), constant::BaseAddr + header.text.size, header.ro.size);
        process->WriteMemory(data.data(), constant::BaseAddr + header.text.size + header.ro.size, header.data.size);
    }
}
