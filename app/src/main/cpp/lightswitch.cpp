#include <jni.h>
#include <string>

#include <unicorn/unicorn.h>

extern "C" JNIEXPORT jstring JNICALL
Java_gq_cyuubi_lightswitch_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    uc_engine *uc;
    uc_err err;
    err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc);
    if (err) {
        std::string failed = "loading failed!";
        return env->NewStringUTF(failed.c_str());
    }
    std::string hello = "loaded successfully!";
    return env->NewStringUTF(hello.c_str());
}
