# DirectX-Render

A real-time DirectX 11 rendering sandbox with an integrated ImGui editor, scene graph, model loading, shadow mapping, and runtime graphics controls.

## What this project is

This project is a C++/DirectX 11 renderer focused on practical engine features in a single executable (`DirectXTest`). It combines rendering, runtime tooling, and scene management in one codebase to experiment with modern real-time techniques.

## Rendering features implemented

- **DirectX 11 forward rendering pipeline**
- **Scene graph + GameObject system** (hierarchical transforms, visibility/shadow flags, inspector editing)
- **Model loading with Assimp** (Sponza import, per-mesh material/texture handling)
- **Cubemap skybox** rendering
- **Multi-light setup**
  - 2 spotlights
  - 1 directional light
- **Shadow mapping**
  - Separate shadow maps for each light
  - Shadow map debug visualization modes
- **PCSS soft shadows** (runtime toggle and tunable parameters)
- **MSAA** runtime toggle (uses multisampling when supported by adapter)
- **ImGui editor/debug UI**
  - Split editor layout (scene/debug panel, game view, inspector)
  - Runtime rendering controls

## Tech stack

- **Language:** C++17
- **Graphics API:** Direct3D 11 (`d3d11`, `dxgi`, `d3dcompiler`)
- **Shaders:** HLSL (compiled at runtime with `D3DCompile`)
- **UI:** Dear ImGui (`imgui_impl_win32`, `imgui_impl_dx11`)
- **Asset import:** Assimp
- **Image loading:** stb_image + WIC (`windowscodecs`)
- **Build system:** CMake (Visual Studio 2022 presets)
- **Platform:** Windows (Win32)

## Build and run

### Prerequisites

- Windows 10/11
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.20+

### Configure + build (Debug)

```powershell
cmake --preset debug
cmake --build --preset build-debug
```

### Run

```powershell
.\build\Debug\DirectXTest.exe
```

## Notes

- The renderer now includes a fallback path for systems without the Direct3D debug layer installed (Graphics Tools), so Debug builds can still run.
- Assets used by default include:
  - Skybox: `Assets/Images/Skybox/Daylight`
  - Model: `Assets/Models/Sponza/sponza.obj`

## Documentation

Implementation-focused docs are in:

- `AI-Documented/README.md`
- `AI-Documented/scene/`
- `AI-Documented/rendering/`
- `AI-Documented/shadows/`
- `AI-Documented/editor/`
- `AI-Documented/environment/`
