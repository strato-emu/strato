#include <vector>
#include "nro.h"

namespace skyline::loader {
    NroLoader::NroLoader(const int romFd) : Loader(romFd) {
        ReadOffset((u32 *) &header, 0x0, sizeof(NroHeader));
        if (header.magic != constant::NroMagic)
            throw exception("Invalid NRO magic! 0x{0:X}", header.magic);
        mainEntry = constant::BaseAddr;
    }

    void NroLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        std::vector<u8> text(header.text.size);
        std::vector<u8> rodata(header.ro.size);
        std::vector<u8> data(header.data.size);

        ReadOffset(text.data(), header.text.offset, header.text.size);
        ReadOffset(rodata.data(), header.ro.offset, header.ro.size);
        ReadOffset(data.data(), header.data.offset, header.data.size);

        std::vector<u32> patch = state.nce->PatchCode(text, constant::BaseAddr, header.text.size + header.ro.size + header.data.size + header.bssSize);

        u64 textSize = text.size();
        u64 rodataSize = rodata.size();
        u64 dataSize = data.size();
        u64 patchSize = patch.size() * sizeof(u32);

        process->MapPrivateRegion(constant::BaseAddr, textSize, {true, true, true}, memory::Type::CodeStatic, memory::Region::Text); // R-X
        state.logger->Debug("Successfully mapped region .text @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr, textSize);

        process->MapPrivateRegion(constant::BaseAddr + textSize, rodataSize, {true, false, false}, memory::Type::CodeReadOnly, memory::Region::RoData); // R--
        state.logger->Debug("Successfully mapped region .ro @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + textSize, rodataSize);

        process->MapPrivateRegion(constant::BaseAddr + textSize + rodataSize, dataSize, {true, true, false}, memory::Type::CodeStatic, memory::Region::Data); // RW-
        state.logger->Debug("Successfully mapped region .data @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + textSize + rodataSize, dataSize);

        process->MapPrivateRegion(constant::BaseAddr + textSize + rodataSize + dataSize, header.bssSize, {true, true, true}, memory::Type::CodeMutable, memory::Region::Bss); // RWX
        state.logger->Debug("Successfully mapped region .bss @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + textSize + rodataSize + dataSize, header.bssSize);

        process->MapPrivateRegion(constant::BaseAddr + textSize + rodataSize + dataSize + header.bssSize, patchSize, {true, true, true}, memory::Type::CodeStatic, memory::Region::Text); // RWX
        state.logger->Debug("Successfully mapped region .patch @ 0x{0:X}, Size = 0x{1:X}", constant::BaseAddr + textSize + rodataSize + dataSize + header.bssSize, patchSize);

        process->WriteMemory(text.data(), constant::BaseAddr, textSize);
        process->WriteMemory(rodata.data(), constant::BaseAddr + textSize, rodataSize);
        process->WriteMemory(data.data(), constant::BaseAddr + textSize + rodataSize, dataSize);
        process->WriteMemory(patch.data(), constant::BaseAddr + textSize + rodataSize + dataSize + header.bssSize, patchSize);
    }
}
