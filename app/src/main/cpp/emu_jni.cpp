// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <csignal>
#include <unistd.h>
#include <sys/resource.h>
#include <android/log.h>
#include "skyline/loader/loader.h"
#include "skyline/common.h"
#include "skyline/os.h"
#include "skyline/jvm.h"
#include "skyline/gpu.h"
#include "skyline/input.h"

skyline::u16 fps;
skyline::u32 frametime;
std::weak_ptr<skyline::gpu::GPU> gpuWeak;
std::weak_ptr<skyline::input::Input> inputWeak;

extern "C" JNIEXPORT void Java_emu_skyline_EmulationActivity_executeApplication(JNIEnv *env, jobject instance, jstring romUriJstring, jint romType, jint romFd, jint preferenceFd, jstring appFilesPathJstring) {
    fps = 0;
    frametime = 0;

    pthread_setname_np(pthread_self(), "EmuMain");
    setpriority(PRIO_PROCESS, static_cast<id_t>(gettid()), -8); // Set the priority of this process to the highest value

    auto jvmManager{std::make_shared<skyline::JvmManager>(env, instance)};
    auto settings{std::make_shared<skyline::Settings>(preferenceFd)};
    close(preferenceFd);

    auto appFilesPath{env->GetStringUTFChars(appFilesPathJstring, nullptr)};
    auto logger{std::make_shared<skyline::Logger>(std::string(appFilesPath) + "skyline.log", static_cast<skyline::Logger::LogLevel>(std::stoi(settings->GetString("log_level"))))};
    //settings->List(logger); // (Uncomment when you want to print out all settings strings)

    auto start{std::chrono::steady_clock::now()};

    try {
        skyline::kernel::OS os(jvmManager, logger, settings, std::string(appFilesPath));
        gpuWeak = os.state.gpu;
        inputWeak = os.state.input;
        jvmManager->InitializeControllers();
        env->ReleaseStringUTFChars(appFilesPathJstring, appFilesPath);

        auto romUri{env->GetStringUTFChars(romUriJstring, nullptr)};
        logger->Info("Launching ROM {}", romUri);
        env->ReleaseStringUTFChars(romUriJstring, romUri);

        os.Execute(romFd, static_cast<skyline::loader::RomFormat>(romType));
    } catch (std::exception &e) {
        logger->Error(e.what());
    } catch (...) {
        logger->Error("An unknown exception has occurred");
    }

    inputWeak.reset();

    logger->Info("Emulation has ended");

    auto end{std::chrono::steady_clock::now()};
    logger->Info("Done in: {} ms", (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));

    close(romFd);
}

extern "C" JNIEXPORT void Java_emu_skyline_EmulationActivity_exitGuest(JNIEnv *, jobject, jboolean halt) {
    // TODO
    exit(0);
}

extern "C" JNIEXPORT void Java_emu_skyline_EmulationActivity_setSurface(JNIEnv *, jobject, jobject surface) {
    auto gpu{gpuWeak.lock()};
    while (!gpu)
        gpu = gpuWeak.lock();
    gpu->presentation.UpdateSurface(surface);
}

extern "C" JNIEXPORT jint Java_emu_skyline_EmulationActivity_getFps(JNIEnv *, jobject) {
    return fps;
}

extern "C" JNIEXPORT jfloat Java_emu_skyline_EmulationActivity_getFrametime(JNIEnv *, jobject) {
    return static_cast<float>(frametime) / 100;
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setController(JNIEnv *, jobject, jint index, jint type, jint partnerIndex) {
    auto input{inputWeak.lock()};
    std::lock_guard guard(input->npad.mutex);
    input->npad.controllers[index] = skyline::input::GuestController{static_cast<skyline::input::NpadControllerType>(type), static_cast<skyline::i8>(partnerIndex)};
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_updateControllers(JNIEnv *, jobject) {
    inputWeak.lock()->npad.Update();
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setButtonState(JNIEnv *, jobject, jint index, jlong mask, jboolean pressed) {
    auto input{inputWeak.lock()};
    if (!input)
        return; // We don't mind if we miss button updates while input hasn't been initialized
    auto device{input->npad.controllers[index].device};
    if (device)
        device->SetButtonState(skyline::input::NpadButton{.raw = static_cast<skyline::u64>(mask)}, pressed);
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setAxisValue(JNIEnv *, jobject, jint index, jint axis, jint value) {
    auto input{inputWeak.lock()};
    if (!input)
        return; // We don't mind if we miss axis updates while input hasn't been initialized
    auto device{input->npad.controllers[index].device};
    if (device)
        device->SetAxisValue(static_cast<skyline::input::NpadAxisId>(axis), value);
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setTouchState(JNIEnv *env, jobject, jintArray pointsJni) {
    using Point = skyline::input::TouchScreenPoint;

    auto input{inputWeak.lock()};
    if (!input)
        return; // We don't mind if we miss touch updates while input hasn't been initialized
    jboolean isCopy{false};

    skyline::span<Point> points(reinterpret_cast<Point *>(env->GetIntArrayElements(pointsJni, &isCopy)), env->GetArrayLength(pointsJni) / (sizeof(Point) / sizeof(jint)));
    input->touch.SetState(points);
    env->ReleaseIntArrayElements(pointsJni, reinterpret_cast<jint *>(points.data()), JNI_ABORT);
}
