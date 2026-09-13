package com.mgd.odyssey;

public class Stamp {
    public final String kingdom;
    public final String displayName;
    public final String subtitle;
    public final int moons;
    public final boolean hasSave;
    public final String saveTime;
    private boolean active;

    public Stamp(String kingdom, String displayName, String subtitle, int moons, boolean hasSave, String saveTime) {
        this.kingdom = kingdom;
        this.displayName = displayName;
        this.subtitle = subtitle;
        this.moons = moons;
        this.hasSave = hasSave;
        this.saveTime = saveTime;
        this.active = false;
    }

    public String getKingdom() { return kingdom; }
    public String getDisplayName() { return displayName; }
    public String getSubtitle() { return subtitle; }
    public int getMoons() { return moons; }
    public boolean hasSave() { return hasSave; }
    public String getSaveTime() { return saveTime; }
    public boolean isActive() { return active; }
    public void setActive(boolean active) { this.active = active; }
}