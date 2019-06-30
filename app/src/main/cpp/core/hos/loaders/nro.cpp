#include <fstream>
#include <syslog.h>
#include <sys/mman.h>
#include <vector>
#include "../../arm/cpu.h"
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

        NroHeader header;
        ReadDataFromFile(file, reinterpret_cast<char *>(&header), 0x0, sizeof(NroHeader));
        if (header.magic != 0x304F524E) {
            syslog(LOG_ERR, "Invalid NRO magic 0x%x\n", header.magic);
            return false;
        }

        std::vector<uint32_t> text, ro, data;
        text.resize(header.segments[0].size);
        ro.resize  (header.segments[1].size);
        data.resize(header.segments[2].size);

        ReadDataFromFile(file, reinterpret_cast<char *>(text.data()), header.segments[0].fileOffset, header.segments[0].size);
        ReadDataFromFile(file, reinterpret_cast<char *>(ro.data()),   header.segments[1].fileOffset, header.segments[1].size);
        ReadDataFromFile(file, reinterpret_cast<char *>(data.data()), header.segments[2].fileOffset, header.segments[2].size);

        uc_engine* uc = core::CpuContext()->uc;
        if( !memory::Map(uc, BASE_ADDRESS,  header.segments[0].size, ".text") ||
            !memory::Map(uc, BASE_ADDRESS + header.segments[0].size,  header.segments[1].size, ".ro") ||
            !memory::Map(uc, BASE_ADDRESS + header.segments[0].size + header.segments[1].size,  header.segments[2].size, ".data") ||
            !memory::Map(uc, BASE_ADDRESS + header.segments[0].size + header.segments[1].size + header.segments[2].size, header.bssSize, ".bss")) {

           syslog(LOG_ERR, "Failed mapping regions for executable");
           return false;
        }

        memory::Write(text.data(), BASE_ADDRESS,  text.size());
        memory::Write(ro.data(),   BASE_ADDRESS + header.segments[0].size,  ro.size());
        memory::Write(data.data(), BASE_ADDRESS + header.segments[0].size + header.segments[1].size, data.size());
        return true;
    }
}