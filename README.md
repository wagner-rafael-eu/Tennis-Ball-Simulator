# Tennis Ball Physics Simulator

A real-time physics simulation comparing tennis ball behavior across four professional court surfaces using realistic physics and Direct2D graphics.

## Technical Overview

This Windows application simulates the physics of tennis balls dropped from 2 meters onto four different professional tennis court surfaces. The simulation uses accurate gravitational physics, surface-specific coefficients of restitution, and real-time visualization to compare how different courts affect ball bounce characteristics.

### Core Technologies
- **Language:** C++ (ISO C++17 Standard)
- **Graphics Engine:** Microsoft Direct2D (Hardware-accelerated 2D graphics)
- **Text Rendering:** DirectWrite (High-quality text rendering)
- **Window Management:** Win32 API
- **Build System:** Microsoft Visual C++ Compiler (MSVC)
- **Target Platform:** Windows 10/11 (x64)

### Architecture
- **Main Application Class:** `D2DApp` - Manages rendering, physics updates, and user interaction
- **Physics Model:** `TennisBall` - Encapsulates ball state, velocity, position, and trajectory
- **Surface Data:** `CourtSurface` - Defines physical properties and visual appearance
- **Update Loop:** Fixed timestep (16ms) for consistent 60 FPS physics simulation
- **Rendering Pipeline:** Hardware-accelerated Direct2D immediate mode rendering

## System Requirements

### Development Requirements
- **Operating System:** Windows 10 (version 1809+) or Windows 11
- **Compiler:** MSVC 2019 or later (v142+ toolset)
- **Build Tools:** One of the following:
  - Visual Studio 2019/2022 (Community, Professional, or Enterprise)
  - Visual Studio Build Tools 2019/2022
  - Windows SDK 10.0 or later
- **Required Workloads:**
  - Desktop development with C++
  - Windows 10/11 SDK

### Runtime Requirements
- **OS:** Windows 10/11 (x64)
- **Graphics:** Direct2D compatible GPU (DirectX 10.0+)
- **Memory:** ~20 MB RAM
- **Disk Space:** ~50 KB for executable

### Dependencies (Linked Libraries)
- `d2d1.lib` - Direct2D graphics
- `dwrite.lib` - DirectWrite text rendering
- `user32.lib` - Windows user interface
- `gdi32.lib` - Graphics device interface

## Build Instructions

### Quick Build
```powershell
# Using the provided build script (recommended)
.\build.bat

# Or using PowerShell directly
& ".\build.bat"
```

### Manual Build (Visual Studio Developer Command Prompt)
```batch
# Initialize Visual Studio environment (if not already in Developer Command Prompt)
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

# Create build directory
mkdir build

# Compile with MSVC
cl.exe /EHsc /W4 /D_UNICODE /DUNICODE /std:c++17 ^
    /Fe:build\TennisBallSimulator.exe ^
    main.cpp ^
    /link d2d1.lib dwrite.lib user32.lib gdi32.lib
```

### VS Code Tasks
```powershell
# Build only
Ctrl+Shift+B → "Build Tennis Ball Simulator"

# Build and run
Ctrl+Shift+P → "Tasks: Run Task" → "Build and Run Tennis Ball Simulator"

# Clean build artifacts
Ctrl+Shift+P → "Tasks: Run Task" → "Clean Build"
```

### Compiler Flags Explained
- `/EHsc` - Enable C++ exception handling (synchronous)
- `/W4` - Warning level 4 (high, recommended)
- `/D_UNICODE /DUNICODE` - Use Unicode character set
- `/std:c++17` - C++17 standard compliance
- `/Fe:` - Specify output executable name

## Running the Application

### Execution Commands
```powershell
# Run the built executable
.\build\TennisBallSimulator.exe

# Or if copied to project root
.\TennisBallSimulator.exe

# Run from VS Code task
Ctrl+Shift+P → "Tasks: Run Task" → "Build and Run Tennis Ball Simulator"
```

### User Controls
- **SPACE** - Start/restart simulation (begins ball drop)
- **R** - Reset simulation to initial state
- **Close Window** - Exit application

## Physics Engine Details

### Simulation Parameters

| Parameter | Value | Unit | Description |
|-----------|-------|------|-------------|
| Gravity (g) | 9.81 | m/s² | Standard Earth gravity |
| Initial Height (h₀) | 2.0 | m | Drop height |
| Initial Velocity (v₀) | 0.0 | m/s | Released from rest |
| Ball Radius | 0.0335 | m | Tennis ball (6.7cm diameter) |
| Timestep (Δt) | 0.016 | s | 60 FPS update rate |
| Pixels per Meter | 100 | px/m | Visualization scaling |
| Window Size | 640×480 | px | Fixed window dimensions |

### Court Surface Properties

| Court | Surface | COR | Friction | Ball Color | Court Color | Characteristics |
|-------|---------|-----|----------|------------|-------------|-----------------|
| Roland Garros | Clay | 0.75 | 0.6 | Yellow (1.0, 0.8, 0.0) | Orange (0.82, 0.52, 0.30) | Highest bounce, slowest ball speed |
| Wimbledon | Grass | 0.70 | 0.4 | Green (0.0, 1.0, 0.0) | Green (0.2, 0.6, 0.2) | Lowest bounce, fastest ball speed |
| US Open | Hard Court | 0.73 | 0.5 | Red (1.0, 0.3, 0.3) | Blue (0.2, 0.4, 0.7) | Medium bounce, consistent |
| Laver Cup | Black Court | 0.72 | 0.5 | White (1.0, 1.0, 1.0) | Black (0.15, 0.15, 0.15) | Similar to hard court |

### Physics Equations

**Velocity Update (Euler Integration):**
```
v(t + Δt) = v(t) - g × Δt
```

**Position Update:**
```
y(t + Δt) = y(t) + v(t) × Δt
```

**Bounce Calculation (Coefficient of Restitution):**
```
v_after = -v_before × COR
```
Where COR ∈ [0, 1] (0 = no bounce, 1 = perfect elastic collision)

**Stopping Condition:**
- Velocity threshold: |v| < 0.1 m/s
- Maximum bounces: 10
- Whichever occurs first terminates ball motion

### Bounce Height Statistics (Theoretical)

For a ball dropped from height h₀ with COR e:
- First bounce height: h₁ = h₀ × e²
- Second bounce height: h₂ = h₀ × e⁴
- Third bounce height: h₃ = h₀ × e⁶

**Calculated Heights (from 2.0m drop):**

| Court | COR | 1st Bounce | 2nd Bounce | 3rd Bounce |
|-------|-----|------------|------------|------------|
| Clay | 0.75 | 1.125m | 0.633m | 0.356m |
| Grass | 0.70 | 0.980m | 0.480m | 0.235m |
| Hard | 0.73 | 1.065m | 0.567m | 0.302m |
| Black | 0.72 | 1.037m | 0.537m | 0.278m |

## Application Statistics

### Code Metrics
- **Total Lines of Code:** ~650 lines
- **Source Files:** 1 (`main.cpp`)
- **Classes:** 1 (`D2DApp`)
- **Structures:** 2 (`CourtSurface`, `BounceData`)
- **Functions:** 15+ methods
- **Constants:** 8 global constants
- **Arrays:** 1 (4 court surfaces)

### Memory Footprint
- **Executable Size:** ~40-50 KB (release build)
- **Runtime Memory:** ~15-20 MB
- **Trajectory Storage:** ~2-3 KB per ball (dynamic vector)
- **Direct2D Resources:** ~5-10 MB (GPU memory)

### Performance Characteristics
- **Frame Rate:** Locked at 60 FPS (16.67ms per frame)
- **Physics Updates:** 60 Hz
- **Simulation Duration:** ~3-5 seconds (until all balls stop)
- **Trajectory Points:** ~180-300 per ball (depending on bounces)
- **Draw Calls:** ~50-100 per frame

## Visual Layout

```
┌──────────────────────────────────────────────────────────────┐
│  Height vs Time Graph (Combined - All 4 Courts)              │
│  [Color-coded trajectories with legend]                      │
│                                                              │
│  150px tall                                                   │
└──────────────────────────────────────────────────────────────┘
│
│ Gap (~60px)
│
┌────────┬────────┬────────┬────────┐
│ Roland │Wimble- │US Open │ Laver  │
│ Garros │  don   │        │  Cup   │
│ (Clay) │(Grass) │ (Hard) │(Black) │
│        │        │        │        │
│   🟡   │   🟢   │   🔴   │   ⚪   │ ← Ball (animated)
│ Court  │ Court  │ Court  │ Court  │
│Telemetry│Telemetry│Telemetry│Telemetry│
│        │        │        │        │
│ 100px  │ 100px  │ 100px  │ 100px  │
│  tall  │  tall  │  tall  │  tall  │
└────────┴────────┴────────┴────────┘
        4 sections × 160px width

Total Window: 640×480 pixels
```

### Display Components
- **Combined Graph:** Height (0-2.5m) vs Time, 4 overlaid trajectories
- **Court Sections:** Individual visualization per surface (160px each)
- **Telemetry Display:** Time, Height, Bounce count per court
- **Bounce Markers:** Red dots at ground impact points (first 3)
- **Height Markers:** Gray horizontal lines showing ball altitude
- **Legend:** Color-coded labels for each court type

## Project Structure

```
c:\_AI\031/
│
├── main.cpp                        # Complete application (650 lines)
│   ├── Physics constants & definitions
│   ├── CourtSurface struct (properties & colors)
│   ├── BounceData struct (trajectory recording)
│   ├── TennisBall class (physics simulation)
│   ├── D2DApp class (rendering & control)
│   ├── WindowProc (message handling)
│   └── wWinMain (entry point)
│
├── build.bat                       # Automated build script
│   ├── Visual Studio detection
│   ├── Environment initialization
│   ├── Compilation with cl.exe
│   └── Error handling
│
├── README.md                       # This documentation
│
└── build/                          # Generated output directory
    ├── TennisBallSimulator.exe    # Compiled executable
    └── *.obj                       # Object files (intermediate)
```

## Implementation Details

### Rendering Pipeline
1. **BeginDraw()** - Start Direct2D rendering
2. **Clear(Black)** - Clear background
3. **Draw 4 Court Sections:**
   - Fill court rectangles (surface colors)
   - Draw court names (DirectWrite text)
   - Draw ball ellipses (colored circles)
   - Draw height markers (gray lines)
   - Draw telemetry text (time, height, bounces)
   - Draw bounce markers (red dots)
4. **Draw Combined Graph:**
   - Fill graph background (semi-transparent)
   - Draw grid lines (5 horizontal divisions)
   - Draw 4 trajectories (color-coded lines)
   - Draw legend (dots + labels)
5. **Draw Instructions** (if not started)
6. **EndDraw()** - Present frame

### Update Loop
```cpp
WM_TIMER (every 16ms):
  ├── For each ball (0-3):
  │   ├── time += Δt
  │   ├── velocity -= gravity × Δt
  │   ├── position += velocity × Δt
  │   ├── Record trajectory point
  │   ├── Check ground collision (y ≤ 0)
  │   │   ├── Apply COR: v = -v × COR
  │   │   ├── Increment bounce counter
  │   │   ├── Record bounce event
  │   │   └── Check stopping conditions
  │   └── Update active state
  └── InvalidateRect (trigger redraw)
```

## Build Output Analysis

### Successful Build
```
Building Tennis Ball Physics Simulator...
Microsoft (R) C/C++ Optimizing Compiler Version 19.xx
main.cpp
Microsoft (R) Incremental Linker Version 14.xx
Build successful! Executable: build\TennisBallSimulator.exe
Run with: .\TennisBallSimulator.exe
```

### Generated Files
- `TennisBallSimulator.exe` - Main executable (~40-50 KB)
- `main.obj` - Object file (intermediate)
- Temporary compiler files (auto-deleted)

## Troubleshooting

### Build Issues

**"Visual Studio not found"**
- Install Visual Studio 2019/2022 with C++ Desktop Development
- Or run from "Developer Command Prompt for VS"

**"Cannot open include file 'd2d1.h'"**
- Install Windows SDK via Visual Studio Installer
- Ensure Windows SDK 10.0 or later is installed

**"Unresolved external symbol"**
- Verify d2d1.lib and dwrite.lib are in linker path
- Check Windows SDK is properly configured

### Runtime Issues

**Window doesn't appear**
- Check if executable is blocked by antivirus
- Verify Windows version is 10/11
- Ensure graphics drivers support DirectX 10+

**Choppy animation**
- Close resource-intensive applications
- Check if GPU drivers are updated
- System may be under heavy load

## All Commands Reference

### Build Commands
```powershell
# Standard build
.\build.bat

# Manual build with MSVC
cl.exe /EHsc /W4 /D_UNICODE /DUNICODE /std:c++17 /Fe:build\TennisBallSimulator.exe main.cpp /link d2d1.lib dwrite.lib user32.lib gdi32.lib

# Clean build directory
Remove-Item -Path .\build -Recurse -Force -ErrorAction SilentlyContinue

# Build from VS Code (Ctrl+Shift+B)
```

### Run Commands
```powershell
# Execute built program
.\build\TennisBallSimulator.exe

# Execute from root (if copied)
.\TennisBallSimulator.exe

# Run in background
Start-Process .\build\TennisBallSimulator.exe
```

### VS Code Tasks
```powershell
# Build only (default build task)
Ctrl+Shift+B

# Build and run
Tasks: Run Task → "Build and Run Tennis Ball Simulator"

# Clean build
Tasks: Run Task → "Clean Build"
```

### Development Commands
```powershell
# Check file info
Get-Item .\build\TennisBallSimulator.exe | Select-Object Length, LastWriteTime

# View source code
Get-Content .\main.cpp

# View build script
Get-Content .\build.bat

# Find Visual Studio installations
Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "vcvars64.bat"

# Check compiler version (from Developer Command Prompt)
cl.exe /?
```

### File Operations
```powershell
# List all project files
Get-ChildItem -Recurse -File

# Check project structure
tree /F

# Copy executable to desktop
Copy-Item .\build\TennisBallSimulator.exe ~\Desktop\
```

## Future Enhancements (Not Implemented)

- Horizontal ball movement (spin effects)
- Air resistance simulation
- Ball rotation visualization
- Export trajectory data to CSV
- Adjustable drop height
- Slow-motion replay
- Multiple simultaneous drops
- Sound effects for bounces

## License & Credits

**Project:** Educational demonstration of physics simulation  
**Author:** Created for learning C++ graphics and physics  
**Date:** November 2025  
**License:** Free to use for educational purposes

**Technologies:**
- Microsoft Direct2D & DirectWrite
- Win32 API
- C++17 Standard Library

## Reference Information

### Professional Tennis Court Specifications
- **ITF Regulations:** Ball must bounce 135-147cm when dropped from 254cm
- **Real COR Range:** 0.7-0.8 (varies by ball condition and surface)
- **Court Types:** Clay (slowest), Grass (fastest), Hard (medium)

---

**Last Updated:** November 17, 2025  
**Version:** 1.0  
**Build System:** MSVC/Windows SDK
