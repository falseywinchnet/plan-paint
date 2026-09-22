# Spirograph

Open **Path / graph dropdown → Spirograph**. The green support and blue insert
are temporary translucent apparatus above the canvas. Their parts never enter
an image export.

## Pegs and media

1. Choose a guide and an insert. The menus beneath their trays open the full
   illustrated catalogs; their captions identify the current parts.
2. Drag a Fine (1 px), Medium (2 px), or Bold (4 px) peg into an empty socket.
3. Choose **Fill pegs**. Click the peg for Primary ink or right-click for Alt.
   No Color empties it. Empty sockets cannot hold ink.
4. Choose **Operate**, then click a seated peg. Its gold outline identifies it
   as the target for **Peg medium** and the ordinary **Materials** gallery.
5. Choose a medium. Each peg keeps its own ink, width, material settings, and
   miniature stroke preview in its cap. Brush assignment does not draw.
6. Drag the insert's center grip. Every loaded peg draws with its own medium.

All thirteen additive media are available, including Gel pen, Watercolor,
Marker, Natural pencil, Crayon, Oil, and Airbrush. These use the same stroke
renderers as freehand brushes. Pixel-mixing and cloning tools are not ink media.
Gel pen has a solid body, gently varying brightness, and sparse glints. Its
material texture remains fixed during a stroke; extra mouse events do not add
extra coats. Grain scale changes the variation scale and Paper tooth changes
the amount of sheen. Paint load zero deposits nothing.

A click selects a peg; moving more than four screen pixels starts a peg drag.
Drop it in another socket to move it, or outside the insert to remove it. An
invalid drop within the insert restores the original socket. Escape cancels a
move. **Deselect peg** returns material choices to the ordinary drawing tool;
clicking away from the apparatus also clears peg selection.

Drag the green support to reposition the assembly without drawing. **Center
guide** returns it to the image center. Removing or replacing an insert clears
its pegs, materials, and selection. The raised red close button removes the
apparatus while keeping the drawing. Each continuous operation is one Undo
step, including all loaded pegs. Drawing respects the active canvas selection,
regular guide constraints, and rotated working view.

## Component catalog

The catalog includes 16 guides and 23 inserts:

- Rings: 96, 120, 144, 180, 210, and 240 teeth.
- Shaped guides: oval, long oval, rounded triangle, square, pentagon, hexagon,
  egg, and shield.
- Racks: standard and long straight supports with travel limits.
- Circular inserts: 12, 18, 24, 30, 31, 36, 37, 40, 45, 48, 56, 60, 64, 72,
  and 80 teeth; the prime counts support longer repeating patterns.
- Shaped inserts: oval, long oval, rounded triangle, square, pentagon, hexagon,
  egg, and shield. Each has its own socket arrangement.

**Roll outside** places the insert against the outside of a closed guide,
producing a different family of curves. Racks roll along a straight segment.
Inside combinations are conservatively checked for curvature clearance;
incompatible parts are disabled. Returning from outside to inside requires a
compatible pair. Changing the guide preserves its approximate displayed size
by rescaling the entire apparatus, including the insert.

## Contact model

Each closed pitch curve uses a support function
`h(n) = 1 + sum(a[k] cos(k*n))`, with positive curvature radius `h + h''`.
Its boundary is `h*n + h'*t`; its arc length has an analytic integral. An
invertible arc map matches the distances traveled on the guide and insert,
then aligns their contact normals. This determines both translation and
rotation, including asymmetric shapes. Circles use the exact familiar radius
ratio as a fast path. Racks use the same arc matching against a straight line.

Integer tooth counts fix the perimeter ratio, so closed-guide patterns return
to their initial pose after `insert_teeth / gcd(guide_teeth, insert_teeth)`
guide circuits. The tooth marks visualize the pitch geometry; there is no
rigid-body collision simulation or accumulating integration slip. These are
original smooth pitch profiles, not reproductions of commercial molded parts.

Pen subdivision is bounded to half an image pixel by a curvature-dependent
speed bound. Per-peg stroke state preserves continuous deposition across mouse
events. Sparse transparent coats are composited in socket order, with higher
numbered sockets above lower ones at crossings. One scratch bitmap is shared
among all pegs; the temporary coats and bitmap are released after each drag. The nearest center-path position is projected near the previous phase;
this avoids substituting a circular orbit for a noncircular piece.

## Geometry references

The physical kits informed the range of mechanisms, not the code or drawings:

- [Wild Gears Compact set](https://www.wildgears.com/compact-gear-set.html):
  circular and noncircular gears, varied socket sizes, and component reuse.
- [Wild Gears Oval set](https://www.wildgears.com/oval-gear-set.html):
  oval inserts and frames with different symmetries.
- [Wild Gears Modular Oblong set](https://shop.wildgears.com/products/modular-oblong-gear-set):
  racks and oblong supports.
- [PlayMonster Art Studio instructions](https://playmonster.com/wp-content/uploads/2021/10/artstudio-instructions.pdf):
  rack use and outside rolling.

Nested independently moving inserts, concave tracks, and modular track assembly
remain possible extensions requiring additional motion constraints.
