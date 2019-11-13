#include "skyline/common.h"
#include "skyline/os.h"
#include <unistd.h>
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/native_activity.h>
#include <csignal>

bool Halt{};
uint faultCount{};
ANativeActivity *Activity{};
ANativeWindow *Window{};
AInputQueue *Queue{};
std::thread *uiThread{};

void GameThread(const std::string &prefPath, const std::string &logPath, const std::string &romPath) {
    while (!Window)
        sched_yield();
    setpriority(PRIO_PROCESS, static_cast<id_t>(getpid()), skyline::constant::PriorityAn.second);
    auto settings = std::make_shared<skyline::Settings>(prefPath);
    auto logger = std::make_shared<skyline::Logger>(logPath, static_cast<skyline::Logger::LogLevel>(std::stoi(settings->GetString("log_level"))));
    //settings->List(logger); // (Uncomment when you want to print out all settings strings)
    auto start = std::chrono::steady_clock::now();
    try {
        skyline::kernel::OS os(logger, settings, Window);
        logger->Info("Launching ROM {}", romPath);
        os.Execute(romPath);
        logger->Info("Emulation has ended");
    } catch (std::exception &e) {
        logger->Error(e.what());
    } catch (...) {
        logger->Error("An unknown exception has occurred");
    }
    auto end = std::chrono::steady_clock::now();
    logger->Info("Done in: {} ms", (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));
    Window = nullptr;
    Halt = true;
}

void UIThread(const std::string &prefPath, const std::string &logPath, const std::string &romPath) {
    while (!Queue)
        sched_yield();
    std::thread gameThread(GameThread, std::string(prefPath), std::string(logPath), std::string(romPath));
    AInputEvent *event{};
    while (!Halt) {
        if (AInputQueue_getEvent(Queue, &event) >= -1) {
            if (AKeyEvent_getKeyCode(event) == AKEYCODE_BACK)
                Halt = true;
            AInputQueue_finishEvent(Queue, event, true);
        }
    }
    Queue = nullptr;
    gameThread.join();
    Halt = false;
    ANativeActivity_finish(Activity);
}

void onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window) {
    Window = window;
}

void onInputQueueCreated(ANativeActivity *activity, AInputQueue *queue) {
    Queue = queue;
}

void onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window) {
    Halt = true;
    while (Window)
        sched_yield();
}

void onInputQueueDestroyed(ANativeActivity *activity, AInputQueue *queue) {
    Halt = true;
    while (Queue)
        sched_yield();
}

void signalHandler(int signal) {
    syslog(LOG_ERR, "Halting program due to signal: %s", strsignal(signal));
    if (faultCount > 2)
        pthread_kill(uiThread->native_handle(), SIGKILL);
    else
        ANativeActivity_finish(Activity);
    faultCount++;
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize) {
    Activity = activity;
    Halt = false;
    faultCount = 0;
    JNIEnv *env = activity->env;
    jobject intent = env->CallObjectMethod(activity->clazz, env->GetMethodID(env->GetObjectClass(activity->clazz), "getIntent", "()Landroid/content/Intent;"));
    jclass icl = env->GetObjectClass(intent);
    jmethodID gse = env->GetMethodID(icl, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    auto jsRom = reinterpret_cast<jstring>(env->CallObjectMethod(intent, gse, env->NewStringUTF("rom")));
    auto jsPrefs = reinterpret_cast<jstring>(env->CallObjectMethod(intent, gse, env->NewStringUTF("prefs")));
    auto jsLog = reinterpret_cast<jstring>(env->CallObjectMethod(intent, gse, env->NewStringUTF("log")));
    const char *romPath = env->GetStringUTFChars(jsRom, nullptr);
    const char *prefPath = env->GetStringUTFChars(jsPrefs, nullptr);
    const char *logPath = env->GetStringUTFChars(jsLog, nullptr);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    uiThread = new std::thread(UIThread, std::string(prefPath), std::string(logPath), std::string(romPath));
    env->ReleaseStringUTFChars(jsRom, romPath);
    env->ReleaseStringUTFChars(jsPrefs, prefPath);
    env->ReleaseStringUTFChars(jsLog, logPath);
}
