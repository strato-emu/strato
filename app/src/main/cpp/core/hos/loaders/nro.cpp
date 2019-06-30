#include <fstream>
#include <string>
#include <syslog.h>
#include <sys/mman.h>
#include <vector>
#include "nro.h"

// TODO: Move memory to it's own file
#define MEM_BASE 0x80000000

void ReadDataFromFile(std::string file, char* output, uint32_t offset, size_t size)
{
    std::ifstream f(file, std::ios::binary | std::ios::beg);

    f.seekg(offset);
    f.read(output, size);

    f.close();
}

namespace loader {
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

        ReadDataFromFile(file, reinterpret_cast<char *>(text.data()),
                                     h.segments[0].fileOffset, h.segments[0].size);
        ReadDataFromFile(file, reinterpret_cast<char *>(ro.data()),
                                     h.segments[1].fileOffset, h.segments[1].size);
        ReadDataFromFile(file, reinterpret_cast<char *>(data.data()),
                                     h.segments[2].fileOffset, h.segments[2].size);


        if(!mmap((void*)(MEM_BASE),
                 h.segments[0].size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0) ||
           !mmap((void*)(MEM_BASE + h.segments[0].size),
                 h.segments[1].size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0) ||
           !mmap((void*)(MEM_BASE + h.segments[0].size + h.segments[1].size),
                 h.segments[2].size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0) ||
           !mmap((void*)(MEM_BASE + h.segments[0].size + h.segments[1].size + h.segments[2].size),
                 h.bssSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0)) {

           syslog(LOG_ERR, "Failed mapping regions for executable");
           return false;
        }

        std::memcpy((void*)(MEM_BASE), text.data(), text.size());
        std::memcpy((void*)(MEM_BASE + h.segments[0].size), ro.data(), ro.size());
        std::memcpy((void*)(MEM_BASE + h.segments[0].size + h.segments[1].size), data.data(), data.size());

        return true;
    }
}