# MGD Odyssey ProGuard Rules

# Keep native methods
-keepclasseswithmembers class * {
    native <methods>;
}

# Keep JNI classes
-keep class com.mgd.odyssey.** { *; }

# Keep GameActivity
-keep class com.google.android.games.activity.GameActivity { *; }

# Keep Vulkan
-keep class androidx.vulkan.** { *; }

# Keep native library
-keep class * {
    native <methods>;
}

# Don't obfuscate native method names
-keepclasseswithmembernames class * {
    native <methods>;
}

# Don't warn about Vulkan
-dontwarn org.vulkan.**
-dontwarn androidx.vulkan.**

# Keep enum values
-keepclassmembers enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}