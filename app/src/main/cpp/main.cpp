#include <jni.h>
#include <pthread.h>
#include <csignal>
#include <string>
#include <thread>
#include "skyline/common.h"
#include "skyline/os.h"

std::thread *EmuThread;
bool Halt = false;

void ThreadMain(const std::string romPath, const std::string prefPath, const std::string logPath) {
    auto logger = std::make_shared<skyline::Logger>(logPath);
    auto settings = std::make_shared<skyline::Settings>(prefPath);
    //settings->List(log); // (Uncomment when you want to print out all settings strings)
    auto start = std::chrono::steady_clock::now();
    try {
        skyline::kernel::OS os(logger, settings);
        logger->Write(skyline::Logger::Info, "Launching ROM {}", romPath);
        os.Execute(romPath);
        logger->Write(skyline::Logger::Info, "Emulation has ended");
    } catch (std::exception &e) {
        logger->Write(skyline::Logger::Error, e.what());
    } catch (...) {
        logger->Write(skyline::Logger::Error, "An unknown exception has occurred");
    }
    auto end = std::chrono::steady_clock::now();
    logger->Write(skyline::Logger::Info, "Done in: {} ms", (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_MainActivity_loadFile(JNIEnv *env, jobject instance, jstring romPathJni, jstring prefPathJni, jstring logPathJni) {
    const char *romPath = env->GetStringUTFChars(romPathJni, nullptr);
    const char *prefPath = env->GetStringUTFChars(prefPathJni, nullptr);
    const char *logPath = env->GetStringUTFChars(logPathJni, nullptr);

    if (EmuThread) {
        Halt = true;  // This'll cause execution to stop after the next breakpoint
        EmuThread->join();
        Halt = false; // Or the current instance will halt immediately
    }

    // Running on UI thread is not a good idea as the UI will remain unresponsive
    EmuThread = new std::thread(ThreadMain, std::string(romPath, strlen(romPath)), std::string(prefPath, strlen(prefPath)), std::string(logPath, strlen(logPath)));

    env->ReleaseStringUTFChars(romPathJni, romPath);
    env->ReleaseStringUTFChars(prefPathJni, prefPath);
    env->ReleaseStringUTFChars(logPathJni, logPath);
}
