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
        const std::map<std::string, int> ext_case = {
                {"nro", 1},
                {"NRO", 1}
        };
    public:
        device(std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings) : cpu(new hw::Cpu()), memory(new hw::Memory(cpu->GetEngine())), state{cpu, memory, settings, logger}, os({cpu, memory, settings, logger}) {};

        void run(std::string rom_file) {
            try {
                switch (ext_case.at(rom_file.substr(rom_file.find_last_of('.') + 1))) {
                    case 1: {
                        loader::NroLoader loader(rom_file, state);
                        break;
                    }
                    default:
                        break;
                }
                cpu->Execute(constant::base_addr);
            } catch (std::out_of_range &e) {
                throw exception("The ROM extension wasn't recognized.");
            }
        }
    };
};