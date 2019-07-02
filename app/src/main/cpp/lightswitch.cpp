#include <jni.h>
#include <string>
#include <syslog.h>
#include <core/arm/cpu.h>
#include <core/hos/loaders/nro.h>
#include <core/arm/memory.h>

extern "C"
JNIEXPORT void JNICALL
Java_gq_cyuubi_lightswitch_MainActivity_loadFile(JNIEnv *env, jobject instance, jstring file_) {
    const char *file = env->GetStringUTFChars(file_, 0);
    core::cpu::Initialize();
    core::loader::LoadNro(file);
    core::cpu::Run(BASE_ADDRESS);
    env->ReleaseStringUTFChars(file_, file);
}