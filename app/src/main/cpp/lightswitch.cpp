#include <jni.h>
#include <string>

#include <unicorn/unicorn.h>
#include <syslog.h>

// code to be emulated
#define ARM_CODE "\xab\x05\x00\xb8\xaf\x05\x40\x38" // str w11, [x13]; ldrb w15, [x13]

// memory address where emulation starts
#define ADDRESS 0x10000

static void hook_block(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    syslog(LOG_DEBUG, ">>> Tracing basic block at 0x%" PRIx64 ", block size = 0x%x\n", size);
}

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    syslog(LOG_DEBUG, ">>> Tracing instruction at 0x%" PRIx64 ", instruction size = 0x%x\n", size);
}

extern "C" JNIEXPORT jstring JNICALL
Java_gq_cyuubi_lightswitch_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    uc_engine *uc;
    uc_err err;
    uc_hook trace1, trace2;

    int64_t x11 = 0x12345678;        // X11 register
    int64_t x13 = 0x10000 + 0x8;     // X13 register
    int64_t x15 = 0x33;              // X15 register

    syslog(LOG_DEBUG, "Emulate ARM64 code\n");

    // Initialize emulator in ARM mode
    err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc);
    if (err) {
        std::string finished = "failed!";
        return env->NewStringUTF(finished.c_str());
    }

    // map 2MB memory for this emulation
    uc_mem_map(uc, ADDRESS, 2 * 1024 * 1024, UC_PROT_ALL);

    // write machine code to be emulated to memory
    uc_mem_write(uc, ADDRESS, ARM_CODE, sizeof(ARM_CODE) - 1);

    // initialize machine registers
    uc_reg_write(uc, UC_ARM64_REG_X11, &x11);
    uc_reg_write(uc, UC_ARM64_REG_X13, &x13);
    uc_reg_write(uc, UC_ARM64_REG_X15, &x15);

    // tracing all basic blocks with customized callback
    uc_hook_add(uc, &trace1, UC_HOOK_BLOCK, (void*)hook_block, NULL, 1, 0);

    // tracing one instruction at ADDRESS with customized callback
    uc_hook_add(uc, &trace2, UC_HOOK_CODE, (void*)hook_code, NULL, ADDRESS, ADDRESS);

    // emulate machine code in infinite time (last param = 0), or when
    // finishing all the code.
    err = uc_emu_start(uc, ADDRESS, ADDRESS + sizeof(ARM_CODE) -1, 0, 0);
    if (err) {
        syslog(LOG_DEBUG, "Failed on uc_emu_start() with error returned: %u\n", err);
    }

    // now print out some registers
    syslog(LOG_DEBUG, ">>> Emulation done. Below is the CPU context\n");
    syslog(LOG_DEBUG, ">>> As little endian, X15 should be 0x78:\n");

    uc_reg_read(uc, UC_ARM64_REG_X15, &x15);
    syslog(LOG_DEBUG, ">>> X15 = 0x%" PRIx64 "\n", x15);

    uc_close(uc);

    std::string finished = "finished!";
    return env->NewStringUTF(finished.c_str());
}
