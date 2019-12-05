#include "jvm.h"

namespace skyline {
    JvmManager::JvmManager(JNIEnv *env, jobject instance) : env(env), instance(instance), instanceClass(env->GetObjectClass(instance)) {}

    jobject JvmManager::GetField(const char *key, const char *signature) {
        return env->GetObjectField(instance, env->GetFieldID(instanceClass, key, signature));
    }

    bool JvmManager::CheckNull(const char *key, const char *signature) {
        return env->IsSameObject(env->GetObjectField(instance, env->GetFieldID(instanceClass, key, signature)), nullptr);
    }
}
