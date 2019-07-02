#include <jni.h>
#include <string>
#include <syslog.h>
#include <core/arm/cpu.h>
#include <core/hos/loaders/nro.h>
#include <core/arm/memory.h>

extern "C" JNIEXPORT jstring JNICALL
Java_gq_cyuubi_lightswitch_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    core::cpu::Initialize();
    core::loader::LoadNro("/sdcard/lawsofaviation.nro");
    core::cpu::Run(BASE_ADDRESS);

    std::string finished = "finished!";
    return env->NewStringUTF(finished.c_str());
}
