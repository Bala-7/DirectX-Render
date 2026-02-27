# Inspector & Components

## Purpose

Add a dedicated `Inspector` view that displays and edits components for the currently selected scene object.

## Implemented UI Layout

The editor now uses a 3-pane horizontal layout:

- **Left panel:** debug controls + scene graph tree
- **Middle panel:** game view render target preview
- **Right panel (`Inspector`):** selected-object component editing

Two draggable splitters are used:

- left/game splitter
- game/inspector splitter

Each panel enforces minimum widths to prevent unusable UI collapse.

## Selection Model

A runtime selection id (`selectedGameObjectId`) is maintained in the frame loop.

- Scene graph nodes are rendered as selectable ImGui tree nodes.
- Clicking a node updates selection.
- Inspector resolves selected object by ID (`SceneGraph::GetById`).

## Component Model

`GameObject` now owns two explicit components:

### TransformComponent

- `position : XMFLOAT3`
- `rotation : XMFLOAT3`
- `scale : XMFLOAT3`

Inspector controls:

- `DragFloat3("Position")`
- `DragFloat3("Rotation")`
- `DragFloat3("Scale")`

### RendererComponent

- `materialColor : XMFLOAT4`
- `useTexture : bool`
- `visible : bool`
- `castsShadow : bool`

Inspector controls:

- `Checkbox("Visible")`
- `Checkbox("Cast Shadows")`
- `Checkbox("Use Texture")`
- `ColorEdit3("Material Color")`

## Runtime Behavior Changes

### Visibility Toggle

Per-object visibility is now controlled by `RendererComponent::visible`.

During render-item construction:

- world transform is still computed for hierarchy correctness
- object is pushed to render list only when `visible == true`

This makes hiding/showing object rendering runtime-editable from Inspector.

### Shadow Casting Toggle

Per-object shadow participation remains controlled by `RendererComponent::castsShadow`.

During each shadow-map pass:

- draw submission is skipped when `castsShadow == false`

## Design Notes

- Transform and renderer state are separated into components to reduce `GameObject` responsibility concentration.
- Existing accessors are preserved and mapped onto component data, minimizing disruption to render code.
- The inspector editing path directly mutates component state, which is consumed by the same frame’s render traversal.
