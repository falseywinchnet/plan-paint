# Portsmouth

Portsmouth is a static typeface constructed as the equal-weight geometric
midpoint of five source fonts. The project treats every shared glyph as a
shape-averaging problem rather than selecting one source as the design base.

## Agreed design specification

- Output: one static, polished font named **Portsmouth**.
- Weighting: all sources selected for a glyph contribute equally. The default
  route remains equal-weight; explicit construction-family decisions may
  select a different equal-weight subset for an individual glyph.
- Routing: Annotation Mono, Comic Shanns Mono, and Routed Gothic contribute to
  every shared glyph. Syne Mono contributes only uppercase
  letters. Bedstead contributes only lowercase letters. Uncased characters
  therefore use the three always-on sources unless a later design decision
  changes the route.
- Normalization: common em size, baseline, x-height, cap height, ascender, and
  descender landmarks, followed by a per-letter maximum-ink-height match to
  the median active-source top, scaled about the baseline. Natural horizontal
  proportions remain part of the source geometry.
- Coverage: the exact shared cmap is the guaranteed baseline. Characters
  encoded by a clear majority may be added with an explicit source-selection
  report and rendered review.
- Construction conflicts: inspect and discuss materially incompatible glyphs
  before choosing a topology. Lowercase `m` is the first target case.
- Implementation: project code and generated artifacts live under
  `/Users/quentinkuttenkuler/future/portsmouth`.
- Provenance: source metadata and design decisions are recorded here and in
  generated audit reports.

## Sources

| File | Embedded family | Version | Outline | UPM | Embedded license summary |
|---|---|---|---|---:|---|
| `AnnotationMono-VF.ttf` | Annotation Mono | 0.004 | Variable TrueType | 800 | OFL 1.1 |
| `ComicShannsMono-Regular.ttf` | Comic Shanns Mono-Regular | 1.3.0 | TrueType | 1000 | MIT |
| `SyneMono-Regular.ttf` | Syne Mono | 2.001 | TrueType | 1000 | OFL 1.1 |
| `bedstead.otf` | Bedstead | 002.002 | CFF | 1000 | Dedicated to the public domain |
| `routed-gothic.ttf` | Routed Gothic | 001.000 | TrueType | 1024 | OFL 1.1; Reserved Font Name “Routed Gothic” |

The complete embedded name-table records are retained in
`reports/source-audit.json`. Portsmouth does not use any Reserved Font Name.

`Syne-Regular.ttf` is present in the surrounding workspace but is explicitly
excluded in favor of Syne Mono. It contributes no outlines, metrics, coverage,
or metadata to Portsmouth.

### Intended use and attribution

Portsmouth is intended for personal and artistic use, not commercial
competition with any source font. Its name, documentation, and embedded
metadata must not imply that it is an original or endorsed version of a source
font. The project documentation preserves the identity and contribution of
every included source. Xenia was examined and then excluded because its
geometry is too similar to Annotation Mono for this collection. This file
records provenance and project intent; it does not make a legal determination
about similarity, derivation, or licensing.

## Shared coverage

The exact intersection of the five included fonts is the guaranteed production
baseline. Within U+0000..U+00FF it contains 167 encoded characters, all now
represented: 166 outlines plus U+0020 spacing. The audit report remains the
authoritative record for that shared set.

Portsmouth Regular 0.018 extends beyond the intersection where at least three of five
fonts encode a character and agree structurally, or where an explicit
family-derived semantic composition has passed rendered review. It encodes
227 characters: 225 outlines plus space and no-break space. Its Latin-1 layer
remains 189 of 191 graphic positions; every graphic position from U+0020
through U+00BF is represented, and the only later Latin-1 omissions are the
rejected `ß` and `ð`. The thirteen
accepted majority additions remain `¤ ¦ § ¬ ± ² ³ µ · ¹ Ð Ø ø`; version 0.016
adds `© ª` soft hyphen, `® ¶ º ½ ¾` as explicit Portsmouth compositions.
Version 0.018 promotes 38 already completed Latin Extended forms shared by all
five source cmaps. `reports/extended-coverage-audit-0.101.json` also records
the next majority-supported candidates without treating topology compatibility
as visual acceptance.
`reports/first256-coverage-audit.json` records the shared baseline; the current
TTF build ledger records the expanded production cmap.

## Geometric method

The stage order is a hard constraint:

1. **Transport.** Render each source through its appropriate adapter, normalize
   global vertical landmarks, extract Eikonal medial atoms, and solve balanced
   correspondences. No output geometry is constructed during this stage.
2. **Barycenter.** Compute the equal-weight midpoint of the transported metric
   graphs, atom positions, and Eikonal radii. Lowercase letters currently use
   four sources at weight 0.25 each; uppercase letters also use four sources at
   weight 0.25 each; uncased glyphs use three at one third each.
3. **Build geometry.** Trace each transported outer boundary, counter, and
   detached component directly as a dense equal-weight contour barycenter.
   The median skeleton is an attractor for correspondence, never the source of
   the filled letter.
4. **Merge geometry.** Assemble the independently transported paths as one
   nonzero-winding contour complex: fill outer paths, subtract their counters,
   and retain detached bodies. Curve fitting follows only after this unity has
   passed topology and visual review.

Transport operates on connected metric geometry, not semantic segmentation.
The current structural solver is a hard, gauge-invariant graph-distance
barycenter: it alternates exact atom assignment with an equal-weight mean of
the sources' intrinsic medial distances. A weak common-frame term fixes the
already agreed baseline and horizontal gauge. This is a font-specific hard
Gromov--Wasserstein-style iteration.

The connection-Laplacian idea from BFFT's night-vision work supplies an
alternative diffusive interpretation. Directly comparing each source's lowest
eigenvectors was tested and rejected: independent spectral charts have
sign/order gauges, and the resulting mismatches created false stem lobes.
Intrinsic graph distances retain the diffusion geometry while removing that
gauge. Eikonal Lanczos remains a possible downstream reconstruction/resampling
primitive, not the barycenter optimizer.

After envelope construction, Portsmouth will apply curve fitting, optical
regularization, spacing, topology checks, and source-dominance measurements.

## Rejected engine prototype

The first executable engine is implemented in `src/portsmouth_engine.py`, but
its filled-field barycenter is **rejected as a design basis**.
It measures source landmarks from outlines, applies case-specific source
routing, rasterizes filled glyphs on a common grid, performs monotone
landmark normalization, constructs signed Euclidean/Eikonal distance fields,
computes the equal-weight field mean, and reinitializes its selected level.

The raw zero set is retained for audit. When it disagrees with the modal source
topology, the engine selects the nearest area-preserving level that matches the
consensus component/hole count and survives a five-unit inward/outward margin.
This avoids silently accepting one-pixel connections. Source-distance and
topology diagnostics are emitted per glyph.

`src/vectorize.py` extracts deterministic level-set contours and simplifies
them without source-point reuse. `src/build_prototype_font.py` packages a small
TrueType research prototype for renderer testing. This polygonal prototype is
not yet the curve-fitted production font.

### Reason for rejection

- Lowercase `m` remains one connected component, but the result suppresses the
  defining construction contributed by Bedstead. Bedstead's diagonal, peaked
  shoulder graph becomes only a minor perturbation of the majority rounded
  silhouette. Equal scalar-field weight therefore does not produce equal
  structural influence.
- Uppercase `M` becomes two components in the unconstrained zero set because
  its source junctions occur at materially different depths. The constrained
  level reconnects it, but presently produces a double-spur central knot. This
  is a visible topology/correspondence decision, not a rasterization defect.

The prototype remains in the repository as a falsified baseline and regression
fixture. It must not be extended font-wide or treated as Portsmouth's design.

## Revised engine basis

Each source must first pass through a renderer adapter appropriate to that
font. Bedstead must be sampled in its native Teletext-derived grid/phase before
being lifted to the common continuous domain. Reading its large-scale outline
directly without examining native-size behavior is insufficient: the lowercase
`m` has an ordered, asymmetric construction. Its two outer peaks are
geometrically level, but the second shoulder is smaller and resolves into a
lower terminal/join, shifting its apparent visual mass downward.

After native rendering, the engine represents each glyph by the shock/medial
graph induced by its Eikonal distance field, together with radius and
boundary-action data along each graph edge. Corresponding graph edges—not
filled pixels—receive equal source weight. Repeated structures remain ordered;
their nodes retain positions in the shared glyph frame and are not separately
realigned. Thus Bedstead's first-to-second shoulder asymmetry—including peak,
width, terminal height, and visual-mass relations—survives explicitly. The
averaged graph is reconstructed into a
filled shape through its Eikonal envelope, after which topology,
dissimilarity, and vectorization stages can operate.

### Fusion experiment 01: lowercase `m`

The first structural experiment detects the three lower stem axes in every
active source and maps them to their equal-weight mean positions. This removes
accidental stem misregistration without changing the vertical relationship of
the two shoulders. It then compares:

1. the rejected unregistered filled-SDF result;
2. a stem-registered filled-SDF result; and
3. an ordered outer/inner shoulder-boundary average.

The third result preserves ordered shoulder behavior more directly, but its
columnwise reconstruction creates scallops at shoulder/stem transitions. It is
an informative intermediate, not an accepted glyph. The next reconstruction
must fit continuous centerline and radius functions along each ordered shoulder
and take their Eikonal envelope instead of splicing vertical columns.

### Transport experiments: lowercase `m`

The first balanced transport control uses equal-mass filled-ink particles and
an exact one-to-one assignment. It proves exact source marginals but describes
area more strongly than construction.

The current medial experiment samples 128 Eikonal center/radius atoms per
source. A position/radius assignment is retained as the direct control. The
structural path refines those correspondences against complete shortest-path
distance rows on each unsegmented medial graph. With Xenia removed, the four
lowercase sources contribute exactly 0.25 each. The intrinsic assignments
stabilize, and the reconstructed `m` remains one component without the false
side lobes seen in the direct control.

The current envelope is nevertheless not a production glyph. Merging only 128
disks leaves small boundary scallops and does not yet give the second shoulder
an explicit continuous optical-mass constraint. The next experiment must build
continuous capsules along intrinsic graph edges before merging, then measure
the ordered first/second shoulder mass in the finished envelope.

### Dense fingerprint experiment: lowercase `m`

This rejected pathway retains the intrinsic 128-atom median skeleton only as
a low-frequency attractor. Each source ink shape is sampled by 2,048
deterministic centroidal-Voronoi/blue-noise atoms. Every dense atom stores its
nearest source-skeleton anchor, its corresponding median-skeleton anchor, and
its radius-normalized tangent/normal offset. All four clouds are transported
into one shared median-skeleton frame.

Annealing occurs over neighborhoods in joint spatial, skeleton-anchor, and
tangent/normal coordinates. Each neighborhood first computes one centroid per
source and then averages those centroids, preventing locally denser sampling
from changing the 0.25 source weights. The temperature schedule contracts from
72 to 10 neighbors over 18 passes while a decreasing skeleton-relative tether
prevents branch exchange.

Equal-source density was then grown as one connected region from the median
skeleton to the mean source ink area. A new shock skeleton and radius field are
extracted from that annealed density; the letter was redrawn from their
Eikonal disk envelope. This method is rejected. It left a terrible optical gap
at the left stem/shoulder junction and read as a two-leg form even when raster
component and stem-peak diagnostics claimed connectivity. Connected-density
growth and radius calibration manufactured a nominal topology instead of
tracing an `m`; neither is part of the active design pathway.

### Direct ordered point-cloud trace: lowercase `m`

The active experiment extracts the actual closed boundary of every
height-normalized source as an ordered 10,240-point cloud. The four source tops
are all exactly 505 normalized units. Each boundary point receives a
median-skeleton anchor for correspondence only; the source contour itself is
never replaced by an inflated skeleton or flooded density.

A band-annealed monotone dynamic-time-warping solve aligns the four closed
paths without allowing correspondence to jump across the contour. At every
contour index, the output point is the arithmetic mean of one aligned point
from each source, preserving exact 0.25 source weight. The resulting ordered
point cloud is traced directly as the new filled outline. The current result
is one component with three unambiguous lower stems and no left-shoulder gap.
It remains an experimental polygonal trace; production curve fitting and
optical junction regularization still follow.

The drawing resolution is now 10,240 ordered points per source contour, ten
times the first accepted `m` trace. Source outline unions are evaluated on a
glyph-local grid at five samples per normalized font unit rather than by
enlarging the full em raster. Correspondence remains hierarchical at 1,024
points, then its monotone map is lifted to the full 10,240-point paths. A final
post-union scale about the baseline makes the actual transported contours'
maximum ink height exactly equal, eliminating sub-unit raster-union drift.

The first same-topology lowercase batch now covers `m n h r u v w x y z c s`.
Visual review provisionally accepts `n h u v x y z c s` as coherent research
traces. The `r` terminal/shoulder and `w` central junction remain explicit
design-review cases; they are generated artifacts, not accepted production
glyphs.

### Multi-contour unity

The active contour-complex extension now covers the countered lowercase set
`a b d e g o p q` and the detached-component set `i j`, all at 10,240 ordered
points per source contour. It performs three operations before averaging:

1. Retain only analytic loops whose component/counter role persists at the
   normalized working scale. This removes tiny overlap artifacts without
   deleting a real source counter.
2. Pair loops by role, position, and area, then synchronize their cyclic phase
   jointly. Cyclic phase fixes only the array seam; it does not give any source
   additional geometric weight.
3. For detached bodies, build a connected medial attractor independently for
   each paired component. A rectangular tittle may legitimately use a
   one-atom attractor while its boundary remains a 10,240-point trace.

Every generated letter matches the agreed source topology: `a b d e g o p q`
each retain one component and one counter, while `i j` each retain two
components and no counter. `b d e g i o p q` are coherent research traces.
The `a` upper-left aperture transition and the `j` shoulder/tittle proportion
remain explicit design-review cases. None is yet a curve-fitted production
glyph.

The first uppercase contour-complex batch covers `A B D O P Q R`, using Syne
Mono in the uppercase-only route and excluding Bedstead. All seven retain the
topology shared by their four active sources; `B` correctly retains two paired
counters. `A B D O P R` are coherent research traces. `Q` is an explicit
failure of independent-contour treatment: its tail intrudes into the counter
differently in the sources, and the fused counter develops an optical notch.
The next unity constraint must transport this tail/counter relationship as a
coupled boundary event rather than post-smoothing the symptom.

An outer/counter clearance-signature coupling was tested on `Q`. It added each
counter point's nearest-outer clearance and local direction to correspondence,
but did not materially remove the notch. That control is retained as
`multicontour-coupled-0051`; it is rejected as the general unity constraint.
The relevant datum is an optional intrusion event—present in three sources and
absent in Syne Mono—not a continuously averaged clearance field.

### Complete topology-compatible letter geometry

All 52 ASCII letters have full-resolution experimental geometry. Lowercase
`f k l t` complete the lowercase alphabet; uppercase
`C E F G H I J K L M N S T U V W X Y Z` join the earlier countered uppercase
batch. Every stored loop contains 10,240 ordered points in the normalized
1,000-UPM frame.

The reusable geometry archives live in `geometry/` as compressed NumPy arrays.
`geometry/manifest.json` is the authoritative machine-readable index: it maps
each character to its geometry, report, source route, equal source weight,
method, contour roles, and review status. The manifest builder validates all
137 character identities, UPM values, contour shapes, and point counts before
writing the index.

Forty-two ASCII letters are coherent experimental traces. Ten remain explicit
design-review cases: lowercase `a f j r w` and uppercase `G J M Q W`. These
review flags do not indicate missing geometry; they prevent structurally or
optically unresolved traces from silently entering curve fitting as approved
designs.

The archive also includes all 85 topology-compatible non-ASCII letters in the
shared cmap, bringing the full-resolution letter total to 137. This extension
covers Latin-1 and Latin Extended capital/lowercase accent families, `Æ æ`,
`Þ þ`, cedillas, tildes, rings, macrons, carons, dot-above marks, and
diaereses. Mixed contour complexes use containment
ownership: every counter is assigned to the smallest paired outer body that
contains it in every source. Thus the A counter and ring counter in `Å` remain
owned by different bodies, while detached marks receive their own connected
medial attractors.

When a very thin mark has a valid analytic contour but no shock pixel at the
working raster scale, its correspondence attractor falls back to one
maximal-inscribed atom. The boundary itself remains the complete 10,240-point
trace. This fallback was required for Syne Mono's diaeresis in `Ï Ö Ü` and
does not inflate or reconstruct the output shape.

Visual sampling accepts the new ownership behavior in forms including
`Á Ä Å Ç Ï Ñ Æ Þ þ`. Lowercase `ä` inherits the unresolved `a` aperture, and
`æ` has an over-compressed internal junction. Extended `ā` inherits the same
aperture issue, `Ġ` inherits the uppercase G junction, and `Ŵ ŵ` inherit their
base W/w central-junction reviews. The validated manifest indexes 137 letters:
121 coherent experimental traces and sixteen explicit review cases.

`reports/letter-coverage-audit.json` is the authoritative shared-letter audit.
It reports 159 shared letters, 137 archived letters, zero topology-compatible
letters missing, and twenty-two remaining topology-conflict cases,
principally because Bedstead joins an accent to the lowercase body where the
other sources keep it detached. They are deliberately not forced through a
false consensus.

### First-256 numerals and disambiguation

All topology-compatible shared digits, punctuation, currency, mathematical
signs, and fractions below U+0100 now have full-resolution geometry. The zero
is an explicit constrained construction because its sources disagree in
topology: Annotation Mono and Comic Shanns Mono contain native slashes, while
Routed Gothic does not. Portsmouth averages the three outer rings and their
reconstructed common counter at equal one-third weight, fits the already fused
Portsmouth solidus, shortens it into the ring, and forms the compound. The
rendered result has one component and two counters. Both slash ends visibly
terminate within the black ring; the exterior oval continues around them.
Rendered inspection, rather than the clipping rule's intent, is authoritative.

The numeral `1` must remain unmistakable from lowercase `i`: it has one
connected body, a left flag, and a broad foot, while `i` has a detached tittle.
Uppercase `I` must not collapse into lowercase `l`: `I` has centered bilateral
cap and foot bars, while `l` has an asymmetric top flag and a laterally shifted
foot. `reports/disambiguation-audit.json` measures these structures and links
the rendered `specimens/disambiguation-01iIl.png` review sheet.

### First installable TrueType build

`build/Portsmouth-Regular-0.010.ttf` is the first font-wide installable
artifact. It packages all 155 completed outlines below U+0100, plus U+0020
space and a semantic U+00A0 no-break space, for 157 encoded ISO-8859-1 graphic
positions and 158 glyphs including `.notdef`. C0, DEL, and C1 control positions
are correctly left out of the cmap; they are byte controls, not printable font
drawings.

The builder transforms the centered research geometry into conventional
horizontal metrics using the equal-weight mean of the routed source advances,
with a 35-unit minimum sidebearing safety floor. It reduces the 2,887,680
archived drawing points to 15,523 TrueType line points at a maximum 0.8-unit
geometric deviation. No glyph has a negative sidebearing. This is a compact
polygonal first build, not the final quadratic curve fitting pass.

The TTF reopens through fontTools, shapes through HarfBuzz, identifies through
Fontconfig, and renders through FreeType/Pillow. Its build ledger is
`reports/first-ttf-build-0.010.json`; the rendered overview is
`specimens/Portsmouth-Regular-0.010.png`.

`build/Portsmouth-Regular-0.011.ttf` is the accent-complete successor. It adds
the ten approved modal detached accents for 165 archived outlines, 167 encoded
ISO-8859-1 graphic positions, and 168 glyphs including `.notdef`. Its current
ledger is both `reports/first-ttf-build-0.011.json` and
`reports/first-ttf-build.json`. Among characters shared by every source, only
U+0040 `@` remains unresolved.

`build/Portsmouth-Regular-0.012.ttf` resolves that final shared conflict with
a user-directed exception. The `@` takes its closed-counter structure from an
equal 0.5/0.5 transport of Annotation Mono and Routed Gothic, then matches
Comic Shanns' width-to-height and advance-to-ink ratios. Comic Shanns is a
proportion reference with zero direct outline weight; its open counter does
not enter the selected topology. Version 0.012 encodes 166 outlines plus space
and no-break space. The first-256 audit consequently reports zero unresolved
topology conflicts and zero compatible glyphs missing.

`build/Portsmouth-Regular-0.013.ttf` promotes the first successful priority
redraw. Lowercase `f` now uses six synchronized boundary events around its
crossbar, so source correspondence cannot slide through the stem junction.
Comic Shanns' optional 360-unit slab foot is normalized to the modal compact
terminal width of the other three sources, then finished as a round stem cap.
All four sources retain equal 0.25 weight within that agreed structure. The
pre-event `f` geometry and report remain archived alongside the promoted form.

`build/Portsmouth-Regular-0.014.ttf` makes two construction-family corrections.
Lowercase `a` and `q` use an equal one-third fusion of Comic Shanns, Syne Mono,
and Routed Gothic. Annotation's alternate `a` construction and hooked `q`, and
Bedstead's incompatible contribution, receive zero weight for those two
glyphs. The former `a` and `q` geometry remains archived. Every existing
detached `a` accent was rebased onto the accepted one-storey body.

Version 0.014 also adds thirteen topology-compatible majority-source
characters: `¤ ¦ § ¬ ± ² ³ µ · ¹ Ð Ø ø`. Every available encoding source
contributes equally within each addition. `ß` was rejected after fusion
introduced a false counter; `ð` was rejected visually because its crossbar
collapsed into an upper knot. Seven other majority characters retain genuine
topology conflicts, and soft hyphen has only two blank encodings.

`build/Portsmouth-Regular-0.015.ttf` replaces the congested all-source `æ` with
an explicit Portsmouth ligature. The accepted Portsmouth `a` and `e` bodies are
placed at a symmetric 145-unit half-separation, united at one controlled central
stem, and retain two counters. Its wider 796-unit advance is deliberate. The
0.014 font and pre-ligature geometry remain untouched as baselines.

Quadratic fitting is isolated from the protected polygonal series. The first
control is `build/experimental/Portsmouth-Quadratic-0.015-q1.ttf`, with the
distinct embedded family name **Portsmouth Quadratic Test**. It fits 9,367
quadratic controls over 10,765 simplified knots and is compared against 0.015
at 18–112 px in
`specimens/experimental/Portsmouth-Quadratic-0.015-q1-comparison.png`. It does
not replace or share an output path with Portsmouth Regular.

Ten ISO-8859-1 graphic positions remain unencoded in 0.015: `© ª` soft hyphen,
`® ¶ º ½ ¾ ß ð`. The symbols and fractions retain source-topology conflicts;
soft hyphen has only two blank source encodings; `ß` and `ð` are rejected
fusion artifacts. U+00A0 remains the safe blank semantic exception.

`build/Portsmouth-Regular-0.016.ttf` closes the requested lower web range.
Capital `J` now has a restrained rounded top bar and numeral `1` a clean,
symmetric 360-unit foot; both repairs preserve the previously accepted body
geometry and retain their pre-repair archives. `©` and `®` use the modal
Annotation/Syne/Routed circular enclosure with accepted Portsmouth `C` and
`R`; the ordinal signs use raised accepted `a` and `o`; `½` and `¾` compose
accepted numerals around the Portsmouth solidus. The pilcrow mirrors accepted
`P` geometry into the conventional left-bowl construction and adds one family
matched descender. Soft hyphen carries the accepted hyphen outline: HarfBuzz
suppresses it with zero advance by default and exposes it only when default
ignorables are explicitly preserved. The actual-font contextual review is
`specimens/Portsmouth-Regular-0.016-web-review.png`.

The quadratic control remains frozen at its separate 0.015 experimental path.
No quadratic geometry or family output was rebuilt during the 0.016 regular
coverage pass.

`build/Portsmouth-Regular-0.017.ttf` applies one further localized correction
to capital `J`. The restrained top bar is unchanged, but its vertical return is
reduced from the first repair's 116 units to the 86-unit width observed in the
stem immediately below. Geometry beneath y=430 is retained, and both the 0.016
font and pre-width-continuity outline remain archived.

The second isolated curve experiment is
`build/experimental/Portsmouth-Quadratic-0.017-makima-q2.ttf`, embedded as the
separate family **Portsmouth Makima Quadratic Test**. It parameterizes every
closed contour by cyclic arc length, marks extrema and 24-degree structural
corners, and constructs a wrapped modified-Akima piecewise cubic Hermite
master. Hard events receive independent one-sided chord tangents. Every cubic
span is then converted through its endpoint tangent intersection and
recursively split with de Casteljau subdivision until its quadratic error is
at most 1.8 font units. Flat spans remain TrueType lines.

Across 186 Makima-compiled outline glyphs, 3,655,680 dense samples reduce to
11,437 Makima knots, 8,439 quadratic segments, and 13,266 line segments. The
maximum dense-to-master error is 2.499 units and maximum cubic-to-quadratic
error is 1.800 units. A 1000 px/em audit preserves topology for all 189 encoded
glyphs. U+00BF is the one explicit protected-polygon fallback: its accepted
outline contains a near-degenerate three-component neck, and even a tightened
Makima fit created a fourth raster component after TrueType integer
quantization. The q2 compiler therefore refuses that conversion rather than
altering topology. q2 is 87,732 bytes versus Regular's 50,840 bytes, so it is a
successful continuous-master and fidelity experiment, not yet the final size
optimization. Its three-way visual control is
`specimens/experimental/Portsmouth-Quadratic-0.017-makima-q2-comparison.png`.

## Canonical Makima family 0.100

The Makima experiment now supplies the continuous master for the canonical
family in `build/family/`. The protected polygonal artifact
`build/Portsmouth-Regular-0.017.ttf` remains unchanged as the historical
non-Makima **Portsmouth Regular**. The new installable Regular deliberately has
no `Regular` suffix in its filename or full-name title:

- `Portsmouth.ttf` — family Portsmouth, style Regular, full name Portsmouth.
- `Portsmouth-Italic.ttf` — family Portsmouth, style Italic.
- `Portsmouth-Bold.ttf` — family Portsmouth, style Bold.
- `Portsmouth-BoldItalic.ttf` — family Portsmouth, style Bold Italic.
- `PortsmouthMono.ttf` — separate family Portsmouth Mono, style Regular.

The five faces are collected in `build/family/Portsmouth-Family.ttc` and
packaged with this project specification in
`build/family/Portsmouth-Family-0.100.zip`. The archival polygonal Regular and
canonical Makima Regular share the Portsmouth/Regular style identity and
therefore should not be installed simultaneously; the archival version is a
source and comparison master rather than a second installable family face.

Italic is a geometric wind transform rather than a uniform oblique. A
10-degree baseline-anchored shear supplies the principal direction; a smooth
14-unit mid-height gust bows the displacement field while returning to the
principal cap shift. Descenders receive 86 percent of the shear. Every contour
is warped homeomorphically before Makima reconstruction, and the font records
the matching italic angle, caret slope, style flags, and name-table roles.

Bold is an Eikonal-style radius increase. The source contour complex is lifted
to a signed distance field and inflated by 22 units, with a binary search and
three-unit post-quantization clearance reserve whenever full inflation would
close a counter or merge detached bodies. Seven compounds require reduced
radii: `½` 9.97, `¾` 12.21, `à` 16.42, `á` 16.91, `è` 18.02, `é` 18.87,
and `õ` 12.70 units. Bold Italic applies the wind field only after this
topology-constrained inflation. Six congested Bold Italic compounds retain
high-resolution polygonal contours because quadratic tangent intersections
changed topology after the combined nonlinear transforms: `¶ ¾ à á é õ`.

Portsmouth Mono fixes every horizontal advance, including spaces and
`.notdef`, at exactly 575 units. Glyphs no wider than the 505-unit ink budget
are not stretched; they simply receive looser symmetric space. Twenty-five
wider glyphs use a content-aware horizontal map. Vertical ink density and
edge activity define a column-importance field, low-information space is
compressed first, and high-information stems resist compression. The most
demanding case is `æ`: it contracts from 726 to 505 ink units with local scales
between 0.539 and 0.874 instead of one uniform 0.696 scale.

The canonical family resolves q2's one inherited fallback rather than
propagating it. U+00BF's pinched outline is reconstructed as a 180-degree
rotation of Portsmouth's accepted `?`, aligned to the former inverted-question
center. The resulting two-component `¿` then supplies every family style.

`reports/family-audit-0.100.json` verifies 189 encoded characters in every
face, exact 575-unit Mono advances, five TTC faces, increased ink for every
Bold glyph, and zero raster-topology mismatches across all transformations.
The overview and detailed controls are
`specimens/Portsmouth-Family-0.100.png` and
`specimens/Portsmouth-Family-0.100-diagnostics.png`.

## Coverage and wind refinement 0.101

Portsmouth Regular 0.018 and canonical Makima family 0.101 promote the 38
completed Latin Extended outlines already present in the protected geometry
archive. Every promoted character is encoded by all five source fonts and was
constructed through the normal case-routed, equal-weight method. Coverage is
now 227 encoded characters per family face: 225 outlines plus space and
no-break space. The historical 0.017 polygonal font and family 0.100 remain
archived unchanged.

The italic wind angle advances from 10 to 13 degrees. A reproducible survey of
installed macOS fonts found 96 conventional Italic or Oblique faces with
`post.italicAngle` magnitudes between 5 and 25 degrees: median 12, first
quartile 10, third quartile 12. CSS Fonts Level 4 assigns 14 degrees when an
`oblique` angle is omitted. Portsmouth therefore uses the exact midpoint,
13 degrees. The contour warp, OpenType `post.italicAngle`, and horizontal caret
slope all record the same choice; the 14-unit nonlinear mid-height gust and
86-percent descender factor remain unchanged. The evidence and installed-face
ledger are in `reports/italic-angle-survey-0.101.json`.

The expanded family passes the full cross-face audit at 227 characters. The
audit tests cmap identity, names and style flags, 13-degree italic metadata and
caret geometry, semantic raster topology, Bold ink growth, exact 575-unit Mono
advances, and all five TTC faces. Bold Italic paragraph sign uses a protected
two-unit simplification after the stronger shear; its genuine counter remains,
while sub-16-pixel raster islands at 1000 px/em are treated as nonpersistent
quantization noise. The family bundle is
`build/family/Portsmouth-Family-0.101.zip`; visual controls are
`specimens/Portsmouth-Family-0.101.png`,
`specimens/Portsmouth-Family-0.101-diagnostics.png`, and the matched-cap-height
source comparison `specimens/Portsmouth-Parentage-0.101.png`.

## Courier-calibrated Light 0.102

Canonical family 0.102 adds `Portsmouth-Light.ttf` as a sixth face in the
Portsmouth family. It is style **Light**, OpenType weight class 300, with no
false Regular, Bold, or Italic style flag. The existing Regular, Italic, Bold,
Bold Italic, and 575-unit Mono geometries remain unchanged.

Light is a geometric weight derivation rather than a metadata alias. Courier
New Regular and Portsmouth Regular were rasterized at a matched 700-pixel `H`
cap height. Median upright-stem runs across `H I N E M` measured 50 pixels for
Courier New and 87 for Portsmouth Regular, implying an 18.5-font-unit removal
from each Portsmouth stroke edge. The Light transform therefore erodes the
Regular signed-distance body by 18.5 units before Makima reconstruction.

Erosion is topology constrained per glyph. If the requested radius would open
a join, erase a detached body, or change a counter, binary search finds the
largest safe radius and retains a two-unit post-quantization reserve. Five
glyphs require reduced erosion: `© ª ® ½ ¾`. All other outlines receive the
full 18.5 units. At matched cap height the compiled Portsmouth Light median
stem is 53 pixels, three pixels from Courier New and 34 pixels lighter than
Portsmouth Regular.

`reports/light-reference-audit-0.102.json` records the measurement, while
`reports/family-audit-0.102.json` verifies 227 characters in every face, less
ink for every Light glyph, unchanged semantic topology, six TTC faces, and the
existing Bold, Italic, and Mono constraints. The direct visual control is
`specimens/Portsmouth-Light-0.102-comparison.png`; the installable bundle is
`build/family/Portsmouth-Family-0.102.zip`.

## Lowercase r and extended composition 0.103

Portsmouth Regular 0.019 shortens lowercase `r` without thinning its stem. The
right stem edge at x=-92.45 is a fixed anchor; only shoulder geometry to its
right is scaled horizontally by 0.90. Ink width falls from 372.55 to 344.68
units. The outline is recentered and the advance falls by the identical 27.86
units, from approximately 547.27 to 519.40, preserving the prior sidebearing.
The pre-squeeze geometry and report remain archived beside the promoted files.
Every canonical face derives `r` from this corrected master; composed `ŗ`
therefore inherits the same shoulder.

The earlier majority audit identified 78 topology-compatible candidates in
U+0100..U+024F. Unicode canonical decomposition shows that 62 are existing
Portsmouth bases plus marks: 52 use a detached accent component and ten require
an attached contour unity, principally ogoneks and the T-cedilla pair. These
glyphs now reuse accepted Portsmouth bases instead of independently averaging
the whole letter. Above marks replace the native `i/j` tittle where required;
mark clearance and centering are normalized; attached forms retain a single
semantic body. The remaining sixteen genuinely structural letters use their
full equal-weight routed fusions: `Đ đ Ħ ħ Ĳ ĳ Ŀ ŀ Ł ł Ŋ ŋ Œ œ ſ ȷ`.

The accepted review is
`specimens/extended-majority-candidates-0.103.png`. Coverage rises from 227 to
305 encoded characters in each face. The raw source-cmap audit also contains
twenty topology conflicts, rendered in
`specimens/latin-extended-topology-conflicts-0.103.png`. Three (`Ĉ`, `Ĝ`, `ķ`)
disappear under the established Syne-uppercase/Bedstead-lowercase routing;
seventeen remain actual design decisions and are not encoded yet.

Canonical family 0.103 carries the 305-character master through Light, Italic,
Bold, Bold Italic, and 575-unit Mono. Reports retain whether each addition was
composed or structurally fused, its selected source set, topology, and
10,240-point contour artifacts.

## Lowercase r spacing correction 0.104

Visual spacing measurement found that lowercase `r` retained 88-unit left and
right sidebearings after its shoulder squeeze, against lowercase medians of
65.5 and 69 units. Portsmouth Regular 0.020 therefore removes 14 units from
each side by reducing its advance 28 units without changing the accepted
outline. Light, Italic, Bold, and Bold Italic inherit the same metric change;
composed `ŗ` inherits the corrected base advance as well. Portsmouth Mono
preserves its exact 575-unit cell and instead expands only the `r` outline by
28 units around its center, recovering 14 units of optical space per side.
Canonical family 0.104 contains this spacing correction.

## Letter-spacing portfolio 0.105

The portfolio solver replaces isolated metric judgment with a separable target:
each lowercase letter contributes 67.5 units to a neighboring ink gap, producing
approximately 135 units between lowercase letters. Each capital contributes an
additional four units, producing approximately 139-unit mixed-case and 143-unit
capital-to-capital gaps. After every Regular, Light, Italic, Bold, or Bold Italic
outline transformation, the solver recenters the compiled geometry and derives
its advance from the actual ink width plus the two case-specific contributions.
This covers every encoded Unicode letter, including composed accents and
ligatures, without a pair-specific kerning table.

Portsmouth Mono retains its strict 575-unit cell. Its constrained portfolio uses
the same case targets for content-aware compression and permits at most twelve
percent expansion of narrow forms. This reduces spacing variance without making
`i`, `l`, or `r` unnaturally broad or silently breaking terminal alignment.
`reports/spacing-portfolio-audit-0.105.json` records every face's complete
letter-pair portfolio. Portsmouth Regular 0.021 and family 0.105 carry the new
metrics.

## 145-unit spacing and OpenAI Sans reference 0.106

Visual paragraph review selected a more relaxed 145-unit lowercase pair target.
Capital-to-capital pairs now target 150 units rather than preserving the earlier
four-unit-per-side increment; their 75-unit contribution is only 2.5 units above
the lowercase contribution, and mixed-case pairs target 147.5 units. Portsmouth
Regular 0.022 and canonical family 0.106 carry this portfolio through every
style.

The installed ChatGPT/Codex application was inspected as a UI reference. Its
ASAR bundle contains `OpenAISans-Regular-DFZxHTKM.woff2` and
`OpenAISans-Medium-B7nJY_kG.woff2`; it contains no Söhne-named font asset. At
1,000 UPM, bundled OpenAI Sans Regular has a 95-unit median lowercase ink gap
with 43.7 units of standard deviation. Its median raster-ridge stroke is 86
units, compared with Portsmouth Regular's 90, so their line darkness is closely
matched even though their spacing philosophies differ. OpenAI Sans uses a
conventional variable optical fit and GPOS; Portsmouth uses its deliberately
constant computational portfolio. Full measurements are retained in
`reports/openai-sans-reference-0.106.json`.

## Lighter family Regular and 140-unit spacing 1.0

The separate non-Makima `Portsmouth Regular` and Portsmouth Mono retain their
established stroke weights. After rendered comparison found the former
9.25-unit build too heavy and the 13.875-unit build too light, the final
Makima-family Regular and Italic use their midpoint: an 11.5625-unit
topology-constrained Eikonal erosion. This places the ordinary text color closer
to SF Pro's practical UI weight without changing Light, Bold, or Bold Italic.
Italic applies its 13-degree wind transform after this Regular construction.

The letter-spacing portfolio now targets 140-unit lowercase pairs and 145-unit
capital pairs. Their respective side contributions are 70 and 72.5 units, so
mixed-case pairs naturally target 142.5 without a large capital emphasis.

Version 1.0 also replaces lowercase `l` everywhere with the accepted organic
stem clipped to its uninterrupted central width. Both former terminal bars are
fully removed; `1` and `I` remain unchanged, while `|` remains a taller,
rounded full-height pipe. The protected source review is
`specimens/disambiguation-01iIl.png`, and the compiled seven-face review is
`specimens/Portsmouth-1.0-disambiguation-family.png`.

The removed bars no longer determine visible ink, but lowercase `l` retains an
invisible spacing proxy: in each proportional face its advance is raised to
exactly match lowercase `i`. Mono remains 575 units by definition. Italic and
Bold Italic use a featureless same-height profile portfolio instead of unrelated
bounding-box extrema, targeting 132-unit lowercase and 137-unit uppercase
perceptual gaps without pair kerning.

All seven faces use consistent 1.25-em typographic metrics: ascender 950,
descender -300, zero line gap, and the OS/2 `USE_TYPO_METRICS` flag. Windows
safety bounds remain 1050/350 to contain the complete 1.0 ink extremes without
controlling modern line advance. Exact per-face ink bounds, overshoots, safety
reserves, table values, and pass/fail checks are recorded in
`reports/vertical-metrics-audit-1.0.json`.

Portsmouth Mono 1.0 subsequently receives a terminal-specific spacing pass
without changing its 575-unit advance or introducing kerning. Every printable
shape, including punctuation and code operators, is passed through the same
content-aware width constraint with a 150-unit neighboring-ink target. Each
outline is then centered by its same-height 15th/85th-percentile silhouette
rather than by isolated bounding-box extrema. This makes pairs such as `ab`
feel deliberately separated while preserving exact column alignment and
allowing narrow forms their natural additional air. The exhaustive printable-
ASCII raster-profile audit is `reports/mono-spacing-audit-1.0.json`; its visual
counterpart is `specimens/Portsmouth-Mono-1.0-spacing-review.png`.

The final 1.0 master cleanup levels lowercase `i` without sterilizing its soft
foot. A fitted 0.0706-unit-per-x-unit baseline tilt is removed through a smooth
vertical taper; its residual lower-envelope slope is 0.0064 and its baseline
minimum is unchanged. Lowercase `v` receives four independently fitted quartic
wall runs with protected endpoints. The largest correction is only 4.53 units,
enough to remove the inherited single ripple without replacing Portsmouth's
organic walls with straight lines. The protected comparison and exact repair
measurements are `specimens/Portsmouth-i-v-repair.png` and
`reports/letter-repair-0069-0076.json`.

Italic capital spacing now has a geometric safety rule in addition to the
same-height optical portfolio. Every capital in Italic and Bold Italic retains
at least 60 actual units on both sides. This corrects the formerly inadequate
advances of `T` and `L`, while deliberately reserving enough support for the
tighter Portsmouth Rapids derivation.

The capital `L` also receives a geometry correction after the metric repair
showed that its physical foot could still crowd a following italic stem. The
constraint compares like with like: `T`'s top bar has a 267.22-unit half-span
from its center; `L`'s former stem-center-to-tip span was 435.52 units. Only the
rightward projection below the lower stem is compressed, pivoting at the
stem's right wall and releasing smoothly by y=160, until `L` has the same
267.22-unit center-to-tip span. The source comparison is
`specimens/Portsmouth-L-foot-repair.png`, with exact measurements in
`reports/letter-repair-004c.json`.

## Portsmouth Rapids 1.0

Portsmouth Rapids is the independent, print-oriented five-face family:
Regular, Light, Italic, Bold, and Bold Italic. It preserves the complete
Portsmouth outlines but reduces every encoded advance by 68 units, translating
the ink left by 34 units so the tightening remains balanced. It contains no
pair kerning, GPOS, or contextual substitutions; the concise rhythm is a global
metric property rather than an exception list.

The reduction is measured from AppKit rather than chosen by eye alone.
TextEdit's Tighten and Loosen actions were probed from 12 through 72 points and
consistently measured approximately 13.75 units per em. Five tighten actions
therefore equal 68.75 units; the compiled 68-unit integer solution places the
test line `Multiple Occupant Room, Hotel, End of Time` within one additional
AppKit tracking step of Arial loosened once. The tighter family still preserves
at least 26 geometric units around every italic capital. Full construction and
verification are recorded in `reports/portsmouth-rapids-1.0.json` and
`reports/portsmouth-rapids-audit-1.0.json`; the visual calibration is
`specimens/Portsmouth-Rapids-1.0-tracking.png`.

Neither Portsmouth nor Portsmouth Rapids encodes U+0009. Tabs are layout
controls whose stops are selected by the editor, terminal, or typesetter; a
font cannot reliably impose their width. U+0020 and U+00A0 remain equal in
every face. The documented recommendation is a four-space tab stop: 1,940
units for proportional Portsmouth, 2,300 units for Portsmouth Mono, and 1,668
units for Portsmouth Rapids.

The 1.0 presentation set includes an explicit language-coverage sheet at
`specimens/Portsmouth-1.0-language-support.png`. English, French, German,
Spanish, and Polish are rendered directly in Portsmouth. Ukrainian and Russian
are shown as muted target copy labeled unsupported because 1.0 contains no
Cyrillic outlines; fallback rendering is never presented as Portsmouth support.
The programming specimen at
`specimens/Portsmouth-Mono-1.0-programming.png` exercises fixed cells,
brackets, operators, pipes, quotes, interpolation syntax, and terminal text.

## Punctuation and case-sensitive typesetting audit 0.107

The family has not yet been altered for case-sensitive punctuation. A dedicated
high-resolution typesetting sheet records the existing defaults in prose, code,
all-capital, micro-rhythm, and individual-glyph contexts. Portsmouth currently
contains no GSUB, GPOS, legacy `kern`, or OpenType `case` feature.

The initial visual and geometric inspection finds the paired delimiters already
well centered on a 700-unit capital band: parentheses, square brackets, braces,
angle brackets, slash, and backslash are within roughly twenty units of its
350-unit optical center. The proportional hyphen is the clear case-sensitive
exception, centered 66 units low. Period, comma, semicolon, and colon have
approximately 160–184 units of internal sidebearing, producing 229–254-unit
letter-to-punctuation gaps despite the 140-unit lowercase letter rhythm. Mono
retains a full 575-unit cell by design and therefore needs separate judgment.
The semicolon's lower comma descends to -131, versus -175 for the comma itself.
No corrections or alternate glyphs are promoted by this audit; these are visual
review candidates only.
Portsmouth Regular 0.023 and canonical family 0.107 carry these metrics.

## Principled punctuation and thought break 0.108

The subsequent user-directed punctuation pass preserves Portsmouth's high
epistemic consistency: no kerning, GPOS, GSUB, automatic `case` forms, or
font-owned double-hyphen conversion is introduced. Literal `--` remains two
hyphens. Host software may independently convert it to Unicode U+2014.

The semicolon's lower tomoe component is raised by exactly 34.7703 units. Its
new vertical bounds center, 40.7146, is the arithmetic midpoint between the
comma component at -23.4143 and the lower colon dot at 104.8434. The upper
semicolon dot is unchanged. The hyphen is raised by exactly 45.7213 units so
its center equals the plus crossbar center at 329.1487.

Curly quotation marks U+2018, U+2019, U+201C, and U+201D are added through the
same equal-third Annotation Mono, Comic Shanns, and Routed Gothic uncased
fusion route as the established punctuation. Unicode U+2014 is encoded
directly as Portsmouth's AI-specific thought break: a compact outlined thinking
cloud with two descending bubbles, placed above the midline with a slight
rightward bias and balanced upward toward capital height. It communicates a
rupture in the continuum without assigning truth, falsity, or importance.
There is deliberately no conventional em-dash outline and no ligature lookup.

### Enlarged thought break 0.109

Visual review found the first U+2014 cloud too annotation-like within its
advance. Version 0.109 expands it to approximately twice the visible ink area.
The horizontal transform grows from 0.76 to 0.91, producing roughly 655–660
units of visible width and therefore filling an em-dash-sized advance once the
minimum sidebearing reserve is included. The vertical transform grows from
0.71 to 1.0 and is repositioned between approximately 180 and 686 units. Its
three components, one counter, upward balance, rightward bias, direct-Unicode
mapping, and featureless input policy remain unchanged.

## Base-preserving diacritics and Eastern Latin expansion 0.110

Visual review identified `ą`, `ģ`, and `ų` as local failures and exposed a
systemic bold-weight inconsistency. Detached accents had previously forced the
topology limiter to reduce the inflation radius of an entire glyph: for example,
plain `A` received the requested 22-unit Bold radius while `Ă` received only
9 units. Version 0.110 weights the accepted letter body and each detached mark
independently. Bold marks move outward only far enough to preserve an eight-unit
separation; Regular and Light retain their original placement. Attached ogoneks
are likewise weighted separately from their accepted base bodies.

`ą` and `ų` are reconstructed from the exact accepted `a` and `u`, plus clipped
and inward-tucked ogoneks, keeping their overall ink widths equal to the base
letters for Mono. Lowercase Latvian `ģ` now uses the exact accepted `g` plus a
rotated comma above; the former eight-unit horizontal sliver beneath its
descender is removed. Long s `ſ` was reviewed and explicitly retained.

The parent cmaps contain 28 further unbuilt Latin characters in at least three
source files; 24 retain at least three contributors after project case routing.
This release promotes twenty Latin Extended-A forms useful across Polish,
Czech/Slovak, Latvian, Croatian/Slovenian, and adjacent orthographies:
`ć Ĉ ĉ č ď ě Ĝ ĝ ĥ ķ ń ň ŕ ř ś š ť ů ź ž`. Each uses an accepted Portsmouth
base plus a reusable fused mark. Side carons in `ď` and `ť` use Portsmouth's
right-curly apostrophe as a clean apostrophe-like caron, scaled and placed from
the parent-font median rather than inheriting a forked generic caron. The acute
accents on `ń`, `ŕ`, and `ś` use parent-derived vertical gaps. Latvian `ķ` uses
the established comma-below construction. Every promoted character has at
least three raw parent sources and at least two contributors after routing.

The separate non-Makima build is republished as Portsmouth Regular 1.0 with
an independent internal family identity: family and typographic family
`Portsmouth Regular`, style `Regular`, and PostScript name
`PortsmouthRegular-Regular`. This prevents font managers from grouping or
aliasing it as the Regular face of the Makima-derived `Portsmouth` family.

Portsmouth Mono is republished alongside release 1.0 with every
family namespace set independently to `Portsmouth Mono` and the distinct
PostScript identity `PortsmouthMono-Regular`. It remains style `Regular` within
its own family. It is not contained in the Portsmouth TTC, which contains
exactly the five proportional faces. The release ZIP includes the standalone
Mono TTF and the separately named non-Makima Portsmouth Regular for convenient
distribution, but neither is embedded in the TTC. Mono is also emitted under
`build/mono` with its own standalone ZIP.

The original eleven shared conflicts are retained source-by-source in
`specimens/latin1-topology-conflicts-predecision.png`; the then-unresolved set
is in `specimens/latin1-topology-conflicts.png`. The proposed resolution was modal
topology with equal geometric influence inside that topology: retain the
closed `@` counter supported by two of three sources; use detached marks for
`à á â ã å è é ê ñ õ`, supported by three of four sources. Bedstead should
still influence each mark's position, angle, and mass, but its Teletext join
should not force a one-source topology on the other three. The detached-mark
decision was approved and implemented at 10,240 points per contour. At that
stage `@` remained undecided; version 0.012 subsequently resolved it.

User review of the first TTF identified four priority redraws: lowercase `a`,
lowercase `f`, uppercase `K`, and lowercase `q`. Version 0.010 remains the
baseline showing those failures honestly. The other glyphs visible in that
font-wide specimen were accepted for this stage; this does not automatically
clear extended characters that were not present in the sheet. The direct
source/output comparison is `specimens/redraw-priorities-afKq.png`.

The first targeted controls are retained but not silently promoted.
Counter-centered radial transport was rejected for `a` because it collapsed
the upper aperture, and for `q` because it converted the descender into a
stepped wedge; neither letter is star-shaped around its counter. Removing the
medial assignment alone did not repair `f`. Boundary-self-anchoring modestly
cleaned the `K` branch junction, so it was retained as a control. On a later
rendered review, the user accepted the current Portsmouth `K` as visually fine;
its baseline geometry remains in place and the piecewise arm/junction candidate
is not promoted. Of the original four priorities, `f` is promoted, `K` is
closed without replacement, and the later construction-family route resolves
`a q` without using the rejected event controls.

## Decision log

- 2026-08-02: Initially identified seven unique fonts.
- 2026-08-02: Selected a static, equal-weight midpoint and exact shared cmap.
- 2026-08-02: Excluded Xenia before geometric work; fixed the source set at six.
- 2026-08-02: Selected Syne Mono rather than Syne Regular; fixed the source set
  at five.
- 2026-08-02: Routed Syne Mono only to uppercase letters and Bedstead only to
  lowercase letters; the other three sources remain active throughout.
- 2026-08-02: Reintroduced Xenia as an all-glyph contributor. Portsmouth is
  designated for personal and artistic use with complete source attribution.
- 2026-08-02: Excluded Xenia again after direct comparison showed it was too
  similar to Annotation Mono. Annotation Mono remains; Xenia contributes no
  geometry, routing weight, coverage constraint, or output metadata.
- 2026-08-02: Completed the first raster-to-TrueType engine prototype. Deferred
  the uppercase `M` central-junction treatment for explicit design review.
- 2026-08-02: Rejected filled-SDF averaging after it erased Bedstead's
  structural contribution to lowercase `m`. Selected an Eikonal shock-graph
  plus radius representation for the replacement engine.
- 2026-08-02: Rejected outline-scale ingestion for Bedstead. Its adapter must
  preserve native Teletext rendering behavior, including the perceptually
  lower second half of lowercase `m`, before continuous Eikonal lifting. A
  scale study established that the effect comes from unequal shoulder mass and
  a lower terminal/join, not unequal outer peak heights.
- 2026-08-02: Completed lowercase `m` fusion experiment 01. Stem registration
  succeeded; columnwise ordered-boundary reconstruction was rejected in favor
  of continuous centerline/radius envelope reconstruction.
- 2026-08-02: Chose vertical-landmark normalization while preserving source
  horizontal proportions.
- 2026-08-02: Reserved topology conflicts for visual examination; lowercase
  `m` is the first case.
- 2026-08-02: Added per-letter maximum-ink-height normalization about the
  baseline. Too-short and too-tall source glyphs are both scaled to the median
  active-source top before transport.
- 2026-08-02: Replaced sparse-disk output with a 2,048-atom-per-source dense
  fingerprint experiment. The median skeleton is an attractor only; annealed
  density generates a new skeleton and Eikonal radius envelope.
- 2026-08-02: Rejected dense-density growth and skeleton-envelope output after
  its left junction produced an optical gap and a two-leg reading. Numerical
  connectivity is not an acceptable substitute for a traced letter.
- 2026-08-02: Selected ordered boundary point clouds as the active pathway.
  Four 1,024-point source contours are monotonically transported, equally
  averaged, annealed, and traced directly; the skeleton guides correspondence
  but does not generate the filled glyph.
- 2026-08-02: Increased direct-trace drawing density from 1,024 to 10,240
  points per source and source-union extraction from 0.5 to 5 samples per font
  unit. Completed the first twelve single-contour lowercase studies; reserved
  `r` and `w` for explicit construction review.
- 2026-08-02: Extended the active method to multi-contour unity. Persistent
  outer/counter loops are role-paired and cyclically phase-synchronized before
  equal-weight tracing. Completed `a b d e g o p q`; reserved `a` for aperture
  transition review.
- 2026-08-02: Added component-wise connected medial attractors for detached
  bodies and completed `i j`. Both retain two components; reserved `j` for
  shoulder and tittle-proportion review.
- 2026-08-02: Completed the first uppercase multi-contour batch `A B D O P Q R`
  through the Syne Mono route. Reserved `Q`: its topology is correct, but its
  tail/counter relation demonstrates the need for coupled contour events.
- 2026-08-02: Tested and rejected continuous outer/counter clearance coupling
  on `Q`; it did not materially repair the optical notch. The next experiment
  must represent the tail intrusion as an optional discrete boundary event.
- 2026-08-02: Completed full experimental geometry for all 52 ASCII letters.
  Archived every loop at 10,240 points in `geometry/`, added a validated
  machine-readable manifest, and reserved `a f j r w G J M Q W` for explicit
  design review.
- 2026-08-02: Added containment-based counter ownership for mixed component and
  counter complexes, plus a one-atom attractor fallback for sub-pixel-thin
  marks. Archived 47 topology-compatible Latin-1 letters, bringing Portsmouth
  to 99 full-resolution letters; reserved `ä` and `æ` for design review and 22
  topology-conflicting shared letters for later explicit decisions.
- 2026-08-02: Archived the remaining 38 topology-compatible Latin Extended
  letters. Portsmouth now contains full-resolution geometry for 137 of 159
  shared letters, with no compatible letter missing; the remaining 22 are
  documented source-topology conflicts. Reserved `ā Ġ Ŵ ŵ` by inheritance
  from unresolved base-letter constructions.
- 2026-08-02: Expanded the archive through U+00FF. Of 167 shared encoded
  characters, 155 now have full-resolution geometry, U+0020 is spacing-only,
  and eleven remain explicit topology conflicts; no compatible character is
  missing.
- 2026-08-02: Constructed Portsmouth `0` from an equal-weight fused ring,
  counter, and solidus. Shortened both slash ends to terminate visibly inside
  the ring after a rendered specimen exposed that the first intended clipping
  constraint had failed visually.
- 2026-08-02: Made `1/i/I/l/|` disambiguation a regression constraint: `1`
  uses a flag and broad foot, `i` a detached dot, uppercase `I` centered
  bilateral bars, lowercase `l` the accepted organic stem with both terminal
  bars completely shaved away, and `|` a distinctly taller pipe.
- 2026-08-02: Built the first font-wide installable artifact,
  `Portsmouth-Regular-0.010.ttf`. It contains 155 archived outlines plus space
  and no-break space, uses routed equal-weight advances with safe
  sidebearings, and passes fontTools, HarfBuzz, Fontconfig, and rendered visual
  checks.
- 2026-08-02: Rendered all eleven shared Latin-1 topology conflicts for direct
  comparison. Proposed, but did not yet commit, the modal closed-counter `@`
  and detached-mark accented lowercase topology while preserving minority
  geometry inside those selected structures.
- 2026-08-02: User approved detached modal topology for Bedstead's conflicting
  lowercase accents. Built `à á â ã å è é ê ñ õ` as equal-weight detached
  compounds, retaining Bedstead in mark geometry while preventing its joined
  Teletext topology from overruling the other three sources.
- 2026-08-02: User review of the first TTF selected `a f K q` as the four
  priority redraws. Their archived geometry remains available as the baseline
  and is explicitly marked `needs_design_review`.
- 2026-08-02: Packaged the ten approved accent compounds as Portsmouth 0.011.
  It encodes 167 of 191 ISO-8859-1 graphic positions; `@` is the sole remaining
  shared topology decision, while 23 positions require new Portsmouth designs
  because at least one required always-on source lacks them.
- 2026-08-02: Resolved `@` using equal Annotation/Routed closed-counter
  structure constrained to Comic Shanns' overall and spacing ratios. Packaged
  the result as Portsmouth 0.012; all 167 shared encoded Latin-1 characters
  are now represented by 166 outlines plus space.
- 2026-08-02: Rejected counter-radial `a` and `q` controls after rendered
  inspection exposed aperture collapse and a descender wedge. Retained a
  boundary-self-anchored `K` only as a promising control.
- 2026-08-02: Rebuilt lowercase `f` with six explicit contour events and a
  modal compact terminal constraint. Promoted the visibly cleaner result and
  packaged it as Portsmouth 0.013 while retaining 0.012 and the pre-event
  geometry as reproducible baselines.
- 2026-08-02: Re-reviewed uppercase `K` in context and accepted its current
  Portsmouth geometry. The arm/junction event experiment remains unpromoted;
  no `K` geometry in 0.013 was replaced.
- 2026-08-02: Replaced lowercase `a` and `q` with equal-third
  Comic-Shanns/Syne-Mono/Routed-Gothic construction-family fusions. Archived
  both previous glyphs and rebased the `a` accent family.
- 2026-08-02: Expanded ISO-8859-1 with thirteen rendered, topology-compatible
  majority-source glyphs. Rejected `ß` for a false counter and `ð` for a
  malformed crossbar/bowl junction.
- 2026-08-02: Rebuilt `æ` as an explicit union of accepted Portsmouth `a` and
  `e`, retaining two counters at a controlled 145-unit half-separation, and
  packaged the result as Portsmouth 0.015.
- 2026-08-02: Built the first quadratic fitting control as the isolated family
  Portsmouth Quadratic Test under `build/experimental`; the Portsmouth Regular
  polygonal master and all historical releases remain separate.
- 2026-08-02: Repaired only the flagged terminals of capital `J` and numeral
  `1`, selecting the restrained `J` bar and symmetric 360-unit `1` foot while
  retaining both pre-repair geometries.
- 2026-08-02: Completed the lower web-symbol range through U+00BF with
  family-derived `© ª` soft hyphen, `® ¶ º ½ ¾`. Packaged Portsmouth Regular
  0.016 with 189 of 191 ISO-8859-1 graphic positions; only rejected `ß` and
  `ð` remain absent. The isolated quadratic experiment was left untouched.
- 2026-08-02: Corrected the capital `J` top-bar return from 116 to 86 units so
  it meets the accepted stem without a width step; packaged the localized
  correction as Portsmouth Regular 0.017 while retaining 0.016.
- 2026-08-02: Built isolated Makima quadratic q2 from Regular 0.017. Added
  cyclic arc-length Makima masters, structural corner events, one-sided hard
  tangents, tangent-preserving cubic-to-quadratic conversion, and adaptive
  subdivision. All 189 encoded glyphs pass raster-topology comparison; U+00BF
  remains an explicit protected-polygon fallback rather than accepting a
  topology-changing fit.
- 2026-08-02: Promoted the Makima true-shape method into canonical Portsmouth
  family 0.100: `Portsmouth.ttf`, Italic, Bold, Bold Italic, and a separate
  575-unit Portsmouth Mono. Italic uses a nonlinear baseline-anchored wind
  field; Bold uses topology-constrained Eikonal radius inflation; Mono uses a
  column-importance compression map and never stretches narrow ink.
- 2026-08-02: Reconstructed canonical U+00BF by rotating the accepted
  Portsmouth `?` through 180 degrees, eliminating the inherited pinched-neck
  fracture. Audited all five faces at 189 characters with no topology
  mismatches, bundled them as a five-face TTC and ZIP, and retained polygonal
  Portsmouth Regular 0.017 unchanged as the historical non-Makima master.
- 2026-08-02: Promoted 38 completed, all-source-cmap Latin Extended outlines
  into polygonal Portsmouth Regular 0.018 and every canonical Makima family
  face. Coverage increased from 189 to 227 encoded characters while rejected
  Latin-1 `ß` and `ð` remained absent.
- 2026-08-02: Increased the nonlinear italic wind from 10 to 13 degrees. The
  choice splits the installed-font median of 12 degrees and the CSS Fonts
  Level 4 omitted-angle oblique convention of 14 degrees; contour geometry,
  caret slope, and font metadata now agree exactly.
- 2026-08-02: Audited the expanded 0.101 family across 227 characters and
  produced a matched-cap-height comparison of all five parent fonts against
  Portsmouth. Preserved 575-unit Mono metrics and the five-face TTC/ZIP
  packaging.
- 2026-08-02: Added Portsmouth Light as a weight-class-300 face. Calibrated an
  18.5-unit topology-constrained Eikonal erosion against Courier New Regular:
  matched-cap median stems measure 53 versus Courier's 50 pixels, down from
  Portsmouth Regular's 87. Audited all 227 glyphs for reduced ink and preserved
  topology, then expanded the canonical collection and bundle to six faces.
- 2026-08-02: Shortened lowercase `r` with a shoulder-only 0.90 horizontal
  scale. Preserved the stem and sidebearings, reduced ink and advance by 27.86
  units, and propagated the corrected master through all six faces.
- 2026-08-02: Promoted 78 majority-supported Latin Extended additions. Reused
  accepted Portsmouth bases for 62 decomposable forms—52 detached marks and
  ten attached unities—and retained full routed fusion only for sixteen true
  structural letters. Coverage increased to 305 characters per face.
- 2026-08-02: Rendered all twenty raw extended topology conflicts. Established
  that case routing resolves `Ĉ Ĝ ķ` automatically and reserved the remaining
  seventeen for explicit design decisions.
- 2026-08-03: Finalized release 1.0 with the independently named heavy
  Portsmouth Regular, five-face proportional Portsmouth TTC, and independently
  installable Portsmouth Mono. Standardized 1.25-em typographic line metrics,
  resolved `1lI|`, and retained the 13-degree italic wind.
- 2026-08-03: Refined Portsmouth Mono as a terminal-specific fixed-pitch
  portfolio. All 330 encoded advances remain 575 units; every visible glyph is
  constrained to a 75-unit hard cell-edge bearing and optically centered only
  within that envelope. Exhaustive raster measurement of all 8,644 overlapping
  printable-ASCII pairs found a 149-unit minimum sustained gap and a 151-unit
  lowercase fifth percentile; `ab` measures 151 units.
- 2026-08-03: Leveled the lowercase `i` terminal, faired the four walls of
  lowercase `v`, and rebuilt every Portsmouth face from the repaired masters.
  Added a 60-unit geometric safety bearing to Italic and Bold Italic capitals,
  correcting the genuine `T` and `L` advance overlap.
- 2026-08-03: Shortened capital `L`'s lower projection by constraining its
  stem-center-to-tip span to the accepted `T` top bar's 267.22-unit half-span.
  The stem and left wall remain fixed; the lower right foot alone is compressed.
- 2026-08-03: Issued the independent five-face Portsmouth Rapids 1.0 print
  family. A local AppKit probe measured 13.75 units/em per tracking action;
  Rapids compiles the requested five-step tightening as a balanced 68-unit
  advance reduction and falls within one further step of Arial loosened once.
  Tabs remain application-controlled with a documented four-space policy.
- 2026-08-06: Added a protected interface-navigation set from the requirements
  in `/Users/quentinkuttenkuler/futurescope/GUI_FORMS_PORTSMOUTH_NAVIGATION_SYMBOLS.md`.
  The required `← ↑ → ▼` is accompanied by the coherently supported
  `↓ ↔ ↕ ↖ ↗ ↘ ↙ ▲ ▶ ◀`; all fourteen characters use ordinary Unicode cmap
  mappings in every Portsmouth, Portsmouth Mono, and Portsmouth Rapids face.
  Broad-parent single chevrons and the ubiquitous interface bullet, ellipsis,
  and mathematical minus complete the nineteen-character interface set.
  Equal-weight parent Eikonal fields establish arrow scale, topology, and
  optical center; a median straight skeleton with explicit boundary events
  resolves the incompatible curved, pixel, and stencil joins into one robust
  small-interface master. Cardinal, diagonal, and bidirectional relatives are
  protected transformations of that construction. Disclosure triangles use a
  compact authored master, with source support establishing their Unicode role.
  Arrows share a 650-unit proportional advance and triangles/chevrons a
  520-unit proportional advance before each family's established Mono or
  Rapids metric policy is applied. The bullet reuses Portsmouth's period shape
  at the five-source median diameter, the ellipsis is three accepted periods,
  and the mathematical minus retains the accepted hyphen construction at its
  five-source median scale and vertical center. Box drawing, check/radio state,
  and application icons stay renderer primitives; line controls
  U+0009/U+000A/U+000D remain unmapped.
