# Scene Graph

## Purpose
Adds hierarchical scene management and visualization of all game objects in the scene.

## Core Class: SceneGraph

`SceneGraph` owns and indexes `GameObject` instances.

### Responsibilities

- Create entities with automatic unique IDs
- Maintain parent-child hierarchy
- Track root nodes for traversal entry points
- Provide ID-based lookup for runtime update/render/UI passes

### Internal Structures

- `std::vector<GameObject> m_objects`: contiguous object storage
- `std::unordered_map<uint32_t, size_t> m_indexById`: ID lookup acceleration
- `std::vector<uint32_t> m_rootIds`: root node list
- `m_nextId`: monotonic ID generator

## Render Traversal

Each frame, the engine traverses roots depth-first and creates ordered `RenderItem` entries.

Traversal steps:

1. Resolve object by ID
2. Compute `localWorld` from object transform
3. Compose `world = localWorld * parentWorld`
4. Compute inverse-transpose matrix
5. Recurse into children

The resulting render-item list is reused by:

- shadow pass (filtered by `castsShadow`)
- scene pass (filtered by `visible`)

## Scene Graph Debug Display (ImGui)

The left debug section of the split editor layout includes a selectable scene graph tree.

For each node, the UI provides:

- object name as tree label
- current selection highlight
- click-to-select behavior used by the right `Inspector` panel

Selection resolves to object ID and drives component editing (transform + renderer) in Inspector.

## Initial Graph in Current Scene

- Root: `GroundPlane` (Plane mesh)
  - Child: `SpinningCube` (Cube mesh)

## Notes

- Current graph is static in topology but dynamic in transforms.
- The architecture supports future runtime add/remove/reparent operations by extending `SceneGraph` APIs.
