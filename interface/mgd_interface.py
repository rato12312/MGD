#!/usr/bin/env python3
"""
MGD Engine Python Interface
Provides a simple Python API to control and monitor the MGD Engine prototype.
"""

import subprocess
import sys
import os
import json
import time
import threading
from pathlib import Path
from typing import Optional, Dict, Any, List
from dataclasses import dataclass, asdict
from enum import IntEnum


class VisualRef(IntEnum):
    NONE = 0
    BACKGROUND = 1
    GROUND = 2
    CUBE = 3
    SPHERE = 4
    PLAYER = 5
    CUSTOM = 0xFFFF


class EntityState(IntEnum):
    ACTIVE = 0
    INACTIVE = 1
    HIDDEN = 2


@dataclass
class RGBA:
    r: int = 0
    g: int = 0
    b: int = 0
    a: int = 255

    def __post_init__(self):
        self.r = max(0, min(255, self.r))
        self.g = max(0, min(255, self.g))
        self.b = max(0, min(255, self.b))
        self.a = max(0, min(255, self.a))

    def to_tuple(self) -> tuple:
        return (self.r, self.g, self.b, self.a)

    @classmethod
    def from_tuple(cls, t: tuple) -> 'RGBA':
        return cls(*t)


@dataclass
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def to_list(self) -> List[float]:
        return [self.x, self.y, self.z]

    @classmethod
    def from_list(cls, lst: List[float]) -> 'Vec3':
        return cls(*lst)


@dataclass
class Entity:
    id: int = 0
    position: Vec3 = None
    color: RGBA = None
    visual_ref: VisualRef = VisualRef.NONE
    state: EntityState = EntityState.ACTIVE
    visible: bool = True

    def __post_init__(self):
        if self.position is None:
            self.position = Vec3()
        if self.color is None:
            self.color = RGBA(255, 255, 255)


@dataclass
class CameraState:
    position: Vec3 = None
    target: Vec3 = None
    zoom: float = 1.0
    rotation: float = 0.0
    viewport_width: float = 800.0
    viewport_height: float = 600.0

    def __post_init__(self):
        if self.position is None:
            self.position = Vec3(0, 0, 10)
        if self.target is None:
            self.target = Vec3(0, 0, 0)


@dataclass
class MGDState:
    entities: List[Entity] = None
    camera: CameraState = None
    frame_count: int = 0
    fps: float = 0.0
    delta_time: float = 0.0
    total_time: float = 0.0

    def __post_init__(self):
        if self.entities is None:
            self.entities = []
        if self.camera is None:
            self.camera = CameraState()


class MGDProcess:
    def __init__(self, executable_path: str = None):
        self.executable_path = executable_path or self._find_executable()
        self.process: Optional[subprocess.Popen] = None
        self.running = False

    def _find_executable(self) -> str:
        candidates = [
            Path("build/mgd_prototype"),
            Path("build/Debug/mgd_prototype.exe"),
            Path("build/Release/mgd_prototype.exe"),
            Path("mgd_prototype"),
            Path("mgd_prototype.exe"),
        ]
        for c in candidates:
            if c.exists():
                return str(c.absolute())
        raise FileNotFoundError(
            "MGD executable not found. Build the project first with CMake."
        )

    def start(self, width: int = 800, height: int = 600, title: str = "MGD Engine") -> bool:
        if self.running:
            return True

        if not os.path.exists(self.executable_path):
            raise FileNotFoundError(f"Executable not found: {self.executable_path}")

        try:
            self.process = subprocess.Popen(
                [self.executable_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                stdin=subprocess.PIPE,
                text=True,
                bufsize=1
            )
            self.running = True
            time.sleep(0.5)
            return True
        except Exception as e:
            print(f"Failed to start MGD: {e}")
            return False

    def stop(self) -> bool:
        if not self.running or not self.process:
            return True

        try:
            self.process.terminate()
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        except Exception:
            pass

        self.running = False
        self.process = None
        return True

    def is_running(self) -> bool:
        if not self.process:
            return False
        return self.process.poll() is None


class MGDInterface:
    def __init__(self, executable_path: str = None):
        self.process = MGDProcess(executable_path)
        self.state = MGDState()
        self._lock = threading.Lock()

    def launch(self, width: int = 800, height: int = 600, title: str = "MGD Engine") -> bool:
        print(f"Launching MGD Engine ({width}x{height})...")
        return self.process.start(width, height, title)

    def shutdown(self) -> bool:
        print("Shutting down MGD Engine...")
        return self.process.stop()

    def get_state(self) -> MGDState:
        with self._lock:
            return self.state

    def update_state(self, entities: List[Dict], camera: Dict, stats: Dict) -> None:
        with self._lock:
            self.state.entities = [Entity(
                id=e.get('id', 0),
                position=Vec3(*e.get('position', [0, 0, 0])),
                color=RGBA(*e.get('color', [255, 255, 255, 255])),
                visual_ref=VisualRef(e.get('visual_ref', 0)),
                state=EntityState(e.get('state', 0)),
                visible=e.get('visible', True)
            ) for e in entities]

            self.state.camera = CameraState(
                position=Vec3(*camera.get('position', [0, 0, 10])),
                target=Vec3(*camera.get('target', [0, 0, 0])),
                zoom=camera.get('zoom', 1.0),
                rotation=camera.get('rotation', 0.0),
                viewport_width=camera.get('viewport_width', 800.0),
                viewport_height=camera.get('viewport_height', 600.0)
            )

            self.state.frame_count = stats.get('frame_count', 0)
            self.state.fps = stats.get('fps', 0.0)
            self.state.delta_time = stats.get('delta_time', 0.0)
            self.state.total_time = stats.get('total_time', 0.0)

    def print_state(self) -> None:
        state = self.get_state()
        print("\n" + "=" * 50)
        print("MGD ENGINE STATE")
        print("=" * 50)
        print(f"Frame: {state.frame_count} | FPS: {state.fps:.1f} | Time: {state.total_time:.2f}s")
        print(f"Delta Time: {state.delta_time*1000:.2f}ms")
        print(f"\nCamera:")
        print(f"  Position: ({state.camera.position.x:.1f}, {state.camera.position.y:.1f}, {state.camera.position.z:.1f})")
        print(f"  Target:   ({state.camera.target.x:.1f}, {state.camera.target.y:.1f}, {state.camera.target.z:.1f})")
        print(f"  Zoom: {state.camera.zoom:.2f} | Rotation: {state.camera.rotation:.3f}")
        print(f"\nEntities ({len(state.entities)}):")
        for e in state.entities:
            status = "ACTIVE" if e.state == EntityState.ACTIVE and e.visible else "INACTIVE"
            print(f"  #{e.id}: {e.visual_ref.name} at ({e.position.x:.1f}, {e.position.y:.1f}, {e.position.z:.1f}) "
                  f"color=({e.color.r},{e.color.g},{e.color.b},{e.color.a}) [{status}]")
        print("=" * 50 + "\n")

    def wait_for_exit(self) -> int:
        if self.process.process:
            return self.process.process.wait()
        return 0

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.shutdown()


def create_test_scene_json() -> Dict[str, Any]:
    return {
        "entities": [
            {"id": 0, "position": [0, 0, -100], "color": [30, 30, 50, 255], "visual_ref": 1, "state": 0, "visible": True},
            {"id": 0, "position": [0, 0, 0], "color": [60, 60, 80, 255], "visual_ref": 2, "state": 0, "visible": True},
            {"id": 0, "position": [-200, 0, 50], "color": [100, 150, 200, 255], "visual_ref": 3, "state": 0, "visible": True},
            {"id": 0, "position": [-100, 0, 50], "color": [130, 150, 200, 255], "visual_ref": 3, "state": 0, "visible": True},
            {"id": 0, "position": [0, 0, 50], "color": [160, 150, 200, 255], "visual_ref": 3, "state": 0, "visible": True},
            {"id": 0, "position": [100, 0, 50], "color": [190, 150, 200, 255], "visual_ref": 3, "state": 0, "visible": True},
            {"id": 0, "position": [200, 0, 50], "color": [220, 150, 200, 255], "visual_ref": 3, "state": 0, "visible": True},
            {"id": 0, "position": [-150, 0, -50], "color": [200, 100, 100, 255], "visual_ref": 4, "state": 0, "visible": True},
            {"id": 0, "position": [0, 0, -50], "color": [200, 140, 100, 255], "visual_ref": 4, "state": 0, "visible": True},
            {"id": 0, "position": [150, 0, -50], "color": [200, 180, 100, 255], "visual_ref": 4, "state": 0, "visible": True},
            {"id": 999, "position": [0, 0, 20], "color": [255, 200, 50, 255], "visual_ref": 5, "state": 0, "visible": True},
        ],
        "camera": {
            "position": [0, 0, 10],
            "target": [0, 0, 0],
            "zoom": 1.0,
            "rotation": 0.0,
            "viewport_width": 800,
            "viewport_height": 600
        }
    }


def main():
    import argparse

    parser = argparse.ArgumentParser(description="MGD Engine Python Interface")
    parser.add_argument("--executable", help="Path to MGD executable")
    parser.add_argument("--width", type=int, default=800, help="Window width")
    parser.add_argument("--height", type=int, default=600, help="Window height")
    parser.add_argument("--title", default="MGD Engine", help="Window title")
    parser.add_argument("--test-scene", action="store_true", help="Print test scene JSON")
    parser.add_argument("--monitor", action="store_true", help="Monitor engine state (requires IPC)")

    args = parser.parse_args()

    if args.test_scene:
        scene = create_test_scene_json()
        print(json.dumps(scene, indent=2))
        return 0

    mgd = MGDInterface(args.executable)

    try:
        if not mgd.launch(args.width, args.height, args.title):
            print("Failed to launch MGD Engine")
            return 1

        print("MGD Engine launched successfully!")
        print("Press Ctrl+C to stop...")

        if args.monitor:
            while mgd.process.is_running():
                time.sleep(1.0)
                mgd.print_state()
        else:
            mgd.wait_for_exit()

    except KeyboardInterrupt:
        print("\nInterrupted by user")
    finally:
        mgd.shutdown()

    return 0


if __name__ == "__main__":
    sys.exit(main())