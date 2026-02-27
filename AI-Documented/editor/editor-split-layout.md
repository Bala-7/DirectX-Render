# Editor Split Layout

## Purpose
Split the application view into two ImGui sections:

- **Left panel:** scene graph + debug controls
- **Right panel:** game view

This creates an editor-style workflow where runtime tuning and hierarchy inspection remain visible while viewing the rendered scene.

## Implementation Summary

### Window Composition

A fullscreen ImGui window (`Engine Editor`) is created every frame and divided into two child regions:

1. `LeftPanel` (~34% width) for controls and scene graph.
2. `GameViewPanel` (remaining width) for game rendering.

### Draggable Vertical Splitter

- A dedicated splitter widget is placed between `LeftPanel` and `GameViewPanel`.
- Dragging the separator updates runtime panel sizing in real time.
- Split behavior uses:
	- minimum left panel width
	- minimum right panel width
	- persistent ratio state during runtime session
- Cursor changes to horizontal resize while hovering/dragging (`ResizeEW`).

### Left Panel Content

- Render/debug controls (MSAA, PCSS, light controls)
- Scene graph tree with per-node metadata

### Right Panel Content

- `ImGui::Image(...)` displays an SRV-backed render texture (`gameViewShaderResourceView`)
- The displayed image is the latest resolved game render

## Render Pipeline Changes

### Before

- Scene rendered directly to swapchain/backbuffer (or MSAA scene target resolved to backbuffer)
- ImGui overlay drawn on top

### Now

- Scene renders to dedicated **game view render target**
- If MSAA is active, scene renders to an MSAA game-view target and resolves into non-MSAA game-view texture
- Backbuffer is cleared for UI and only ImGui is rendered there
- Final present shows editor UI with embedded game image

## Notes

- Game-view texture size currently matches initial window dimensions (`kWindowWidth`/`kWindowHeight`).
- This layout is a foundational step toward full editor viewport resizing and multi-view tools.
