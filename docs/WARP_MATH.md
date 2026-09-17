# CONV image deformation in Rainstar Paint

I am Astra. I implemented this native C++20 image-transform module for Rainstar Paint, sponsored by Joshuah, with thanks to Hashem. This implementation and its documentation are MIT licensed under the project's joshuah.rainstar persona.

The material interpolator is the canonical joint CONV* field from the research snapshot verified against the MacBook Neo backup and the published composed paper. The mesh is a separate geometric map. This distinction matters: CONV determines the colour of the stretched material at an arbitrary source coordinate; the mesh determines which source coordinate a destination pixel visits. The geometry is not a fabric simulation, and CONV is not being renamed from a bilinear image sampler.

## Research correspondence

The implementation follows the local `bfft` research checkout's:

- `experiments/conv_warp/compact_measurement/reference_source.c`, SHA-256 `de1f14b64495da678354cba7717e4c7c729ed7b075877619ed16f2ea3dd84b53`.
- `experiments/conv_warp/joint_reference.py`, SHA-256 `88f034a06b00939679dbecf1cc440b90e0952300c13829186a175dfdfca64860`, especially `canonical_sampled_control_net`, `finite_joint_control_nets`, and `evaluate_joint_atlas`.
- `output/pdf/convstar_warp_addendum.tex`, SHA-256 `87e6cf5ad6fbf11cbcfbb2b626478929de279054a0c1e027194c3ded988cfdd2`.

Those files were initially read from the authorized local `bfft-6b3e7ffa7539` research mirror. On September 17, 2026, a separate provenance check extracted only the named research files from the MacBook Neo emergency backup `20260916T015732Z-MacBook-Neo/home-bfft.tar.gz`. The addendum, its timing appendix, the native reference, the Python reference, and `PRELIMINARY_THEORY.md` were all **byte-identical** to that mirror.

The same check read the latest published composed paper from [papers_please at commit a43e178f053b9f968c18e8f14d8489a949d04120](https://github.com/falseywinchnet/papers_please/blob/a43e178f053b9f968c18e8f14d8489a949d04120/conv_paper_composed.tex), whose repository head was dated September 14, 2026. The 166,102-byte `conv_paper_composed.tex` is byte-identical in all three places: GitHub, the Neo backup, and the local mirror. Its SHA-256 is `5b89a42e9f6ac5a7cb8b6ca46ef223f3a2f2a9afa3d408ac60bf5ac9244baed4`.

This establishes that the implementation's research snapshot matches both the specified Neo backup and the latest published paper at the time of verification. It does not imply that every optional operator in that paper is implemented: the compact order coordinate and bounded area-integration limits are described below. The provenance check was read-only, and no paid editorial model was called.

The website's `web/exhibits/compact-ordered-nodal-variation/conv-core.mjs` and the application's `conv.cpp` supply the earlier ordered line-current construction and axis-aligned basin reduction. The new arbitrary-coordinate module advances beyond simply applying that resize routine: it retains a shared two-dimensional admitted control atlas. The first-factor line equations, sign ledger, and projection are the same CONV family.

The compact, bilinearly extended nodal order coordinate **β\*** is used, as in the retained native reference. The paper also describes a global metric-distance order coordinate β. This implementation does not silently claim the global metric variant. The addendum explicitly distinguishes these two coordinates.

## Compiling the continuous material

`ConvWarpField::compile` takes a straight-alpha sRGB `Image` and prepares a replacement field before publishing it. It performs the following fixed construction in double precision:

1. Convert encoded sRGB channels to premultiplied values `(αr, αg, αb, α)`. This matches the existing demonstrator's colour-space convention; it is not linear-light colour processing.
2. Form endpoint and interior finite-difference first/second jets on complete source lines. Complete lines retain the sign witnesses across flat runs.
3. Convert the jets to five raw Bernstein currents on each source interval. Select the ordered sign ledger and project each current vector onto its signed endpoint-conservation fibre.
4. Evaluate both horizontal–vertical and vertical–horizontal factor orders on the global fifth-step lattice. Derivative energies provide the nodal order coordinate, and β* interpolates that scalar coordinate between nodes.
5. Apply the fixed inverse six-node Bernstein collocation matrix on each axis. Incident cells share their entire control edges.
6. Clip each shared control to the intersection of the channelwise ranges of its incident declared two-jet supports. Restore source vertices exactly.
7. Construct each channel's finite planar dual of the support's Q1 gradient cone. Admit corrections from the degree-elevated Q1 baseline using shared-edge/interior entity capacities. Finish with one common admissible ray step.
8. Retain the resulting double-precision tensor-quintic controls and discard the construction workspace.

For one source cell the continuous field is

\[
U(u,v)=\sum_{i=0}^{5}\sum_{j=0}^{5}P_{ij}B_i^5(u)B_j^5(v).
\]

The raw line-current synthesis uses

\[
f(u)=f(0)+\sum_{k=0}^{4}c_k\tau_k(u),\qquad
\tau_k(u)=\sum_{j=k+1}^{5}B_j^5(u).
\]

The Q1/bilinear baseline in step 7 is the research algorithm's feasible state for admitting higher-order corrections. It is not a replacement sampling algorithm. An admissible region can legitimately retain little or none of a proposed correction; the data and constraints determine that outcome.

The retained field is cardinal at source vertices and continuous across complete shared edges. Its Bernstein controls satisfy the admitted support ranges and current constraints within the implementation's numerical tolerances. It is not asserted to be globally differentiable across all cell boundaries, to recover missing image detail, or to dominate every other method in an image-error metric.

All controls and arithmetic here are `double`. Unlike the retained WASM reference, there are no intermediate Float32 storage boundaries. The admission tolerance is `65536 * numeric_limits<double>::epsilon()` for normalized channel values. Source line projections retain their smaller epsilon-scaled conservation checks. Fast-math is not enabled. SIMD/vectorization that preserves the compiler's ordinary floating-point contract is allowed.

Images shorter than five nodes along an axis are extended by repeating their final row/column to five nodes before compilation. The query domain remains the original image footprint. This is an explicit boundary extension for tiny materials, not a hidden fallback to another pixel interpolator. Fully transparent pixels are represented by zero premultiplied colour, so hidden RGB bytes are not preserved through a transform.

## Mapping and pixel footprints

`AffineMap` maps source pixel-centre coordinates to target pixel-centre coordinates. `render_affine` inverts it once, then evaluates the same compiled field for every destination query. It rejects singular, excessively ill-conditioned, or nonfinite maps before changing the destination.

The physical source footprint is `[-0.5, width-0.5) × [-0.5, height-0.5)`. The continuous edge-centre value is extended constantly through the outer half-pixel strip; outside the footprint the material is transparent. `affine_bounds` includes target pixel basins intersecting the transformed footprint. To create a tight result, subtract its returned `x` and `y` from the map's translation before rendering its `w × h` output.

Two output functionals are explicit:

- `WarpSampling::Point` evaluates the interpolant at the target pixel centre. It is suitable for inexpensive drag previews and exact node inspection.
- `WarpSampling::Area`, the default, applies positive Gauss–Legendre rules over each target pixel square. The maximum inverse-map column length selects 2, 4, or 8 nodes per axis at thresholds 1.25 and 3.0 source pixels. The quadrature combines premultiplied doubles before conversion back to straight-alpha bytes. Integer translations, reflections, and quarter rotations use an exact lattice-isometry shortcut; an unchanged mesh also preserves its original nodal samples.

The area mode is **bounded numerical quadrature, not the paper's exact polygon-clipped affine moment bank or its certified projective moment bank**. Its nodes can cross source cell knots and coverage edges. Consequently no universal quadrature error bound, exact conservation claim, or alias-free guarantee is made, especially for severe reduction, narrow features, or highly compressed mesh triangles. The existing `conv_resize` path remains preferable for pure axis-aligned reduction because it integrates its ordered-factor basins. Exact clipped-cell moment integration is a separate, unimplemented extension here.

The native field itself is not an approximation by a lower-resolution image. Rendering a smaller preview does not replace or recompile its source. An application can adjust the target map and target output size for preview, then render the full target on commit.

## Reshape geometry

`make_reshape_mesh(outline, spacing)` accepts a simple lasso polygon. It removes duplicate/collinear samples without changing the polygon, normalizes its orientation, subdivides its boundary, ear-clips its interior, inserts coarse interior lattice points, and improves triangle angles with bounded Lawson edge flips. Boundary nodes retain cyclic order. The image overload uses the full rectangular pixel footprint; the polygon overload should receive the actual lasso boundary, while the compiled selected material retains its alpha mask.

Each triangle defines an affine map from its source nodes to target nodes. Moving a knob interpolates that geometric node exactly. Inverting the target triangle produces source coordinates; those coordinates are then sampled with the CONV field. The piecewise-affine geometry is continuous at shared edges, but its Jacobian can change across edges. Knobs influence their incident triangles; there is no elastic solver or automatic global relaxation.

`move_reshape_node` rejects a move if any triangle becomes inverted or near-degenerate, or if the target boundary crosses or touches a non-neighbouring edge. For a triangulated disk with the constructor's topology, positive triangle orientations and a simple consistently oriented boundary preserve a one-to-one map. The function restores the prior position on rejected moves. Topology must remain the topology produced by the constructor; callers may edit target coordinates, not invent disconnected or overlapping triangle lists.

Mesh area rendering uses a common quadrature rule selected from the largest triangle footprint. A per-pixel 64-bit ownership mask assigns each quadrature point to exactly one triangle, preventing cracks from missing shared edges and preventing duplicate contributions. Samples outside the deformed region contribute transparent zero. The work arrays are allocated once per render; there is no per-pixel allocation. The default bounded Gauss approximation has the same integration limits stated above.

Self-crossing lassos are rejected. Holes, disconnected regions, topology changes, and fold-over effects are not represented by this mesh. The constructor limits a mesh to 2,048 knobs and bounds its interior candidate lattice. A coarser spacing is required if the supplied polygon would exceed these limits. Thirty-two edge-flip sweeps bound construction work; this improves the coarse tessellation without claiming an optimal mesh in every degenerate configuration.

## Resources and ownership

The compiled field stores four double channels on a shared `(5W-4) × (5H-4)` lattice, approximately **800 bytes per source pixel**. Workspace adds the source, one streamed factor stage, line scratch, shared capacities, and cached cone normals. Compilation checks the padded source extent against **1,048,576 pixels before allocating**. It raises a clear exception rather than silently reducing the material or changing algorithms.

A one-megapixel field therefore retains about 0.84 GB. Peak compilation is approximately 1.1 GB for a fresh field; replacing an already compiled field temporarily retains both fields because failure must preserve the old state. Applications should cache only active material fields, release them when the interaction ends, and avoid copying a field unnecessarily. A larger source requires tiled/lazy canonical atlas compilation, which is not implemented here.

Compilation is intentionally separated from rendering. The field is read-only across rendering workers. The affine renderer partitions rows across at most eight named C++ worker threads, with no shared mutable sampling workspace. A caller must not mutate or recompile a field concurrently with its rendering. The mesh renderer is currently sequential. Both renderers prepare a separate image and replace the destination only after success.

For long initial compilation, a GUI should use its named background-task boundary or display a busy state. Rebuilding the field for every mouse motion defeats the API's intended lifetime.

## Validation and measured M4 timing

`tests/warp_tests.cpp` checks constant and affine-field reproduction, source-node identity, exact quarter rotation, transformed bounds, transparent-edge colour, tiny images, shared-edge continuity, support range, straight-edge admitted variation, rejected singular maps, polygon construction, interior knobs, fold rejection, concave and invalid lassos, mesh identity, knob interpolation, and quadrature seam ownership. It also compares samples against retained values independently produced by Python `finite_joint_control_nets` / `evaluate_joint_atlas` on mixed affine, wrapped, and curved-edge channels. That comparison allows `2e-5` because the research reference's proposal passes through Float32 while this implementation remains double.

The initial focused suite also passed AddressSanitizer and UndefinedBehaviorSanitizer. An optional numeric command-line argument runs a synthetic square benchmark after the tests:

```sh
clang++ -std=c++20 -O3 -Wall -Wextra -Wpedantic \
  src/warp.cpp src/image.cpp tests/warp_tests.cpp -Isrc -o /tmp/rainstar-warp-tests
/tmp/rainstar-warp-tests 1024
```

Measured on the task's **Apple M4**, with ordinary `-O3` and no fast-math, using opaque mixed ramp/wrapped/checker channels and the affine map in the test:

| Material | Atlas compilation | Render time | Retained atlas |
| --- | ---: | ---: | ---: |
| 256 × 256 | 0.46 s | 0.77 ms, point mode before area integration | 52,101,632 bytes |
| 512 × 512 | 1.86 s | 4.57 ms | 209,060,352 bytes |
| 1024 × 1024 | 7.21 s | 17.41 ms | 837,550,592 bytes |

These are local measurements for this input and map, not a performance guarantee for every image or transform. Strong reduction invokes more quadrature samples, and different source channels alter current-admission work. The compilation cost is paid once for a cached material, while a subsequent affine preview reuses the atlas.
