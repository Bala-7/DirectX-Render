# PCSS Soft Shadows

## Purpose
Adds Percentage-Closer Soft Shadows (PCSS) for softer, distance-dependent shadow edges.

## Implementation Summary

### Shader Branching Model
- Scene pixel shader contains two shadow evaluation paths:
	- `ComputeShadow(...)` for hard shadow lookup
	- `ComputeShadowPCSS(...)` for soft shadow evaluation
- Active path is selected per-fragment using `shadowParams.z` (`> 0.5` enables PCSS).

### PCSS Algorithm Stages
1. **Projection:** Convert shadow position to UV + receiver depth (`ProjectShadow`).
2. **Blocker Search:** Sample depth map with Poisson offsets using non-comparison sampler (`shadowDepthSampler`).
3. **Average Blocker Depth:** Compute mean depth of samples closer than receiver.
4. **Penumbra Estimation:**
	 - `penumbraRatio = saturate((receiverDepth - avgBlockerDepth) / max(avgBlockerDepth, eps))`
	 - `filterRadius = clamp(penumbraRatio * searchRadius * 4.0, minRadius, maxRadius)`
5. **Adaptive PCF:** Run comparison samples (`shadowSampler`) with computed radius and average visibility.

### Sampling Configuration
- Uses a fixed 12-tap Poisson disk for both blocker search and PCF filtering.
- Two sampler bindings are required:
	- `SamplerComparisonState shadowSampler` (`s1`) for filtered visibility
	- `SamplerState shadowDepthSampler` (`s2`) for raw blocker-depth reads

### CPU → GPU Parameter Mapping
- `FrameBufferData.shadowParams` layout:
	- `x`: minimum radius (currently `1 / kShadowMapSize`)
	- `y`: blocker search radius (UI-controlled)
	- `z`: PCSS enable flag (`1.0`/`0.0`)
	- `w`: max filter radius (UI-controlled)

## Runtime Controls
- PCSS can be enabled/disabled in ImGui.
- Search radius and soft filter radius are editable in ImGui.

## Notes
- PCSS parameters are passed through frame constants to the shader.
- Values are clamped at runtime for stable behavior.
- If no blockers are found in search phase, visibility returns fully lit (`1.0`).
