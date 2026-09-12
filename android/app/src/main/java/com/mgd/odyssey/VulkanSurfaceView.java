package com.mgd.odyssey;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.TextureView;

public class VulkanSurfaceView extends TextureView implements TextureView.SurfaceTextureListener {
    
    private static final String TAG = "VulkanSurfaceView";
    private Surface surface;
    private VulkanRenderer renderer;
    private boolean surfaceCreated = false;

    public VulkanSurfaceView(Context context) {
        super(context);
        init();
    }

    public VulkanSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public VulkanSurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setSurfaceTextureListener(this);
        setOpaque(true);
    }

    public void setRenderer(VulkanRenderer renderer) {
        this.renderer = renderer;
        if (surfaceCreated && surface != null && renderer != null) {
            renderer.setSurface(surface);
        }
    }

    public Surface getSurface() {
        return surface;
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
        surface = new Surface(surfaceTexture);
        surfaceCreated = true;
        if (renderer != null) {
            renderer.setSurface(surface);
            renderer.onSurfaceCreated(width, height);
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
        if (renderer != null) {
            renderer.onSurfaceChanged(width, height);
        }
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
        if (renderer != null) {
            renderer.onSurfaceDestroyed();
        }
        if (surface != null) {
            surface.release();
            surface = null;
        }
        surfaceCreated = false;
        return true;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {
        // Frame rendered
    }
}