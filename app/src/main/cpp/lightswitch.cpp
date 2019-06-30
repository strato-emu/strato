#include <jni.h>
#include <string>
#include <syslog.h>

extern "C" JNIEXPORT jstring JNICALL
Java_gq_cyuubi_lightswitch_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    std::string finished = "finished!";
    return env->NewStringUTF(finished.c_str());
}
