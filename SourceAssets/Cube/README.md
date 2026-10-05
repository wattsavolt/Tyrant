# Cube.glb

Canonical glTF 2.0 unit cube for Tyrant engine render testing.

Regenerate with the `asset-authoring` skill:
`.claude/skills/asset-authoring/scripts/gen_cube_glb.py`

## Geometry

- Unit cube, ±0.5 on each axis (matches `Shapes.h` `Cube::c_Vertices`)
- 24 vertices (4 per face, hard edges), 36 indices, `TRIANGLES`
- Attributes: `POSITION` vec3, `NORMAL` vec3, `TEXCOORD_0` vec2, `TANGENT` vec4 (w = sign)
  — 1:1 with `tyr::Vertex`
- Every face maps the full `[0,1]` UV square (one shared texture across all 6 faces)

## Coordinate space — RIGHT-HANDED (glTF spec)

+X right, +Y up, **+Z toward viewer**. CCW front faces.

The engine is **left-handed** (+Z into the scene). The importer must, on load:

```
position.z    *= -1
normal.z       *= -1
tangent.xyz.z *= -1
tangent.w     *= -1
reverse triangle winding (swap 2nd/3rd index per triangle)
UVs: unchanged (glTF top-left origin == D3D)
```

## Material — `CubeMaterial`, metallic-roughness

All textures are 512×512 PNGs **packed inside the .glb** (bufferView images).

| Map | Detail |
|---|---|
| `baseColorTexture` | sRGB UV debug grid: 8×8 checker; green edge = V0 (top), red = V1 (bottom), blue = U0 (left), yellow = U1 (right); white square = UV origin (0,0) |
| `normalTexture` | linear tangent-space normal map, subtle sinusoidal bump (~19° max) |
| `metallicRoughnessTexture` + `occlusionTexture` | one ORM texture — R = occlusion (1.0), **G = roughness (0.5)**, **B = metallic (0.0)** |

`baseColorFactor` = white, `metallicFactor` = `roughnessFactor` = 1.0 (texture-driven), `doubleSided` = false.
