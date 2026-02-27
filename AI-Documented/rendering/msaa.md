# MSAA Runtime Control

## Purpose
Adds multisample anti-aliasing (MSAA) to reduce edge aliasing in the rendered image.

## Implementation Summary

### Capability Detection
- On startup, device support is validated via `CheckMultisampleQualityLevels` for:
	- `DXGI_FORMAT_R8G8B8A8_UNORM` (color)
	- `DXGI_FORMAT_D24_UNORM_S8_UINT` (depth)
- Configured sample target is `kMsaaSampleCount = 8`.
- Runtime `msaaSupported` is true only if both color and depth return quality levels > 0.

### Resource Allocation Strategy
- Non-MSAA path always exists:
	- Backbuffer RTV
	- 1x depth texture + DSV
- MSAA path is conditionally created:
	- MSAA scene color texture + RTV
	- MSAA depth texture + DSV
- Sample quality for MSAA path uses `min(colorQuality, depthQuality) - 1`.

### Frame Render Flow
- Active path is chosen every frame using `runtimeMsaaEnabled` and `msaaSupported`.
- Scene and debug overlay render into active scene targets.
- If MSAA is active, final composed image is resolved into swapchain backbuffer via `ResolveSubresource` before `Present`.

## Runtime Controls
- MSAA can be toggled through the ImGui debug interface.
- UI indicates unsupported state if the active adapter does not support configured sample count.

## Notes
- MSAA setting is now UI-driven (no direct debug key binding for this feature).
- Both scene and ImGui are composed within the same active render target path before resolve.
