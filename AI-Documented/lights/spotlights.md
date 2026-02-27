# Spotlights

## Purpose
Adds two dynamic spotlights to illuminate scene geometry with cone falloff and distance attenuation.

## Implementation Summary

### CPU-Side Configuration
- Two fixed world-space light positions are authored in C++ (`lightPosition0`, `lightPosition1`).
- Two light targets are used to derive normalized spotlight direction vectors on CPU.
- Cone and range controls are encoded as:
	- `spotlightInnerCos = cos(20°)`
	- `spotlightOuterCos = cos(30°)`
	- `spotlightRange = 12.0`

### Constant Buffer Mapping
- Spotlight data is written into `FrameBufferData` every frame:
	- `lightPosition0`, `lightPosition1`
	- `lightDirection0`, `lightDirection1`
	- `spotlightParams0`, `spotlightParams1` (`x=innerCos`, `y=outerCos`, `z=range`)
- CPU values are uploaded through `UpdateSubresource(frameBuffer, ...)` before scene draw.

### Pixel Shader Lighting Model
- Spotlight shading is computed in scene pixel shader via:
	1. `toLight = lightPosition - worldPosition`
	2. Distance attenuation: `1 / (1 + 0.25d + 0.08d²)`
	3. Cone term from `ComputeSpotFactor(...)`
	4. Range fade term: `saturate(1 - d / range)` squared
	5. Blinn/Phong-style diffuse + specular composition with material albedo
- Each spotlight result is multiplied by its shadow visibility term (`shadow0`, `shadow1`).

## Shader Interface Notes
- Spotlight calculations run in the same pixel shader path as directional light.
- World normal and world position come from vertex shader outputs (`worldNormal`, `worldPosition`).
- Specular exponent is driven by `lightingParams.x`.

## Runtime Controls
- Directional light controls remain available in runtime (keyboard/UI).
- Spotlight cone/range defaults are currently code-defined constants (not exposed in UI yet).

## Notes
- Spotlight contribution is combined with ambient and directional lighting.
- Spotlights are integrated into the same frame constant buffer as other lighting data.
- Per-light color/intensity uses `lightColor0/1` where RGB is color and alpha acts as intensity scale.
