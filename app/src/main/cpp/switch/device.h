#pragma once

#include "os/os.h"
#include "loader/nro.h"

namespace lightSwitch {
    class device {
    private:
        std::shared_ptr<hw::Cpu> cpu;
        std::shared_ptr<hw::Memory> memory;
        os::OS os;
        device_state state;
    public:
        device(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings) : cpu(new hw::Cpu()), memory(new hw::Memory()), state{cpu, memory, settings, logger}, os({cpu, memory, settings, logger}) {};

        void run(std::string rom_file) {
            std::string rom_ext = rom_file.substr(rom_file.find_last_of('.') + 1);
            std::transform(rom_ext.begin(), rom_ext.end(), rom_ext.begin(),
                    [](unsigned char c){ return std::tolower(c); });

            if (rom_ext == "nro") loader::NroLoader loader(rom_file, state);
            else throw exception("Unsupported ROM extension.");

            cpu->Execute(hw::Memory::text, memory, std::bind(&os::OS::SvcHandler, std::ref(os), std::placeholders::_1, std::placeholders::_2), &state);
        }
    };
};