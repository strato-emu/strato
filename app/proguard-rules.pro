# Skyline Proguard Rules
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Retain all classes within Skyline for traces + JNI access + Serializable classes
-keep class emu.skyline.** { *; }
