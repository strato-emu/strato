# Skyline Proguard Rules
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

-keep class emu.skyline.loader.AppEntry {
    void writeObject(java.io.ObjectOutputStream);
    void readObject(java.io.ObjectInputStream);
}
-keep class emu.skyline.GameActivity { *; }
