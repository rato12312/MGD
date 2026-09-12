package com.mgd.odyssey;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    
    private VulkanSurfaceView vulkanSurface;
    private VulkanRenderer renderer;
    private TextView fpsText, frameTimeText, drawCallsText, vramText;
    private Button btnLoadGame, btnPauseResume, btnSettings;
    private boolean isPaused = false;
    private Handler uiHandler = new Handler(Looper.getMainLooper());
    private Runnable statsUpdater;

    static {
        System.loadLibrary("mgd_odyssey");
    }

    // Native methods
    private native long nativeCreateRunner();
    private native void nativeDestroyRunner(long runnerPtr);
    private native boolean nativeInitialize(long runnerPtr, String nspPath, String keysDir, String saveDir, int preset);
    private native void nativeShutdown(long runnerPtr);
    private native boolean nativeRunFrame(long runnerPtr);
    private native void nativeSetSurface(long runnerPtr, Surface surface);
    private native void nativeSetQualityPreset(long runnerPtr, int preset);
    private native void nativeSetResolutionScale(long runnerPtr, float scale);
    private native void nativeSetSharpness(long runnerPtr, float sharpness);
    private native void nativeSetFSREnabled(long runnerPtr, boolean enabled);
    private native void nativeSetTAAEnabled(long runnerPtr, boolean enabled);
    private native void nativeSetRCASEnabled(long runnerPtr, boolean enabled);
    private native void nativeOnTouch(long runnerPtr, float x, float y, boolean pressed);
    private native void nativeOnKey(long runnerPtr, int keyCode, boolean pressed);
    private native PerformanceStats nativeGetPerformanceStats(long runnerPtr);

    private long runnerPtr = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        vulkanSurface = findViewById(R.id.vulkan_surface);
        fpsText = findViewById(R.id.fps_text);
        frameTimeText = findViewById(R.id.frame_time_text);
        drawCallsText = findViewById(R.id.draw_calls_text);
        vramText = findViewById(R.id.vram_text);
        btnLoadGame = findViewById(R.id.btn_load_game);
        btnPauseResume = findViewById(R.id.btn_pause_resume);
        btnSettings = findViewById(R.id.btn_settings);

        btnLoadGame.setOnClickListener(v -> loadGame());
        btnPauseResume.setOnClickListener(v -> togglePause());
        btnSettings.setOnClickListener(v -> openSettings());

        // Create native runner
        runnerPtr = nativeCreateRunner();
        
        // Setup renderer bridge
        renderer = new VulkanRenderer() {
            @Override
            public void setSurface(Surface surface) {
                nativeSetSurface(runnerPtr, surface);
            }

            @Override
            public void onSurfaceCreated(int width, int height) {
                // Surface ready
            }

            @Override
            public void onSurfaceChanged(int width, int height) {
                // Surface resized
            }

            @Override
            public void onSurfaceDestroyed() {
                // Surface destroyed
            }

            @Override
            public void setQualityPreset(int preset) {
                nativeSetQualityPreset(runnerPtr, preset);
            }

            @Override
            public void setResolutionScale(float scale) {
                nativeSetResolutionScale(runnerPtr, scale);
            }

            @Override
            public void setSharpness(float sharpness) {
                nativeSetSharpness(runnerPtr, sharpness);
            }

            @Override
            public void setFSREnabled(boolean enabled) {
                nativeSetFSREnabled(runnerPtr, enabled);
            }

            @Override
            public void setTAAEnabled(boolean enabled) {
                nativeSetTAAEnabled(runnerPtr, enabled);
            }

            @Override
            public void setRCASEnabled(boolean enabled) {
                nativeSetRCASEnabled(runnerPtr, enabled);
            }

            @Override
            public void onTouch(float x, float y, boolean pressed) {
                nativeOnTouch(runnerPtr, x, y, pressed);
            }

            @Override
            public void onKey(int keyCode, boolean pressed) {
                nativeOnKey(runnerPtr, keyCode, pressed);
            }

            @Override
            public PerformanceStats getPerformanceStats() {
                return nativeGetPerformanceStats(runnerPtr);
            }
        };

        vulkanSurface.setRenderer(renderer);

        // Start stats update loop
        statsUpdater = new Runnable() {
            @Override
            public void run() {
                if (!isPaused && runnerPtr != 0) {
                    nativeRunFrame(runnerPtr);
                    PerformanceStats stats = nativeGetPerformanceStats(runnerPtr);
                    updateUI(stats);
                }
                uiHandler.postDelayed(this, 16); // ~60 FPS
            }
        };
        uiHandler.post(statsUpdater);
    }

    private void loadGame() {
        // TODO: Open file picker for NSP
        String nspPath = getExternalFilesDir(null) + "/game.nsp";
        String keysDir = getExternalFilesDir(null) + "/keys/";
        String saveDir = getExternalFilesDir(null) + "/saves/";
        
        boolean ok = nativeInitialize(runnerPtr, nspPath, keysDir, saveDir, 2); // Balanced preset
        if (!ok) {
            // Show error
        }
    }

    private void togglePause() {
        isPaused = !isPaused;
        btnPauseResume.setText(isPaused ? getString(R.string.btn_resume) : getString(R.string.btn_pause));
    }

    private void openSettings() {
        // TODO: Open settings activity
    }

    private void updateUI(PerformanceStats stats) {
        fpsText.setText(String.format("FPS: %.1f", stats.currentFps));
        frameTimeText.setText(String.format("Frame: %.2f ms", stats.frameTimeMs));
        drawCallsText.setText(String.format("Draw Calls: %d", stats.drawCalls));
        vramText.setText(String.format("VRAM: %d MB", stats.vramUsedMb));
    }

    @Override
    protected void onDestroy() {
        uiHandler.removeCallbacks(statsUpdater);
        if (runnerPtr != 0) {
            nativeShutdown(runnerPtr);
            nativeDestroyRunner(runnerPtr);
            runnerPtr = 0;
        }
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }
}