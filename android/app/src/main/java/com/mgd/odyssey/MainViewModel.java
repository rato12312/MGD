package com.mgd.odyssey;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

public class MainViewModel extends ViewModel {
    private final MutableLiveData<String> gameTitle = new MutableLiveData<>("No game loaded");
    private final MutableLiveData<Integer> fps = new MutableLiveData<>(60);
    private final MutableLiveData<String> frameTime = new MutableLiveData<>("16.7 ms");
    private final MutableLiveData<Boolean> gameLoaded = new MutableLiveData<>(false);
    private final MutableLiveData<String> keysDir = new MutableLiveData<>();

    public LiveData<String> getGameTitle() { return gameTitle; }
    public LiveData<Integer> getFps() { return fps; }
    public LiveData<String> getFrameTime() { return frameTime; }
    public LiveData<Boolean> getGameLoaded() { return gameLoaded; }
    public LiveData<String> getKeysDir() { return keysDir; }

    public void setGameTitle(String title) { gameTitle.setValue(title); }
    public void setFps(int fps) { this.fps.setValue(fps); }
    public void setFrameTime(String time) { frameTime.setValue(time); }
    public void setGameLoaded(boolean loaded) { gameLoaded.setValue(loaded); }
    public void setKeysDir(String path) { keysDir.setValue(path); }
}