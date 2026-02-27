# Model Loading & Sponza Import

## Purpose

Add a runtime model import pipeline that supports at least OBJ and FBX through Assimp, supports multi-mesh assets, and renders imported geometry (including textures/material hints) in both the shadow and scene passes.

## High-Level Architecture

The implementation introduces an explicit model-resource layer on top of existing primitive rendering:

- **`ModelMesh`**: GPU-ready submesh payload (`vertexBuffer`, `indexBuffer`, `indexCount`, optional texture SRV, material color).
- **`ModelResource`**: model-level container (`id`, `name`, `meshes`).
- **`GameObject` extension**:
  - Added `MeshType::Model`
  - Added `modelId` to resolve which loaded model resource the object references.

This keeps scene graph transforms and draw traversal unchanged while allowing imported content to be rendered as multiple submeshes.

## Dependency Integration

`CMakeLists.txt` now adds:

- **Assimp** via `FetchContent` for model parsing (OBJ/FBX and other formats)
- **stb** via `FetchContent` for image decoding used by imported material textures
- `DirectXTest` links against `assimp`
- `DirectXTest` includes the stb header path

## Import Pipeline

### Path Resolution

`ResolveSponzaPath` checks multiple candidate relative paths and returns an absolute `std::filesystem::path` for robust workspace/build-directory execution.

### Model Parse (`LoadModelFromFile`)

Assimp import flags:

- `aiProcess_Triangulate`
- `aiProcess_GenNormals`
- `aiProcess_CalcTangentSpace`
- `aiProcess_ImproveCacheLocality`

For each Assimp mesh:

1. Convert vertices into engine `Vertex` layout (`position`, `normal`, `uv`).
2. Flatten face indices into a contiguous 32-bit index array.
3. Create immutable D3D11 vertex/index buffers.
4. Extract diffuse texture path from material (if available).
5. Load texture with stb and create shader resource view.
6. Store fallback material tint (`aiMaterial` diffuse color or white default).

### Texture Import (`LoadTextureFromImageFile`)

- Uses `stbi_load` with forced RGBA output.
- Creates `DXGI_FORMAT_R8G8B8A8_UNORM` texture.
- Creates SRV for pixel-shader sampling.
- Uses generated mipmaps for stable distance rendering.

## Runtime Registration

At startup, the app attempts to load Sponza from `Assets/Models/Sponza/sponza.obj` (through candidate path resolution). On success:

- Pushes a `ModelResource` into the runtime model registry (`loadedModels`).
- Creates a `GameObject` named `Sponza`.
- Assigns `MeshType::Model` and `modelId`.
- Applies transform (position and uniform scale).
- Enables shadow casting/receiving.
- Inserts into the scene graph root.

## Render Integration

## Shadow Pass

When a render item is `MeshType::Model`, the renderer:

- Resolves its `ModelResource` by `modelId`.
- Iterates every `ModelMesh` submesh.
- Updates object constant buffer (`world`, inverse-transpose, material metadata).
- Binds submesh vertex/index buffers.
- Issues `DrawIndexed` per submesh.

This ensures imported models contribute to all shadow maps.

## Scene Pass

For `MeshType::Model`, per-submesh draw does:

- Bind albedo SRV (submesh texture if present, otherwise engine fallback texture).
- Keep shadow-map SRVs bound in slots 1..3.
- Update object/material constants.
- Draw submesh indexed geometry.

Result: imported models are lit by existing spotlight/directional logic, receive shadows, and cast shadows.

## Multi-Format Notes

Because parsing is delegated to Assimp, the pipeline is format-agnostic at import call-site. OBJ and FBX are both supported through the same loader function, as long as the source content can be triangulated and provides usable vertex/material data.

## Constraints & Future Work

- Current material handling focuses on diffuse/albedo texture + base tint.
- PBR maps (normal/roughness/metallic), alpha modes, and material layering are not yet mapped.
- Imported node hierarchy is currently flattened into mesh draws under one object transform; Assimp scene-node transform preservation can be added later for full authoring fidelity.
