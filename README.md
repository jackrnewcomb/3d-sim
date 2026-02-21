# 3D Drone Show Simulation (C++ / OpenGL)

A real-time 3D simulation and visualization project in C++ that renders and animates a drone show using OpenGL.  
This repository demonstrates graphics programming fundamentals, simulation update loops, and basic agent behavior/physics.

---

## Overview

This repository contains:

- A real-time OpenGL rendering pipeline (shaders, camera controls, asset loading)
- A simple UAV/agent model with state (position/velocity/acceleration) updated continuously
- A 3D “drone show” style scene rendered in real time
- A CMake-based build that vendors common graphics dependencies under `external/`

---

## Features

- OpenGL rendering with programmable shaders
- Camera controls + interactive visualization scaffolding
- Agent/UAV simulation updated on a fixed timestep (threaded update loop)
- Basic physics integration (position/velocity updates)
- Asset loading (OBJ models) and reusable helper utilities under `common/`

---

## Tech Stack

- **Language:** C++
- **Graphics:** OpenGL, GLSL shaders
- **Math:** GLM
- **Window/Input:** GLFW
- **OpenGL Loading:** GLEW
- **Build:** CMake
- **3D Assets:** OBJ files (see `OBJ files/`)

> Note: Most third-party libraries are included under `external/` for easier local builds.

---

## Build Instructions

### Prerequisites
- C++ compiler with C++11+ support
- CMake
- Working OpenGL drivers/tooling for your platform

### Build (out-of-source recommended)

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

## Usage 

After building, run the executable `UavSimulation.exe`.

---

## Demo 

Below is a brief demonstration of the show.

https://github.com/user-attachments/assets/b25332a9-93f2-4e81-ae0b-13d83c767b75


