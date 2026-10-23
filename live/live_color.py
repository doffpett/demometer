#!/usr/bin/env python3
"""Local webcam color sampler for Particle demometer firmware r6.

Only RGB values leave this computer. Video frames stay in RAM.
The center sampling region is not an object or beer-brand detector.
"""
import argparse
import collections
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT / ".runtime"
WIDTH, HEIGHT = 480, 270
# Upper center keeps the LEDs and their strongest reflections out of the sample.
SAMPLE_BOUNDS = (.33, .08, .62, .42)
SEND_INTERVAL = 2.0
KEEPALIVE_INTERVAL = 5.0
LOST_COLOR_TIMEOUT = 3.0


def sample_color(frame):
    """Return the dominant saturated color in the upper center, or None.

    Reject very dark/white pixels and require a coherent hue. Normalize
    brightness so changing reflections do not make the strip flicker. The
    LED output favors vivid colors over an exact photographic RGB match.
    """
    image = Image.fromarray(frame)
    roi = image.crop(tuple(int(fraction * size) for fraction, size in
                           zip(SAMPLE_BOUNDS, (image.width, image.height) * 2)))
    rgb = np.asarray(roi)
    hsv = np.asarray(roi.convert("HSV"))
    mask = (hsv[:, :, 1] >= 100) & (hsv[:, :, 2] >= 35) & (hsv[:, :, 2] <= 245)
    if mask.mean() < .25:
        return None
    bins = hsv[:, :, 0].astype(np.int32) * 36 // 256
    # Favor the colored can over pale lettering and warm reflected light.
    weights = (hsv[:, :, 1][mask].astype(np.float64) / 255.0) ** 3
    histogram = np.bincount(bins[mask], weights=weights, minlength=36)
    # Hue is circular: red spans both ends of the histogram.
    scores = histogram + np.roll(histogram, 1) + np.roll(histogram, -1)
    peak = int(scores.argmax())
    # A shaded metal can can span several hues. Require a clear winner over
    # other, non-overlapping hue neighborhoods instead of an absolute majority.
    separation = np.abs(np.arange(36) - peak)
    competitors = np.minimum(separation, 36 - separation) > 3
    if (scores[peak] / histogram.sum() < .32 or
            scores[peak] < 1.25 * scores[competitors].max()):
        return None
    distance = np.abs(bins - peak)
    selected = mask & (np.minimum(distance, 36 - distance) <= 1)
    color = np.median(rgb[selected], axis=0)
    # Remove the neutral/white component and reduce secondary channels. Camera
    # RGB is gamma encoded; sending it directly makes the LEDs look washed out.
    color -= color.min()
    color = np.rint(200.0 * (color / color.max()) ** 2.2)
    return tuple(int(v) for v in np.clip(color, 0, 255))


def particle_request(path, arg=None):
    config_dir = Path.home() / ".particle"
    profile_file = config_dir / "profile.json"
    profile = json.loads(profile_file.read_text()).get("name", "particle") if profile_file.exists() else "particle"
    config = json.loads((config_dir / (profile + ".config.json")).read_text())
    token = config["access_token"]
    data = urllib.parse.urlencode({"arg": arg}).encode() if arg is not None else None
    request = urllib.request.Request("https://api.particle.io/v1/" + path,
                                     data=data, headers={"Authorization": "Bearer " + token})
    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        # Do not print headers, credentials or the response body.
        raise RuntimeError(f"Particle HTTP {error.code}; check login and connectivity") from None


def resolve_device(name):
    devices = particle_request("devices")
    matches = [d for d in devices if d.get("name") == name or d.get("id") == name]
    if len(matches) != 1:
        raise RuntimeError("Device name must match exactly one Particle device")
    device = particle_request("devices/" + matches[0]["id"])
    if "set_color" not in device.get("functions", []):
        raise RuntimeError("Device needs demometer firmware r6 with set_color")
    return matches[0]["id"]


def send_color(device_id, value):
    response = particle_request("devices/" + device_id + "/set_color", value)
    expected = 0 if value == "off" else 1
    if response.get("return_value") != expected:
        raise RuntimeError("Device did not acknowledge the color command")


def write_state(**state):
    RUNTIME.mkdir(exist_ok=True)
    state.update(pid=os.getpid(), updated_at=time.time())
    temporary = RUNTIME / "state.tmp"
    temporary.write_text(json.dumps(state, indent=2) + "\n")
    temporary.replace(RUNTIME / "state.json")


def run(args):
    RUNTIME.mkdir(exist_ok=True)
    lock = (RUNTIME / "lock").open("w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError("Live mode is already running") from None
    (RUNTIME / "pid").write_text(str(os.getpid()))
    stop = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())
    capture = None
    device_id = None
    active = False
    errors = 0
    latest = [None, 0.0]
    frame_lock = threading.Lock()
    try:
        write_state(mode="starting", device=args.device, camera=args.camera)
        device_id = resolve_device(args.device)
        ffmpeg = shutil.which("ffmpeg")
        if not ffmpeg:
            raise RuntimeError("ffmpeg is required")
        capture = subprocess.Popen([
            ffmpeg, "-nostdin", "-hide_banner", "-loglevel", "error",
            "-f", "avfoundation", "-framerate", "30", "-video_size", "1280x720",
            "-pixel_format", "uyvy422", "-i", args.camera + ":none",
            "-vf", f"scale={WIDTH}:{HEIGHT},fps=2", "-pix_fmt", "rgb24",
            "-f", "rawvideo", "pipe:1"], stdout=subprocess.PIPE)

        def read_frames():
            size = WIDTH * HEIGHT * 3
            while not stop.is_set():
                buffer = bytearray()
                while len(buffer) < size:
                    chunk = capture.stdout.read(size - len(buffer))
                    if not chunk:
                        return
                    buffer.extend(chunk)
                frame = np.frombuffer(buffer, dtype=np.uint8).reshape((HEIGHT, WIDTH, 3))
                with frame_lock:
                    latest[:] = [frame, time.monotonic()]

        reader = threading.Thread(target=read_frames, daemon=True)
        reader.start()
        write_state(mode="waiting_for_color", device=args.device, camera=args.camera, rgb=None)
        colors = collections.deque(maxlen=3)
        last_frame_at = last_valid = last_sent_at = 0.0
        last_sent = None
        started_at = time.monotonic()
        while not stop.wait(.1):
            now = time.monotonic()
            if capture.poll() is not None:
                raise RuntimeError("Camera capture stopped; check the camera connection")
            with frame_lock:
                frame, captured_at = latest
            if captured_at and now - captured_at > 5:
                raise RuntimeError("Camera frames stopped arriving")
            if not captured_at and now - started_at > 15:
                raise RuntimeError("No camera frames; check macOS camera permission")
            if frame is None or captured_at == last_frame_at:
                continue
            last_frame_at = captured_at
            color = sample_color(frame)
            if color is None:
                colors.clear()
                if not active or now - last_valid < LOST_COLOR_TIMEOUT:
                    continue
                desired = "off"
            else:
                last_valid = now
                colors.append(color)
                if len(colors) < 3:
                    continue
                stable = tuple(int(v) for v in np.median(np.array(colors), axis=0))
                changed = last_sent is None or max(abs(a - b) for a, b in zip(stable, last_sent)) >= 12
                if active and not changed and now - last_sent_at < KEEPALIVE_INTERVAL:
                    continue
                desired = ",".join(map(str, stable))
            if now - last_sent_at < SEND_INTERVAL:
                continue
            try:
                # A command may reach the device even if its response times out.
                # Mark it as potentially active so shutdown always attempts off.
                active = active or desired != "off"
                send_color(device_id, desired)
                errors = 0
                active = desired != "off"
                last_sent = stable if active else None
                write_state(mode="live" if active else "waiting_for_color",
                            device=args.device, camera=args.camera,
                            rgb=list(last_sent) if last_sent else None)
            except Exception as error:
                errors += 1
                write_state(mode="connection_error", device=args.device, error=str(error))
                if errors >= 3:
                    raise RuntimeError("Three Particle commands failed; live mode stopped") from None
            finally:
                last_sent_at = time.monotonic()
        write_state(mode="stopping", device=args.device)
    except Exception as error:
        write_state(mode="error", device=args.device, error=str(error))
        raise
    finally:
        if capture is not None:
            capture.terminate()
            try:
                capture.wait(timeout=3)
            except subprocess.TimeoutExpired:
                capture.kill()
                capture.wait()
        if device_id and active:
            try:
                send_color(device_id, "off")
            except Exception:
                # Firmware expires the override after 15s without a successful update.
                pass
        (RUNTIME / "pid").unlink(missing_ok=True)
        if stop.is_set():
            write_state(mode="stopped", device=args.device)


def running_pid():
    try:
        pid = int((RUNTIME / "pid").read_text())
        command = subprocess.check_output(["ps", "-p", str(pid), "-o", "command="], text=True)
        return pid if str(Path(__file__).resolve()) in command and " run" in command else None
    except (FileNotFoundError, ValueError, subprocess.CalledProcessError):
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["run", "start", "stop", "status"])
    parser.add_argument("--device", default="LiveDemoBergen")
    parser.add_argument("--camera", default="Logitech BRIO")
    args = parser.parse_args()
    if args.action == "run":
        run(args)
    elif args.action == "start":
        RUNTIME.mkdir(exist_ok=True)
        if running_pid():
            print("Live mode is already on.")
            return
        with (RUNTIME / "live.log").open("a") as log:
            process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "run",
                                        "--device", args.device, "--camera", args.camera],
                                       stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                       start_new_session=True)
        print(f"Starting live mode (PID {process.pid}). Hold the can in the upper center of the camera.")
    elif args.action == "stop":
        pid = running_pid()
        if pid:
            os.kill(pid, signal.SIGTERM)
            print("Stopping live mode. Normal meter display returns automatically.")
        else:
            print("Live mode is already off.")
    else:
        path = RUNTIME / "state.json"
        state = json.loads(path.read_text()) if path.exists() else {"mode": "off"}
        state["process_running"] = running_pid() is not None
        print(json.dumps(state, indent=2))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
