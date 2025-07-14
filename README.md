# BongoCat Global Keyboard Visualizer (Wayland, Linux)

A real-time animated Bongo Cat overlay that reacts to your keystrokes globally - typing along as you use your keyboard

Built for **Linux (Wayland)** using **Fedora GNOME**, with direct keyboard event capturing via **libevdev**, and visuals rendered using **SDL2** + **SDL2_image**.

> Bongo Cat responds to left half keys, right half keys, both halfs pressed at the same time, and spacebar presses.

## 🔧 Requirements

- SDL2
- SDL2_image
- libevdev
- root access (for reading `/dev/input/event*` directly)

## 📦 Dependencies & Build

```bash
# Install dependencies with
sudo dnf install SDL2 SDL2_image libevdev

# And build with
gcc catkeys.c -o catkeys $(pkg-config --cflags --libs sdl2 SDL2_image libevdev) -pthread
```

## 🚀 Running

```bash
sudo ./catkeys /dev/input/eventX
# Replace /dev/input/eventX with your actual keyboard input device
```

To find the correct event number:

```bash
sudo libinput list-devices
# or
cat /proc/bus/input/devices
```

## 🛑 Notes

- This **must** be run as **root** to access raw keyboard input.
- Only tested under **Wayland** on **Fedora GNOME**.
- If it lags or stutters, verify SDL2 is using hardware acceleration.
- Make sure only one instance reads from the input device at a time.

## ⚔️ Developer Notes

Core logic breakdown:

- Captures global keystates via `libevdev` in a background thread.
- Classifies keypresses as left half of keyboard/right half of keyboard/spacebar.
- Renders appropriate animation frame using SDL2 in a loop.
- Uses mutex locks to sync state between threads.
- Falls back to idle frame when no keys are pressed after timeout.

## 🧠 Why?

Because there is no working alternatives for GNOME Wayland...


