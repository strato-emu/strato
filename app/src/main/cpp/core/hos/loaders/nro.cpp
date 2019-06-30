#include <fstream>
#include <syslog.h>
#include <sys/mman.h>
#include <vector>
#include "../../arm/memory.h"
#include "nro.h"

void ReadDataFromFile(std::string file, char* output, uint32_t offset, size_t size)
{
    std::ifstream f(file, std::ios::binary | std::ios::beg);

    f.seekg(offset);
    f.read(output, size);

    f.close();
}

namespace core::loader {
    bool LoadNro(std::string file) {
        syslog(LOG_INFO, "Loading NRO file %s\n", file.c_str());

        NroHeader h;
        ReadDataFromFile(file, reinterpret_cast<char *>(&h), 0x0, sizeof(NroHeader));
        if (h.magic != 0x304F524E) {
            syslog(LOG_ERR, "Invalid NRO magic 0x%x\n", h.magic);
            return false;
        }

        std::vector<uint32_t> text, ro, data;
        text.resize(h.segments[0].size);
        ro.resize(h.segments[1].size);
        data.resize(h.segments[2].size);

        ReadDataFromFile(file, reinterpret_cast<char *>(text.data()), h.segments[0].fileOffset, h.segments[0].size);
        ReadDataFromFile(file, reinterpret_cast<char *>(ro.data()), h.segments[1].fileOffset, h.segments[1].size);
        ReadDataFromFile(file, reinterpret_cast<char *>(data.data()), h.segments[2].fileOffset, h.segments[2].size);

        if( !mem::Map(nullptr, MEM_BASE, h.segments[0].size, ".text") ||
            !mem::Map(nullptr, MEM_BASE + h.segments[0].size, h.segments[1].size, ".ro") ||
            !mem::Map(nullptr, MEM_BASE + h.segments[0].size + h.segments[1].size, h.segments[2].size, ".data") ||
            !mem::Map(nullptr, MEM_BASE + h.segments[0].size + h.segments[1].size + h.segments[2].size, h.bssSize, ".bss")) {

           syslog(LOG_ERR, "Failed mapping regions for executable");
           return false;
        }

        mem::Write(text.data(), MEM_BASE, text.size());
        mem::Write(ro.data(), MEM_BASE + h.segments[0].size, ro.size());
        mem::Write(data.data(), MEM_BASE + h.segments[0].size + h.segments[1].size, data.size());

        return true;
    }
}