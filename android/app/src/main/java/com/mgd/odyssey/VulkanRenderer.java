package com.mgd.odyssey;

import android.view.Surface;

public interface VulkanRenderer {
    void setSurface(Surface surface);
    void onSurfaceCreated(int width, int height);
    void onSurfaceChanged(int width, int height);
    void onSurfaceDestroyed();
    void setQualityPreset(int preset);
    void setResolutionScale(float scale);
    void setSharpness(float sharpness);
    void setFSREnabled(boolean enabled);
    void setTAAEnabled(boolean enabled);
    void setRCASEnabled(boolean enabled);
    void onTouch(float x, float y, boolean pressed);
    void onKey(int keyCode, boolean pressed);
    PerformanceStats getPerformanceStats();
}