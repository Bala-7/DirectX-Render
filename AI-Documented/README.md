# AI-Documented

This folder contains implementation-focused documentation for render engine features added through AI-assisted development.

## Feature Index

- [GameObject System](scene/game-object-system.md)
- [Scene Graph](scene/scene-graph.md)
- [Inspector & Components](editor/inspector-and-components.md)
- [Model Loading & Sponza Import](scene/model-loading-and-sponza.md)
- [Cubemap Skybox](environment/cubemap-skybox.md)
- [Editor Split Layout](editor/editor-split-layout.md)
- [Spotlights](lights/spotlights.md)
- [Spotlight Shadows](shadows/spotlight-shadows.md)
- [MSAA Runtime Control](rendering/msaa.md)
- [PCSS Soft Shadows](shadows/pcss-soft-shadows.md)
- [ImGui Debug Interface](editor/imgui-debug-interface.md)

## Current Rendering Stack (Technical Snapshot)

- **API:** DirectX 11 (`ID3D11Device`, `ID3D11DeviceContext`, `IDXGISwapChain`)
- **Main pass shaders:** Inlined HLSL strings compiled at runtime with `D3DCompile`
- **Entity layer:** `GameObject` + `SceneGraph` runtime model
- **Asset layer:** Assimp-based model importer with multi-mesh resources and per-submesh texture/material binding
- **Editor layout:** left debug/scene-graph panel + middle game-view panel + right Inspector panel
- **Shadowing:** 3 shadow maps (`spotlight0`, `spotlight1`, `directional`)
- **Background:** Cubemap skybox loaded from `Assets/Images/Skybox/Daylight`
- **Anti-aliasing:** Runtime-switchable MSAA path (8x target sample count when supported)
- **Soft shadows:** Toggleable PCSS branch in scene pixel shader
- **Debug UI:** Dear ImGui (`imgui_impl_win32` + `imgui_impl_dx11`)

## Documentation Convention

Each feature doc includes:
- Purpose
- Code-level implementation summary
- Data layout and shader/CPU parameter mapping
- Runtime pipeline behavior
- Runtime controls
- Notes and constraints

## Parameter Ownership Model

- **CPU authoritative state:** runtime toggles and tuning values are maintained in C++ variables and serialized into constant buffers each frame.
- **GPU execution state:** shaders consume the constant buffers and samplers bound during the scene pass.
- **UI integration:** ImGui writes to CPU state; no direct shader-side UI binding exists.

## Update Policy

- Update docs whenever shader signatures, constant buffer layouts, runtime controls, or rendering pass ordering change.
- Prefer updating existing feature pages rather than creating overlapping docs.
