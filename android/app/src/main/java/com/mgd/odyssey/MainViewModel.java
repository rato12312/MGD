package com.mgd.odyssey;

import android.app.Application;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class MainViewModel extends AndroidViewModel {

    private final MutableLiveData<String> gameTitle = new MutableLiveData<>("No game loaded");
    private final MutableLiveData<Integer> fps = new MutableLiveData<>(60);
    private final MutableLiveData<String> frameTime = new MutableLiveData<>("16.7 ms");
    private final MutableLiveData<Boolean> gameLoaded = new MutableLiveData<>(false);
    private final MutableLiveData<String> keysDir = new MutableLiveData<>();
    private final MutableLiveData<String> gamePath = new MutableLiveData<>();
    private final MutableLiveData<List<Stamp>> stamps = new MutableLiveData<>(new ArrayList<>());
    private final MutableLiveData<String> selectedKingdom = new MutableLiveData<>();

    public MainViewModel(@NonNull Application application) {
        super(application);
        // Initialize with default stamps
        List<Stamp> defaultStamps = new ArrayList<>();
        stamps.setValue(defaultStamps);
    }

    // Getters
    public LiveData<String> getGameTitle() { return gameTitle; }
    public LiveData<Integer> getFps() { return fps; }
    public LiveData<String> getFrameTime() { return frameTime; }
    public LiveData<Boolean> getGameLoaded() { return gameLoaded; }
    public LiveData<String> getKeysDir() { return keysDir; }
    public LiveData<String> getGamePath() { return gamePath; }
    public LiveData<List<Stamp>> getStamps() { return stamps; }
    public LiveData<String> getSelectedKingdom() { return selectedKingdom; }

    // Setters
    public void setGameTitle(String title) { gameTitle.setValue(title); }
    public void setFps(int fps) { this.fps.setValue(fps); }
    public void setFrameTime(String time) { frameTime.setValue(time); }
    public void setGameLoaded(boolean loaded) { gameLoaded.setValue(loaded); }
    public void setKeysDir(String path) { keysDir.setValue(path); }
    public void setGamePath(String path) { gamePath.setValue(path); }
    public void setStamps(List<Stamp> stamps) { this.stamps.setValue(stamps); }
    public void selectKingdom(String kingdom) { selectedKingdom.setValue(kingdom); }

    public void loadGame(String path) {
        gamePath.setValue(path);
        // TODO: Load NSP, decrypt, boot, etc.
        gameLoaded.setValue(true);
        gameTitle.setValue(new File(path).getName());
    }

    public void setKeysDir(String path) {
        keysDir.setValue(path);
        // Load prod.keys and title.keys
    }

    public void selectKingdom(String kingdom) {
        selectedKingdom.setValue(kingdom);
    }

    public void loadStamps(List<Stamp> stamps) {
        this.stamps.setValue(stamps);
    }
}