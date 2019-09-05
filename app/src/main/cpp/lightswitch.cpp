#include <jni.h>
#include <pthread.h>
#include <csignal>
#include <string>
#include <thread>
#include "switch/common.h"
#include "switch/os.h"

std::thread *emu_thread;
bool halt = false;

void thread_main(std::string rom_path, std::string pref_path, std::string log_path) {
    auto log = std::make_shared<lightSwitch::Logger>(log_path);
    auto settings = std::make_shared<lightSwitch::Settings>(pref_path);
    try {
        lightSwitch::kernel::OS os(log, settings);
        log->Write(lightSwitch::Logger::INFO, "Launching ROM {}", rom_path);
        os.Execute(rom_path);
        log->Write(lightSwitch::Logger::INFO, "Emulation has ended");
    } catch (std::exception &e) {
        log->Write(lightSwitch::Logger::ERROR, e.what());
    } catch (...) {
        log->Write(lightSwitch::Logger::ERROR, "An unknown exception has occurred");
    }
}

extern "C" JNIEXPORT void JNICALL Java_emu_lightswitch_MainActivity_loadFile(JNIEnv *env, jobject instance, jstring rom_path_, jstring pref_path_, jstring log_path_) {
    const char *rom_path = env->GetStringUTFChars(rom_path_, nullptr);
    const char *pref_path = env->GetStringUTFChars(pref_path_, nullptr);
    const char *log_path = env->GetStringUTFChars(log_path_, nullptr);

    if (emu_thread) {
        halt = true;  // This'll cause execution to stop after the next breakpoint
        emu_thread->join();
    }

    // Running on UI thread is not a good idea as the UI will remain unresponsive
    emu_thread = new std::thread(thread_main, std::string(rom_path, strlen(rom_path)), std::string(pref_path, strlen(pref_path)), std::string(log_path, strlen(log_path)));

    env->ReleaseStringUTFChars(rom_path_, rom_path);
    env->ReleaseStringUTFChars(pref_path_, pref_path);
    env->ReleaseStringUTFChars(log_path_, log_path);
}
