#pragma once

#include <os.h>
#include <kernel/types/KProcess.h>

namespace skyline::loader {
    class Loader {
      protected:
        std::string filePath; //!< The path to the ROM file
        std::ifstream file; //!< An input stream from the file

        /**
         * @brief Read the file at a particular offset
         * @tparam T The type of object to write to
         * @param output The object to write to
         * @param offset The offset to read the file at
         * @param size The amount to read in bytes
         */
        template<typename T>
        void ReadOffset(T *output, u32 offset, size_t size) {
            file.seekg(offset, std::ios_base::beg);
            file.read(reinterpret_cast<char *>(output), size);
        }

        /**
         * @brief This patches specific parts of the code
         * @param code A vector with the code to be patched
         */
        inline void PatchCode(std::vector<u8> &code) {
            u32 *address = reinterpret_cast<u32 *>(code.data());
            u32 *end = address + (code.size() / sizeof(u32));
            while (address < end) {
                auto instrSvc = reinterpret_cast<instr::Svc *>(address);
                auto instrMrs = reinterpret_cast<instr::Mrs *>(address);
                if (instrSvc->Verify()) {
                    instr::Brk brk(static_cast<u16>(instrSvc->value));
                    *address = *reinterpret_cast<u32 *>(&brk);
                } else if (instrMrs->Verify()) {
                    if (instrMrs->srcReg == constant::TpidrroEl0) {
                        instr::Brk brk(static_cast<u16>(constant::SvcLast + 1 + instrMrs->dstReg));
                        *address = *reinterpret_cast<u32 *>(&brk);
                    } else if (instrMrs->srcReg == constant::CntpctEl0) {
                        instr::Mrs mrs(constant::CntvctEl0, instrMrs->dstReg);
                        *address = *reinterpret_cast<u32 *>(&mrs);
                    }
                }
                address++;
            }
        }

      public:
        /**
         * @param file_path_ The path to the ROM file
         */
        Loader(std::string &filePath) : filePath(filePath), file(filePath, std::ios::binary | std::ios::beg) {}

        /**
         * This loads in the data of the main process
         * @param process The process to load in the data
         * @param state The state of the device
         */
        virtual void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) = 0;
    };
}
