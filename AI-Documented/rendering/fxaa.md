# FXAA — Fast Approximate Anti-Aliasing

## Purpose
A low-cost screen-space post-process that detects and blurs high-contrast edges in the final image to reduce aliasing without any additional scene geometry passes.

## Implementation Summary

### Algorithm
FXAA (Fast Approximate Anti-Aliasing) operates on the resolved LDR colour buffer as a full-screen image-space filter.  
It detects edges by comparing luminance of the current pixel with its 4 diagonal neighbours, then blends along the dominant edge direction.

Key tunable: **edge threshold** — minimum luminance contrast required to trigger the filter.

### Shaders
Both shaders are defined inline as HLSL strings and compiled at startup with `D3DCompile`:

| Shader | Source variable | Profile |
|---|---|---|
| Vertex (fullscreen quad) | `fxaaVertexShaderSource` | `vs_5_0` |
| Pixel (filter) | `fxaaPixelShaderSource` | `ps_5_0` |

The vertex shader generates a fullscreen triangle-strip quad from `SV_VertexID` with no vertex buffer.

#### Pixel Shader Logic
```hlsl
// Luma of center + 4 diagonal neighbours
float lumaRange = lumaMax - lumaMin;
if (lumaRange < edgeThreshold) return center;  // early-out, not an edge

// Compute blend direction and apply 8-tap blend
float3 rgbA = 0.5 * (sample(uv + dir*(1/3 - 0.5)) + sample(uv + dir*(2/3 - 0.5)));
float3 rgbB = rgbA*0.5 + 0.25*(sample(uv - dir*0.5) + sample(uv + dir*0.5));

return (luma(rgbB) out of [lumaMin,lumaMax]) ? rgbA : rgbB;
```

### Constant Buffer
`FxaaBufferData` — 16 bytes (`float4 params`):
| Field | Meaning |
|---|---|
| `params.x` | Texel width (`1 / sceneRenderWidth`) |
| `params.y` | Texel height (`1 / sceneRenderHeight`) |
| `params.z` | Edge threshold (`runtimeFsaaEdgeThreshold`) |
| `params.w` | Unused |

### Resources
| Resource | Description |
|---|---|
| `fsaaColorBuffer` | Output render target (display resolution, `R8G8B8A8_UNORM`) |
| `fsaaRenderTargetView` | RTV for `fsaaColorBuffer` |
| `fsaaShaderResourceView` | SRV for `fsaaColorBuffer` — fed to ImGui preview when FSAA active |
| `fxaaBuffer` | Constant buffer containing params |
| `fsaaSampler` | `MIN_MAG_MIP_LINEAR`, `TEXTURE_ADDRESS_CLAMP` |

### Frame Render Flow
1. Scene renders normally into `gameViewRenderTargetView` (standard no-AA target).
2. The FXAA pass reads `gameViewShaderResourceView` and writes to `fsaaRenderTargetView`.
3. ImGui preview uses `fsaaShaderResourceView` when FSAA is the active mode.

## Runtime Controls
| Control | Location | Default |
|---|---|---|
| Select `FSAA` in **Anti-Aliasing** combo | ImGui left panel | Off |
| **FSAA Edge Threshold** slider | ImGui left panel (visible when FSAA active) | `0.0833` |
| Valid range | — | `[0.0312, 0.2500]` |

Lower threshold → more aggressive filtering (more blur on low-contrast edges).  
Higher threshold → only strong edges are filtered (sharper but more aliased).

## Performance Notes
- One additional fullscreen pixel shader pass per frame (~0.1–0.3 ms on modern hardware).
- No additional rasterization or depth buffer work.
- Cheapest of the three implemented AA modes.

## Known Limitations
- Image-space only: sub-pixel aliasing from thin geometry is not fully removed.
- Can blur fine texture detail near edges if the threshold is too low.
- No temporal accumulation; flicker on specular highlights is not addressed.

## Related
- [MSAA Runtime Control](msaa.md)
- [SSAA — Super Sampling Anti-Aliasing](ssaa.md)
