#pragma once

#include <string>
#include "../common.h"

namespace lightSwitch::loader {
    class Loader {
    protected:
        std::string file_path;
        std::ifstream file;
        device_state state;

        template<typename T>
        void ReadOffset(T *output, uint32_t offset, size_t size) {
            file.seekg(offset, std::ios_base::beg);
            file.read(reinterpret_cast<char *>(output), size);
        }

        virtual void Load(device_state state) = 0;

    public:
        Loader(std::string &file_path_, device_state &state_) : file_path(file_path_), state(state_), file(file_path, std::ios::binary | std::ios::beg) {}
    };
}