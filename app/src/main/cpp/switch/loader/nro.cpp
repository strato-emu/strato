#include <vector>
#include "nro.h"

namespace lightSwitch::loader {
    void NroLoader::Load(device_state state) {
        NroHeader header{};
        ReadOffset((uint32_t *) &header, 0x0, sizeof(NroHeader));
        if (header.magic != constant::nro_magic)
            throw exception(fmt::format("Invalid NRO magic 0x{0:x}", header.magic));

        auto text = new uint32_t[header.text.size]();
        auto ro = new uint32_t[header.ro.size]();
        auto data = new uint32_t[header.data.size]();

        ReadOffset(text, header.text.offset, header.text.size);
        ReadOffset(ro, header.ro.offset, header.ro.size);
        ReadOffset(data, header.data.offset, header.data.size);

        state.mem->Map(constant::base_addr, header.text.size, hw::Memory::text);
        state.logger->write(Logger::DEBUG, "Successfully mapped region .text to 0x{0:x}, size is 0x{1:x}.", constant::base_addr, header.text.size);
        state.mem->Map(constant::base_addr + header.text.size, header.ro.size, hw::Memory::rodata);
        state.logger->write(Logger::DEBUG, "Successfully mapped region .ro to 0x{0:x}, size is 0x{1:x}.", constant::base_addr + header.text.size, header.ro.size);
        state.mem->Map(constant::base_addr + header.text.size + header.ro.size, header.data.size, hw::Memory::data);
        state.logger->write(Logger::DEBUG, "Successfully mapped region .data to 0x{0:x}, size is 0x{1:x}.", constant::base_addr + header.text.size + header.ro.size, header.data.size);
        state.mem->Map(constant::base_addr + header.text.size + header.ro.size + header.data.size, header.bssSize, hw::Memory::bss);
        state.logger->write(Logger::DEBUG, "Successfully mapped region .bss to 0x{0:x}, size is 0x{1:x}.", constant::base_addr + header.text.size + header.ro.size + header.data.size, header.bssSize);

        state.mem->Write(text, constant::base_addr, header.text.size);
        state.mem->Write(ro, constant::base_addr + header.text.size, header.ro.size);
        state.mem->Write(data, constant::base_addr + header.text.size + header.ro.size, header.data.size);

        delete[] text;
        delete[] ro;
        delete[] data;
    }
}