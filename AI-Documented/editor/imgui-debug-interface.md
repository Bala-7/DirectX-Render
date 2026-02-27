# ImGui Debug Interface

## Purpose
Provides an in-app debug UI for runtime feature inspection and tuning.

## Implementation Summary

### Build and Backend Integration
- ImGui is integrated through CMake `FetchContent` and built as `dear_imgui` static library.
- Enabled sources include core ImGui units and backends:
	- `imgui_impl_win32.cpp`
	- `imgui_impl_dx11.cpp`
- Runtime init sequence:
	1. `ImGui::CreateContext()`
	2. `ImGui_ImplWin32_Init(window)`
	3. `ImGui_ImplDX11_Init(device, context)`

### Message Routing
- `WindowProc` forwards Win32 messages to ImGui handler first.
- If ImGui consumes the event, message handling returns early.
- This avoids duplicated input handling for UI interactions.

### Frame Lifecycle
- Every frame:
	1. `ImGui_ImplDX11_NewFrame()`
	2. `ImGui_ImplWin32_NewFrame()`
	3. `ImGui::NewFrame()`
	4. Build split editor UI (`Engine Editor`) with left debug panel, middle game-view panel, and right Inspector panel
	5. Render 3D scene into game-view render target
	6. Render ImGui to swapchain backbuffer
	7. `ImGui::Render()` + `ImGui_ImplDX11_RenderDrawData(...)`
- Shutdown path calls backend shutdown functions and `ImGui::DestroyContext()`.

### Split View Layout
- A fullscreen ImGui editor window is used as the container.
- `LeftPanel` hosts debug settings and scene graph.
- `GameViewPanel` hosts an `ImGui::Image` of the current game render SRV.
- `InspectorPanel` hosts selected-object component editing.
- Two vertical splitters enable runtime width resizing by drag.
- Splitters enforce minimum widths for all three panels.

### Game View Render Target Flow
- Added dedicated non-MSAA game-view texture with RTV + SRV.
- Added MSAA game-view RTV when MSAA is enabled.
- Scene pass targets game-view RTVs (not the backbuffer).
- MSAA path resolves into non-MSAA game-view texture for sampling in ImGui image widget.
- Backbuffer is reserved for UI composition and final present.

### Input Arbitration with Camera
- Camera mouse-look no longer captures mouse continuously.
- Mouse-look is active only when all conditions are true:
	- App window is foreground
	- Right mouse button is held
	- Cursor is over game-view panel (or a capture session is already active)
	- `ImGuiIO::WantCaptureMouse == false`
- This preserves normal cursor interaction for all ImGui widgets.

### Exposed Controls
- MSAA enable/disable (when supported by adapter).
- PCSS enable/disable.
- PCSS search radius and soft filter radius.
- Directional light intensity and RGB color.
- Shadow debug mode status display.
- Scene graph hierarchical view of all runtime game objects.

### Scene Graph Panel Content
- Displays object tree using ImGui tree nodes.
- Supports click selection for each node.
- Highlights selected node and drives Inspector content.
- Uses object IDs as stable node identifiers.

### Inspector Panel Content
- Shows currently selected object identity (name, ID, mesh type).
- `Transform` component section: editable position/rotation/scale.
- `Renderer` component section: `Visible`, `Cast Shadows`, `Use Texture`, and material color.
- Edits apply immediately to runtime state and are reflected by the same frame’s render path.

## Runtime Controls
- Mouse interaction is available directly in ImGui.
- Camera look is activated only while holding right mouse button, preventing UI conflict.
- MSAA and PCSS settings are editable exclusively via ImGui controls.

## Notes
- ImGui is linked through CMake as a dedicated static library target.
- This UI currently focuses on rendering diagnostics and tuning parameters.
- Title bar state strings are updated from UI-modified runtime values.
