# Spotlight Shadows

## Purpose
Adds shadow mapping support for spotlight illumination so lit objects cast and receive shadows.

## Implementation Summary

### Shadow Resources
- Three shadow textures are allocated as `DXGI_FORMAT_R32_TYPELESS` with:
	- DSV view format: `DXGI_FORMAT_D32_FLOAT`
	- SRV view format: `DXGI_FORMAT_R32_FLOAT`
- Spotlight shadows use map indices `0` and `1`; directional uses map `2`.

### Shadow Pass Generation
- For each shadow map index:
	1. Update `ShadowFrameBufferData.lightViewProjection`.
	2. Bind no color RTV and the corresponding shadow DSV.
	3. Clear depth.
	4. Render shadow-casting geometry with `shadowVertexShader`.
- Shadow pass uses a depth-biased rasterizer state to reduce self-shadow acne.

### Matrix Path
- Spotlight view matrices are built via `XMMatrixLookAtLH(lightPosition, lightTarget, worldUp)`.
- Spotlight projection uses perspective (`XMMatrixPerspectiveFovLH`).
- Combined matrices are stored in `lightViewProjection0/1` for scene sampling.

### Scene Pass Sampling
- Scene pixel shader receives projected shadow positions (`shadowPosition0/1/2`) from vertex shader.
- Shadow visibility for spotlight maps is computed through comparison sampling (`SampleCmpLevelZero`) and bias.
- Final spotlight illumination multiplies by respective shadow factor.

## Runtime Controls
- Shadow map debug visualization can be cycled using the shadow debug mode control.

## Notes
- Spotlight shadows share the existing multi-shadow pipeline.
- Directional shadow map remains part of the same shadow set.
- Shadow bias currently comes from `lightingParams.y` and is globally shared for all three lights.
