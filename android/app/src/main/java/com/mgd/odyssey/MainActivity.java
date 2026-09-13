package com.mgd.odyssey;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.FileProvider;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetBehavior;
import com.google.android.material.chip.Chip;
import com.google.android.material.chip.ChipGroup;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {

    private MainViewModel viewModel;
    private RecyclerView stampRecycler;
    private StampAdapter stampAdapter;
    private BottomSheetBehavior<View> sheetBehavior;
    private View sheet;
    private View overlay;
    private View hatThrow;

    // File pickers
    private final ActivityResultLauncher<Intent> keysDirPicker = registerForActivityResult(
        new ActivityResultContracts.StartActivityForResult(),
        result -> {
            if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                Uri uri = result.getData().getData();
                if (uri != null) {
                    String path = getPathFromUri(uri);
                    viewModel.setKeysDir(path);
                    Toast.makeText(this, "Keys carregadas: " + path, Toast.LENGTH_SHORT).show();
                }
            }
        });

    private final ActivityResultLauncher<Intent> gameFilePicker = registerForActivityResult(
        new ActivityResultContracts.StartActivityForResult(),
        result -> {
            if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                Uri uri = result.getData().getData();
                if (uri != null) {
                    String path = getPathFromUri(uri);
                    viewModel.loadGame(path);
                    Toast.makeText(this, "Carregando jogo...", Toast.LENGTH_SHORT).show();
                }
            }
        });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        viewModel = new ViewModelProvider(this).get(MainViewModel.class);

        // Observe UI state
        viewModel.getGameTitle().observe(this, title -> {
            TextView tv = findViewById(R.id.gameTitle);
            tv.setText(title);
        });

        viewModel.getFps().observe(this, fps -> {
            ((TextView) findViewById(R.id.fpsTop)).setText(String.valueOf(fps));
            ((TextView) findViewById(R.id.hudFps)).setText(String.valueOf(fps));
        });

        viewModel.getFrameTime().observe(this, ms -> {
            ((TextView) findViewById(R.id.msTop)).setText(ms + " ms");
            ((TextView) findViewById(R.id.hudMs)).setText(ms + " ms");
        });

        viewModel.getGameLoaded().observe(this, loaded -> {
            findViewById(R.id.gameStatus).setVisibility(loaded ? View.VISIBLE : View.GONE);
            findViewById(R.id.quickLoadBtn).setEnabled(!loaded);
            findViewById(R.id.quickPauseBtn).setEnabled(loaded);
            findViewById(R.id.quickStepBtn).setEnabled(loaded);
            findViewById(R.id.quickSaveBtn).setEnabled(loaded);
        });

        // Setup UI
        setupStampRecycler();
        setupBottomSheet();
        setupOverlay();
        setupHatThrow();
        setupDock();
        setupPerformanceMock();
        setupFilePickers();
        observeStamps();
    }

    private void setupFilePickers() {
        findViewById(R.id.loadGameBtn).setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/octet-stream");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/octet-stream"});
            gameFilePicker.launch(intent);
        });

        findViewById(R.id.quickLoadBtn).setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/octet-stream");
            gameFilePicker.launch(intent);
        });

        findViewById(R.id.settings_keys_dir).setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            keysDirPicker.launch(intent);
        });

        // Observe keys dir
        viewModel.getKeysDir().observe(this, path -> {
            if (path != null) {
                ((TextView) findViewById(R.id.settings_keys_path)).setText(path);
            }
        });
    }

    private String getPathFromUri(Uri uri) {
        // Handle content:// and file:// URIs
        if (DocumentsContract.isDocumentUri(this, uri)) {
            String docId = DocumentsContract.getDocumentId(uri);
            String[] split = docId.split(":");
            if (split.length >= 2) {
                String type = split[0];
                String path = split[1];
                if ("primary".equals(type)) {
                    return Environment.getExternalStorageDirectory() + "/" + path;
                }
            }
        }
        return uri.getPath();
    }

    private void setupStampRecycler() {
        stampRecycler = findViewById(R.id.stampRecycler);
        stampAdapter = new StampAdapter(new ArrayList<>(), kingdom -> {
            openSheet(kingdom);
            throwHat();
        });
        stampRecycler.setLayoutManager(new GridLayoutManager(this, 2));
        stampRecycler.setAdapter(stampAdapter);
    }

    private void observeStamps() {
        viewModel.getStamps().observe(this, stamps -> {
            stampAdapter.submitList(stamps);
        });
    }

    private void setupBottomSheet() {
        sheet = findViewById(R.id.sheet);
        sheetBehavior = BottomSheetBehavior.from(sheet);
        sheetBehavior.setHideable(true);
        sheetBehavior.setPeekHeight(0);
        sheetBehavior.setFitToContents(true);
    }

    private void openSheet(String kingdom) {
        // Update sheet content based on kingdom
        viewModel.selectKingdom(kingdom);
        sheetBehavior.setState(BottomSheetBehavior.STATE_EXPANDED);
    }

    private void setupOverlay() {
        overlay = findViewById(R.id.overlay);
    }

    private void setupHatThrow() {
        hatThrow = findViewById(R.id.hatThrow);
    }

    private void throwHat() {
        hatThrow.setVisibility(View.VISIBLE);
        hatThrow.animate()
            .translationY(-300)
            .rotation(540)
            .scaleX(0.9f)
            .scaleY(0.9f)
            .setDuration(680)
            .withEndAction(() -> {
                hatThrow.setVisibility(View.GONE);
                hatThrow.setTranslationY(0);
                hatThrow.setRotation(0);
                hatThrow.setScaleX(1f);
                hatThrow.setScaleY(1f);
                findViewById(R.id.overlay).setVisibility(View.VISIBLE);
            })
            .start();
    }

    private void setupDock() {
        findViewById(R.id.dockPlay).setOnClickListener(v -> {
            throwHat();
            findViewById(R.id.overlay).postDelayed(() -> 
                findViewById(R.id.overlay).setVisibility(View.VISIBLE), 420);
        });

        findViewById(R.id.dockSettings).setOnClickListener(v -> {
            findViewById(R.id.settingsAppBar).scrollIntoView(true);
        });

        findViewById(R.id.btnPlayHero).setOnClickListener(v -> {
            throwHat();
            findViewById(R.id.overlay).postDelayed(() -> 
                findViewById(R.id.overlay).setVisibility(View.VISIBLE), 420);
        });

        findViewById(R.id.btnLaunch).setOnClickListener(v -> {
            throwHat();
            findViewById(R.id.overlay).postDelayed(() -> 
                findViewById(R.id.overlay).setVisibility(View.VISIBLE), 420);
        });

        findViewById(R.id.capBtn).setOnClickListener(v -> {
            throwHat();
        });
    }

    private void setupPerformanceMock() {
        // Mock FPS for demo
        new Thread(() -> {
            while (!Thread.interrupted()) {
                try {
                    Thread.sleep(420);
                } catch (InterruptedException e) {
                    break;
                }
                runOnUiThread(() -> {
                    int fps = 58 + (int)(Math.random() * 4);
                    double ms = 1000.0 / fps;
                    runOnUiThread(() -> {
                        ((TextView) findViewById(R.id.fpsTop)).setText(String.valueOf(fps));
                        ((TextView) findViewById(R.id.hudFps)).setText(String.valueOf(fps));
                        ((TextView) findViewById(R.id.msTop)).setText(String.format("%.1f ms", 1000.0 / fps));
                        ((TextView) findViewById(R.id.hudMs)).setText(String.format("%.1f ms", 1000.0 / fps));
                    });
                });
            } catch (InterruptedException ignored) {}
        }).start();
    }
}