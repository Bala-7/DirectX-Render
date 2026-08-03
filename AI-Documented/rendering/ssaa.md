# SSAA — Super Sampling Anti-Aliasing (4x)

## Purpose
Renders the scene at 2× the display resolution in both axes (4× total sample count) and box-filters the result down to the output resolution. This eliminates geometric aliasing, shader aliasing, and texture aliasing at the cost of 4× rasterization work and 4× VRAM for the SSAA buffer.

## Implementation Summary

### Constant
```cpp
constexpr UINT kSsaaScale = 2; // 2x per axis → 4x SSAA
```

### Resources (size-dependent, recreated on resize)
| Resource | Format | Dimensions |
|---|---|---|
| `ssaaColorBuffer` | `R8G8B8A8_UNORM` | `width × kSsaaScale`, `height × kSsaaScale` |
| `ssaaRenderTargetView` | RTV | same |
| `ssaaShaderResourceView` | SRV | same |
| `ssaaDepthBuffer` | `D24_UNORM_S8_UINT` | same |
| `ssaaDepthStencilView` | DSV | same |

### Downsample Shader
A dedicated pixel shader (`ssaaDownsamplePixelShaderSource`) performs a 4-tap box filter at the 4 SSAA sub-pixel positions that contribute to each output texel:

```hlsl
float2 halfTexel = params.xy * 0.5f; // params.xy = 1/ssaaWidth, 1/ssaaHeight
float3 c0 = ssaaTexture.Sample(ssaaSampler, uv + float2(-halfTexel.x, -halfTexel.y)).rgb;
float3 c1 = ssaaTexture.Sample(ssaaSampler, uv + float2( halfTexel.x, -halfTexel.y)).rgb;
float3 c2 = ssaaTexture.Sample(ssaaSampler, uv + float2(-halfTexel.x,  halfTexel.y)).rgb;
float3 c3 = ssaaTexture.Sample(ssaaSampler, uv + float2( halfTexel.x,  halfTexel.y)).rgb;
return float4((c0 + c1 + c2 + c3) * 0.25f, 1.0f);
```

The fullscreen-quad vertex shader is shared with the FXAA pass.  
Sampler: `D3D11_FILTER_MIN_MAG_MIP_LINEAR`, `D3D11_TEXTURE_ADDRESS_CLAMP`.

### Constant Buffer
`ssaaBuffer` uses the same layout as `FxaaBufferData` (`float4 params`).  
Each frame: `params.xy = (1/ssaaWidth, 1/ssaaHeight)`.

### Frame Render Flow
1. **Scene pass** — rendered to `ssaaRenderTargetView` + `ssaaDepthStencilView`.
2. **Viewport** — `activeSceneViewport` is set to `sceneRenderWidth * kSsaaScale` × `sceneRenderHeight * kSsaaScale` when SSAA is active; restored to display-res viewport after.
3. **Skybox** — inherits the SSAA viewport and renders at full upscale resolution.
4. **Downsample pass** — box-filter blit from `ssaaShaderResourceView` → `gameViewRenderTargetView` (display-resolution, the same target used by the no-AA path).
5. **ImGui preview** — reads from `gameViewShaderResourceView` (same as no-AA path, no extra branch needed).

### GPU Resource Reuse
- The FXAA fullscreen vertex shader (`fxaaVertexShader`) is reused for the downsample draw call.
- The output is written to `gameViewColorBuffer` so that the rest of the pipeline (ImGui preview, backbuffer blit) is unchanged.

## Runtime Controls
- Select **SSAA (4x)** in the **Anti-Aliasing** ImGui combo box (left panel).
- The window title reflects the active AA mode.
- SSAA is always available (no hardware capability check required).

## Performance Notes
- Rasterizes all scene geometry at 4× the pixel count — significant GPU cost.
- Shadow maps, debug overlay, and FXAA are unaffected when SSAA is the active mode.
- SSAA and MSAA/FSAA are mutually exclusive runtime options.

## Known Limitations
- Temporal effects (e.g., TAA) are not implemented; SSAA has no frame-to-frame accumulation.
- Scale factor is fixed at compile time via `kSsaaScale`.

## Related
- [MSAA Runtime Control](msaa.md)
- [FSAA / FXAA](fxaa.md)
