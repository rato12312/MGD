#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <string>
#include <memory>

#include "../../../../emulador-mgd/runtime/MarioOdysseyRunner.h"
#include "../../../../emulador-mgd/gpu/VulkanBackend.h"

using namespace mgd::emu;

extern "C" {

static MarioOdysseyRunner* g_runner = nullptr;

JNIEXPORT jlong JNICALL
Java_com_mgd_odyssey_MainActivity_nativeCreateRunner(JNIEnv* env, jobject /* this */) {
    return reinterpret_cast<jlong>(new MarioOdysseyRunner());
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeDestroyRunner(JNIEnv* env, jobject /* this */, jlong ptr) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) {
        runner->shutdown();
        delete runner;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_mgd_odyssey_MainActivity_nativeInitialize(JNIEnv* env, jobject /* this */, 
    jlong ptr, jstring nspPath, jstring keysDir, jstring saveDir, jint preset) {
    
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (!runner) return JNI_FALSE;
    
    const char* nsp = env->GetStringUTFChars(nspPath, nullptr);
    const char* keys = env->GetStringUTFChars(keysDir, nullptr);
    const char* save = env->GetStringUTFChars(saveDir, nullptr);
    
    MarioOdysseyRunner::MarioOdysseyConfig config;
    config.nsp_path = nsp;
    config.keys_dir = keys;
    config.save_dir = save;
    config.graphics.preset = static_cast<MarioOdysseyRunner::QualityPreset>(preset);
    config.auto_load = true;
    
    env->ReleaseStringUTFChars(nspPath, nsp);
    env->ReleaseStringUTFChars(keysDir, keys);
    env->ReleaseStringUTFChars(saveDir, save);
    
    return runner->initialize(config) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeShutdown(JNIEnv* env, jobject /* this */, jlong ptr) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->shutdown();
}

JNIEXPORT jboolean JNICALL
Java_com_mgd_odyssey_MainActivity_nativeRunFrame(JNIEnv* env, jobject /* this */, jlong ptr) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (!runner) return JNI_FALSE;
    return runner->runFrame() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetSurface(JNIEnv* env, jobject /* this */, jlong ptr, jobject surface) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (!runner) return;
    
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window) {
        auto& gpu = runner->getGPU();
        if (gpu.getVulkanContext()) {
            gpu.getVulkanContext()->initFromExisting(
                VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                window, VK_NULL_HANDLE
            );
        }
        ANativeWindow_release(window);
    }
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetQualityPreset(JNIEnv* env, jobject /* this */, jlong ptr, jint preset) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) {
        runner->setQualityPreset(static_cast<MarioOdysseyRunner::QualityPreset>(preset));
    }
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetResolutionScale(JNIEnv* env, jobject /* this */, jlong ptr, jfloat scale) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->setResolutionScale(scale);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetSharpness(JNIEnv* env, jobject /* this */, jlong ptr, jfloat sharpness) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->setSharpness(sharpness);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetFSREnabled(JNIEnv* env, jobject /* this */, jlong ptr, jboolean enabled) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->setFSREnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetTAAEnabled(JNIEnv* env, jobject /* this */, jlong ptr, jboolean enabled) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->setTAAEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeSetRCASEnabled(JNIEnv* env, jobject /* this */, jlong ptr, jboolean enabled) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->setRCASEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeOnTouch(JNIEnv* env, jobject /* this */, jlong ptr, jfloat x, jfloat y, jboolean pressed) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->onTouch(x, y, pressed);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeOnKey(JNIEnv* env, jobject /* this */, jlong ptr, jint keyCode, jboolean pressed) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->onKey(keyCode, pressed);
}

JNIEXPORT jobject JNICALL
Java_com_mgd_odyssey_MainActivity_nativeGetPerformanceStats(JNIEnv* env, jobject /* this */, jlong ptr) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (!runner) return nullptr;
    
    // Create PerformanceStats object
    jclass statsClass = env->FindClass("com/mgd/odyssey/PerformanceStats");
    jmethodID constructor = env->GetMethodID(statsClass, "<init>", "()V");
    jobject statsObj = env->NewObject(statsClass, constructor);
    
    const auto& stats = runner->getPerformanceStats();
    
    auto setField = [&](const char* name, const char* sig, auto value) {
        jfieldID fid = env->GetFieldID(statsClass, name, sig);
        if (fid) env->SetField(statsObj, fid, value);
    };
    
    setField("frameCount", "J", static_cast<jlong>(stats.frame_count));
    setField("currentFps", "D", static_cast<jdouble>(stats.current_fps));
    setField("avgFps", "D", static_cast<jdouble>(stats.avg_fps));
    setField("minFps", "D", static_cast<jdouble>(stats.min_fps));
    setField("maxFps", "D", static_cast<jdouble>(stats.max_fps));
    setField("frameTimeMs", "D", static_cast<jdouble>(stats.frame_time_ms));
    setField("cpuTimeMs", "D", static_cast<jdouble>(stats.cpu_time_ms));
    setField("gpuTimeMs", "D", static_cast<jdouble>(stats.gpu_time_ms));
    setField("trianglesRendered", "J", static_cast<jlong>(stats.triangles_rendered));
    setField("drawCalls", "I", static_cast<jint>(stats.draw_calls));
    setField("verticesRendered", "I", static_cast<jint>(stats.vertices_rendered));
    setField("vramUsedMb", "J", static_cast<jlong>(stats.vram_used_mb));
    setField("vramBudgetMb", "J", static_cast<jlong>(stats.vram_budget_mb));
    setField("cpuUsage", "F", static_cast<jfloat>(stats.cpu_usage));
    setField("gpuUsage", "F", static_cast<jfloat>(stats.gpu_usage));
    setField("batteryLevel", "F", static_cast<jfloat>(stats.battery_level));
    setField("temperatureC", "F", static_cast<jfloat>(stats.temperature_c));
    setField("thermalThrottling", "Z", static_cast<jboolean>(stats.thermal_throttling));
    
    return statsObj;
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeOnTouch(JNIEnv* env, jobject /* this */, jlong ptr, jfloat x, jfloat y, jboolean pressed) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->onTouch(x, y, pressed);
}

JNIEXPORT void JNICALL
Java_com_mgd_odyssey_MainActivity_nativeOnKey(JNIEnv* env, jobject /* this */, jlong ptr, jint keyCode, jboolean pressed) {
    auto* runner = reinterpret_cast<MarioOdysseyRunner*>(ptr);
    if (runner) runner->onKey(keyCode, pressed);
}

} // extern "C"