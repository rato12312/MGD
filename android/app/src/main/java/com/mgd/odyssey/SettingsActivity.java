package com.mgd.odyssey;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.preference.PreferenceManager;

import com.google.android.material.switchmaterial.SwitchMaterial;

public class SettingsActivity extends AppCompatActivity {

    private SharedPreferences prefs;
    private TextView sharpVal;
    private TextView lodBiasVal;
    private TextView resolutionVal;
    private TextView maxPolysVal;
    private TextView targetFpsVal;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);

        // Setup toolbar
        androidx.appcompat.widget.Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
            getSupportActionBar().setTitle("Ajustes");
        }

        prefs = PreferenceManager.getDefaultSharedPreferences(this);

        // Sharpness SeekBar
        SeekBar sharpRange = findViewById(R.id.sharpRange);
        sharpVal = findViewById(R.id.sharpVal);
        int sharpness = prefs.getInt("sharpness", 50);
        sharpRange.setProgress(sharpness);
        updateSharpValue(sharpness);
        sharpRange.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                updateSharpValue(progress);
                prefs.edit().putInt("sharpness", progress).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // FSR 2.x Switch
        SwitchMaterial toggleFsr = findViewById(R.id.toggleFsr);
        toggleFsr.setChecked(prefs.getBoolean("fsr_enabled", true));
        toggleFsr.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("fsr_enabled", isChecked).apply());

        // TAA Switch
        SwitchMaterial toggleTaa = findViewById(R.id.toggleTaa);
        toggleTaa.setChecked(prefs.getBoolean("taa_enabled", true));
        toggleTaa.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("taa_enabled", isChecked).apply());

        // Frame Reuse Switch
        SwitchMaterial toggleFrameReuse = findViewById(R.id.toggleFrameReuse);
        toggleFrameReuse.setChecked(prefs.getBoolean("frame_reuse_enabled", true));
        toggleFrameReuse.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("frame_reuse_enabled", isChecked).apply());

        // Framebuffer Optimizer (MFO) Switch
        SwitchMaterial toggleMfo = findViewById(R.id.toggleMfo);
        toggleMfo.setChecked(prefs.getBoolean("mfo_enabled", true));
        toggleMfo.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("mfo_enabled", isChecked).apply());

        // Seed Predictor Switch
        SwitchMaterial toggleSeed = findViewById(R.id.toggleSeed);
        toggleSeed.setChecked(prefs.getBoolean("seed_predictor_enabled", true));
        toggleSeed.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("seed_predictor_enabled", isChecked).apply());

        // Debug Overlay Switch
        SwitchMaterial toggleDebug = findViewById(R.id.toggleDebug);
        toggleDebug.setChecked(prefs.getBoolean("debug_overlay_enabled", false));
        toggleDebug.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("debug_overlay_enabled", isChecked).apply());

        // Shader Cache Switch
        SwitchMaterial toggleShaderCache = findViewById(R.id.toggleShaderCache);
        toggleShaderCache.setChecked(prefs.getBoolean("shader_cache_enabled", true));
        toggleShaderCache.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("shader_cache_enabled", isChecked).apply());

        // VSync Switch
        SwitchMaterial toggleVsync = findViewById(R.id.toggleVsync);
        toggleVsync.setChecked(prefs.getBoolean("vsync_enabled", true));
        toggleVsync.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("vsync_enabled", isChecked).apply());

        // LOD Bias SeekBar
        SeekBar lodBiasRange = findViewById(R.id.lodBiasRange);
        TextView lodBiasVal = findViewById(R.id.lodBiasVal);
        int lodBias = prefs.getInt("lod_bias", 0);
        lodBiasRange.setProgress(lodBias + 100); // -100 to 100
        updateLodBiasValue(lodBias, lodBiasVal);
        lodBiasRange.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                int value = progress - 100;
                updateLodBiasValue(value, lodBiasVal);
                prefs.edit().putInt("lod_bias", value).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Resolution Scale SeekBar
        SeekBar resolutionRange = findViewById(R.id.resolutionRange);
        TextView resolutionVal = findViewById(R.id.resolutionVal);
        int resolutionScale = (int)(prefs.getFloat("resolution_scale", 0.67f) * 100);
        resolutionRange.setProgress(resolutionScale);
        updateResolutionValue(resolutionScale, resolutionVal);
        resolutionRange.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                float scale = progress / 100f;
                updateResolutionValue(progress, resolutionVal);
                prefs.edit().putFloat("resolution_scale", scale).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Max Polygons Per Frame SeekBar
        SeekBar maxPolysRange = findViewById(R.id.maxPolysRange);
        TextView maxPolysVal = findViewById(R.id.maxPolysVal);
        int maxPolys = prefs.getInt("max_polygons_per_frame", 100000);
        maxPolysRange.setProgress(maxPolys / 1000); // 0-200 (0-200k)
        updateMaxPolysValue(maxPolys, maxPolysVal);
        maxPolysRange.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                int value = progress * 1000;
                updateMaxPolysValue(value, maxPolysVal);
                prefs.edit().putInt("max_polygons_per_frame", value).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Target FPS SeekBar
        SeekBar targetFpsRange = findViewById(R.id.targetFpsRange);
        TextView targetFpsVal = findViewById(R.id.targetFpsVal);
        int targetFps = prefs.getInt("target_fps", 60);
        targetFpsRange.setProgress(targetFps - 30); // 30-120
        updateTargetFpsValue(targetFps, targetFpsVal);
        targetFpsRange.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                int value = progress + 30;
                updateTargetFpsValue(value, targetFpsVal);
                prefs.edit().putInt("target_fps", value).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

prefs.edit().putInt("target_fps", value).apply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // VSync Switch
        SwitchMaterial toggleVsync = findViewById(R.id.toggleVsync);
        toggleVsync.setChecked(prefs.getBoolean("vsync_enabled", true));
        toggleVsync.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("vsync_enabled", isChecked).apply());

        // Debug Overlay Switch
        SwitchMaterial toggleDebug = findViewById(R.id.toggleDebug);
        toggleDebug.setChecked(prefs.getBoolean("debug_overlay_enabled", false));
        toggleDebug.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("debug_overlay_enabled", isChecked).apply());

        // Shader Cache Switch
        SwitchMaterial toggleShaderCache = findViewById(R.id.toggleShaderCache);
        toggleShaderCache.setChecked(prefs.getBoolean("shader_cache_enabled", true));
        toggleShaderCache.setOnCheckedChangeListener((buttonView, isChecked) -> 
            prefs.edit().putBoolean("shader_cache_enabled", isChecked).apply());

    }

    private void updateSharpValue(int progress) {
        sharpVal.setText(String.format("%.2f", progress / 100f));
    }

    private void updateLodBiasValue(int value, TextView tv) {
        tv.setText(String.format("%.2f", value / 100f));
    }

    private void updateResolutionValue(int progress, TextView tv) {
        float scale = progress / 100f;
        tv.setText(String.format("%.2fx", scale));
    }

    private void updateMaxPolysValue(int value, TextView tv) {
        tv.setText(String.format("%dk", value / 1000));
    }

    private void updateTargetFpsValue(int value, TextView tv) {
        tv.setText(value + " FPS");
    }

    @Override
    public boolean onSupportNavigateUp() {
        onBackPressed();
        return true;
    }
}