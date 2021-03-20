// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <csignal>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <sys/system_properties.h>
#include "skyline/common.h"
#include "skyline/common/signal.h"
#include "skyline/common/settings.h"
#include "skyline/common/trace.h"
#include "skyline/loader/loader.h"
#include "skyline/vfs/android_asset_filesystem.h"
#include "skyline/os.h"
#include "skyline/jvm.h"
#include "skyline/gpu.h"
#include "skyline/input.h"
#include "skyline/kernel/types/KProcess.h"

skyline::u16 Fps;
skyline::u32 FrameTime;
std::weak_ptr<skyline::kernel::OS> OsWeak;
std::weak_ptr<skyline::gpu::GPU> GpuWeak;
std::weak_ptr<skyline::input::Input> InputWeak;

// https://cs.android.com/android/platform/superproject/+/master:bionic/libc/tzcode/bionic.cpp;l=43;drc=master;bpv=1;bpt=1
static std::string GetTimeZoneName() {
    const char* nameEnv = getenv("TZ");
    if (nameEnv)
        return std::string(nameEnv);

    char propBuf[PROP_VALUE_MAX];
    if (__system_property_get("persist.sys.timezone", propBuf)) {
        std::string nameProp(propBuf);

        // Flip -/+, see https://cs.android.com/android/platform/superproject/+/master:bionic/libc/tzcode/bionic.cpp;l=53;drc=master;bpv=1;bpt=1
        if (nameProp.size() > 2) {
            if (nameProp[2] == '-')
                nameProp[2] = '+';
            else if (nameProp[2] == '+')
                nameProp[2] = '-';
        }

        return nameProp;
    }

    // Fallback to GMT
    return "GMT";
}

extern "C" JNIEXPORT void Java_emu_skyline_EmulationActivity_executeApplication(JNIEnv *env, jobject instance, jstring romUriJstring, jint romType, jint romFd, jint preferenceFd, jstring appFilesPathJstring, jobject assetManager) {
    skyline::signal::ScopedStackBlocker stackBlocker; // We do not want anything to unwind past JNI code as there are invalid stack frames which can lead to a segmentation fault
    Fps = FrameTime = 0;

    pthread_setname_np(pthread_self(), "EmuMain");

    auto jvmManager{std::make_shared<skyline::JvmManager>(env, instance)};
    auto settings{std::make_shared<skyline::Settings>(preferenceFd)};
    close(preferenceFd);

    auto appFilesPath{env->GetStringUTFChars(appFilesPathJstring, nullptr)};
    auto logger{std::make_shared<skyline::Logger>(std::string(appFilesPath) + "skyline.log", settings->logLevel)};

    auto start{std::chrono::steady_clock::now()};

    // Initialize tracing
    perfetto::TracingInitArgs args;
    args.backends |= perfetto::kSystemBackend;
    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();

    try {
        auto os{std::make_shared<skyline::kernel::OS>(jvmManager, logger, settings, std::string(appFilesPath), GetTimeZoneName(), std::make_shared<skyline::vfs::AndroidAssetFileSystem>(AAssetManager_fromJava(env, assetManager)))};
        OsWeak = os;
        GpuWeak = os->state.gpu;
        InputWeak = os->state.input;
        jvmManager->InitializeControllers();
        env->ReleaseStringUTFChars(appFilesPathJstring, appFilesPath);

        auto romUri{env->GetStringUTFChars(romUriJstring, nullptr)};
        logger->InfoNoPrefix("Launching ROM {}", romUri);
        env->ReleaseStringUTFChars(romUriJstring, romUri);

        os->Execute(romFd, static_cast<skyline::loader::RomFormat>(romType));
    } catch (std::exception &e) {
        logger->Error("An exception has occurred: {}", e.what());
    } catch (const skyline::signal::SignalException &e) {
        logger->Error("An exception has occurred: {}", e.what());
    } catch (...) {
        logger->Error("An unknown exception has occurred");
    }

    perfetto::TrackEvent::Flush();

    InputWeak.reset();

    logger->InfoNoPrefix("Emulation has ended");

    auto end{std::chrono::steady_clock::now()};
    logger->InfoNoPrefix("Done in: {} ms", (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()));

    close(romFd);
}

extern "C" JNIEXPORT jboolean Java_emu_skyline_EmulationActivity_stopEmulation(JNIEnv *, jobject) {
    auto os{OsWeak.lock()};
    if (!os)
        return false;
    auto process{os->state.process};
    if (!process)
        return false;
    process->Kill(true, false, true);
    return true;
}

extern "C" JNIEXPORT jboolean Java_emu_skyline_EmulationActivity_setSurface(JNIEnv *, jobject, jobject surface) {
    auto gpu{GpuWeak.lock()};
    if (!gpu)
        return false;
    gpu->presentation.UpdateSurface(surface);
    return true;
}

extern "C" JNIEXPORT void Java_emu_skyline_EmulationActivity_updatePerformanceStatistics(JNIEnv *env, jobject thiz) {
    static jclass clazz{};
    if (!clazz)
        clazz = env->GetObjectClass(thiz);

    static jfieldID fpsField{};
    if (!fpsField)
        fpsField = env->GetFieldID(clazz, "fps", "I");
    env->SetIntField(thiz, fpsField, Fps);

    static jfieldID frametimeField{};
    if (!frametimeField)
        frametimeField = env->GetFieldID(clazz, "frametime", "F");
    env->SetFloatField(thiz, frametimeField, static_cast<float>(FrameTime) / 100);
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setController(JNIEnv *, jobject, jint index, jint type, jint partnerIndex) {
    auto input{InputWeak.lock()};
    std::lock_guard guard(input->npad.mutex);
    input->npad.controllers[index] = skyline::input::GuestController{static_cast<skyline::input::NpadControllerType>(type), static_cast<skyline::i8>(partnerIndex)};
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_updateControllers(JNIEnv *, jobject) {
    InputWeak.lock()->npad.Update();
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setButtonState(JNIEnv *, jobject, jint index, jlong mask, jboolean pressed) {
    auto input{InputWeak.lock()};
    if (!input)
        return; // We don't mind if we miss button updates while input hasn't been initialized
    auto device{input->npad.controllers[index].device};
    if (device)
        device->SetButtonState(skyline::input::NpadButton{.raw = static_cast<skyline::u64>(mask)}, pressed);
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setAxisValue(JNIEnv *, jobject, jint index, jint axis, jint value) {
    auto input{InputWeak.lock()};
    if (!input)
        return; // We don't mind if we miss axis updates while input hasn't been initialized
    auto device{input->npad.controllers[index].device};
    if (device)
        device->SetAxisValue(static_cast<skyline::input::NpadAxisId>(axis), value);
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_EmulationActivity_setTouchState(JNIEnv *env, jobject, jintArray pointsJni) {
    using Point = skyline::input::TouchScreenPoint;

    auto input{InputWeak.lock()};
    if (!input)
        return; // We don't mind if we miss touch updates while input hasn't been initialized
    jboolean isCopy{false};

    skyline::span<Point> points(reinterpret_cast<Point *>(env->GetIntArrayElements(pointsJni, &isCopy)), env->GetArrayLength(pointsJni) / (sizeof(Point) / sizeof(jint)));
    input->touch.SetState(points);
    env->ReleaseIntArrayElements(pointsJni, reinterpret_cast<jint *>(points.data()), JNI_ABORT);
}
