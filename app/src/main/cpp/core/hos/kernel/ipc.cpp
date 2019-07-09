#include <syslog.h>
#include <cstdlib>
#include "ipc.h"

namespace core::kernel
{
    IpcRequest::IpcRequest(uint8_t* tlsPtr)
    {
        for(int i = 0; i < 32; i++)
            syslog(LOG_DEBUG, "%02x\t%02x %02x %02x %02x %02x %02x %02x %02x", i*8, tlsPtr[0+(i*8)], tlsPtr[1+(i*8)], tlsPtr[2+(i*8)], tlsPtr[3+(i*8)], tlsPtr[4+(i*8)], tlsPtr[5+(i*8)], tlsPtr[6+(i*8)], tlsPtr[7+(i*8)]);
        syslog(LOG_DEBUG,     "-----------------------");
        uint32_t word1 = ((uint32_t*)tlsPtr)[1];
        type = *(uint16_t*)tlsPtr;
        xCount = tlsPtr[2] & 0xF0 >> 4;
        aCount = tlsPtr[2] & 0x0F;
        bCount = tlsPtr[3] & 0xF0 >> 4;
        wCount = tlsPtr[3] & 0x0F;
        dataSize = word1 & 0x3FF;
        dataPos = 8;

        if(tlsPtr[2] || tlsPtr[3])
        {
            syslog(LOG_ERR, "IPC - X/A/B/W descriptors");
            exit(0);
        }

        syslog(LOG_DEBUG, "Enable handle descriptor: %s", word1 >> 31 ? "yes" : "no");
        if(word1 >> 31)
        {
            syslog(LOG_ERR, "IPC - Handle descriptor");
            exit(0);
        }

        // Align to 16 bytes
        if((dataPos % 16) != 0)
            dataPos += 16 - (dataPos % 16);
        dataPtr = &tlsPtr[dataPos+16];

        syslog(LOG_DEBUG, "Type: %x", type);
        syslog(LOG_DEBUG, "X descriptors: 0x%x", xCount);
        syslog(LOG_DEBUG, "A descriptors: 0x%x", aCount);
        syslog(LOG_DEBUG, "B descriptors: 0x%x", bCount);
        syslog(LOG_DEBUG, "W descriptors: 0x%x", wCount);
        syslog(LOG_DEBUG, "Raw data size: 0x%x", word1 & 0x3FF);
        syslog(LOG_DEBUG, "Data offset=%x, Data size=%x", dataPos, dataSize);
        syslog(LOG_DEBUG, "Payload CmdId=%i", *((uint32_t*)&tlsPtr[dataPos+8]));
        syslog(LOG_DEBUG, "Setting dataPtr to %08x", dataPos+16);
    }
}