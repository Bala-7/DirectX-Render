# Cubemap Skybox

## Purpose

Adds a scene background skybox using a cubemap texture loaded from `Assets/Images/Skybox/Daylight`.

## Asset Source

Skybox faces are loaded from the Daylight folder using these files:

- `Daylight Box_Right.bmp`
- `Daylight Box_Left.bmp`
- `Daylight Box_Top.bmp`
- `Daylight Box_Bottom.bmp`
- `Daylight Box_Front.bmp`
- `Daylight Box_Back.bmp`

A path resolver checks multiple base candidates (`Assets/...`, `../Assets/...`, `../../Assets/...`) to support both workspace and build-directory launch contexts.

## CPU-Side Architecture

### Data Types

- `SkyboxFrameBufferData`
  - `viewProjection : float4x4`

### Loader Functions

- `ResolveSkyboxFacePaths(...)`
  - Validates that all six face files exist.
  - Returns resolved absolute/relative face paths in the expected cubemap order.

- `LoadCubemapFromFiles(...)`
  - Decodes each face with WIC as RGBA8.
  - Enforces matching dimensions across all faces.
  - Creates a single `ID3D11Texture2D` with:
    - `ArraySize = 6`
    - `MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE`
  - Creates an SRV with `D3D11_SRV_DIMENSION_TEXTURECUBE`.

## GPU Pipeline Integration

### Shaders

- **Skybox VS**
  - Input: position-only cube vertices.
  - Uses dedicated skybox constant buffer.
  - Outputs clip position with `xyww` to keep depth at far plane.
  - Passes direction vector for cubemap sampling.

- **Skybox PS**
  - Samples `TextureCube` using normalized direction.

### Draw Resources

- Dedicated position-only input layout.
- Dedicated cube mesh vertex/index buffers.
- Dedicated sampler (`MIN_MAG_MIP_LINEAR`, clamp addressing).
- Dedicated rasterizer (`Cull Front`) for inside-of-cube rendering.
- Dedicated depth-stencil state (`DepthWriteMask = ZERO`, `DepthFunc = LESS_EQUAL`).

## Frame Ordering

Skybox is rendered after opaque scene objects and before ImGui composition.

This ordering + depth state ensures:

- Existing geometry remains visible in front.
- Skybox only fills remaining far-depth background regions.
- Skybox does not overwrite depth buffer content.

## Camera Behavior

Skybox uses camera rotation but ignores camera translation.

Implementation detail:

- Start from current view matrix.
- Zero the translation row.
- Multiply by current projection.

Result: skybox appears infinitely distant and does not parallax with camera position.

## Notes

- Skybox rendering is independent from object lighting and shadow map generation.
- If any face file is missing or load fails, startup returns an explicit skybox load error dialog.
