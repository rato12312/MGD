package com.mgd.odyssey;

public class PerformanceStats {
    public long frameCount = 0;
    public double currentFps = 0.0;
    public double avgFps = 0.0;
    public double minFps = 999.0;
    public double maxFps = 0.0;
    public double frameTimeMs = 0.0;
    public double cpuTimeMs = 0.0;
    public double gpuTimeMs = 0.0;
    public long trianglesRendered = 0;
    public int drawCalls = 0;
    public int verticesRendered = 0;
    public long vramUsedMb = 0;
    public long vramBudgetMb = 0;
    public float cpuUsage = 0.0f;
    public float gpuUsage = 0.0f;
    public float batteryLevel = 1.0f;
    public float temperatureC = 0.0f;
    public boolean thermalThrottling = false;
}