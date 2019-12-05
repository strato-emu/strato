#include "skyline/common.h"
#include "skyline/os.h"
#include "skyline/jvm.h"
#include <unistd.h>
#include <csignal>

bool Halt;
uint FaultCount;
std::mutex jniMtx;

void signalHandler(int signal) {
    syslog(LOG_ERR, "Halting program due to signal: %s", strsignal(signal));
    if (FaultCount > 2)
        exit(SIGKILL);
    else
        Halt = true;
    FaultCount++;
}

extern "C" JNIEXPORT void Java_emu_skyline_GameActivity_executeRom(JNIEnv *env, jobject instance, jstring romJstring, jint romType, jint romFd, jint preferenceFd, jint logFd) {
    Halt = false;
    FaultCount = 0;

    std::signal(SIGTERM, signalHandler);
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);

    setpriority(PRIO_PROCESS, static_cast<id_t>(getpid()), skyline::constant::PriorityAn.second);

    auto jvmManager = std::make_shared<skyline::JvmManager>(env, instance);
    auto settings = std::make_shared<skyline::Settings>(preferenceFd);
    auto logger = std::make_shared<skyline::Logger>(logFd, static_cast<skyline::Logger::LogLevel>(std::stoi(settings->GetString("log_level"))));
    //settings->List(logger); // (Uncomment when you want to print out all settings strings)

    auto start = std::chrono::steady_clock::now();

    try {
        skyline::kernel::OS os(jvmManager, logger, settings);
        const char *romString = env->GetStringUTFChars(romJstring, nullptr);
        logger->Info("Launching ROM {}", romString);
        env->ReleaseStringUTFChars(romJstring, romString);
        os.Execute(romFd, static_cast<skyline::TitleFormat>(romType));
        logger->Info("Emulation has ended");
    } catch (std::exception &e) {
        logger->Error(e.what());
    } catch (...) {
        logger->Error("An unknown exception has occurred");
    }

    auto end = std::chrono::steady_clock::now();
    logger->Info("Done in: {} ms", (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));
}

extern "C" JNIEXPORT void Java_emu_skyline_GameActivity_lockMutex(JNIEnv *env, jobject instance) {
    jniMtx.lock();
}

extern "C" JNIEXPORT void Java_emu_skyline_GameActivity_unlockMutex(JNIEnv *env, jobject instance) {
    jniMtx.unlock();
}
