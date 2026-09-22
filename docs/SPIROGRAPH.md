# Spirograph

Open the **Path / graph icon dropdown → Spirograph**. A translucent green ring
and blue wheel appear above the canvas. The Spirograph tab contains separate
**Guides**, **Inserts**, and **Pegs** trays.

1. Choose a ring and wheel. The first set includes 96- and 120-tooth rings,
   and 36-, 40-, and 48-tooth wheels.
2. Drag a Fine, Medium, or Bold peg from its tray onto an empty wheel socket.
   A gold outline identifies the socket that will accept the peg.
3. Choose **Fill pegs**, then click a seated peg to load Primary ink. Right-click
   loads Alt ink. The cap shows the ink color. An empty socket cannot hold ink.
4. Choose **Operate** and drag the wheel's center grip around the ring. Every
   inked peg traces its own curve. Empty pegs make no marks. The center grip can
   also operate the wheel while Fill pegs is selected.
5. Drag the green support to reposition the assembly without drawing. The wheel
   and its pegs travel with it. **Center guide** returns it to the image center.

Peg widths are 1, 2, and 4 image pixels. To move a peg, use Operate and drag its
cap to another socket. Release it outside the wheel to remove it and its ink.
An unsuccessful drop inside the wheel returns it to its original socket.
Escape cancels a peg move.

**Remove insert** clears the wheel and its pegs. Choosing a replacement wheel
starts with empty sockets. The red raised close button removes the whole
apparatus. These operations leave previously drawn pixels alone.

The apparatus is temporary editor state. It is not a floating image, is not
saved into an export, and creates no image history entries. Each continuous
wheel operation with loaded pegs is one undoable drawing gesture. Drawing obeys
the active canvas selection and existing guide constraints. The working view
can be rotated; the apparatus and pointer interactions use original image
coordinates.

## Mechanism and extension

Circular inserts roll inside circular rings. Integer tooth counts define the
radius ratio, and the wheel's orientation is derived from its orbital angle.
The mechanism projects pointer motion onto that constraint; there is no
accumulating numerical slip. A drag close to the guide center holds the last
angle, where the pointer's direction would otherwise be ambiguous.

The rendered teeth, bevels, translucent bodies, sockets, and peg caps are
editor overlays. Their appearance does not enter the rolling calculation.
Stroke subdivision limits each pen step to half an image pixel before drawing
with the existing raster tools. Multiple loaded pegs follow the same motion.
Only the changed drawing bounds are republished during operation.

The component catalog separates guide definitions, insert definitions and hole
locations, and peg width/ink state. More components can extend those catalogs.
Rods, noncircular rolling boundaries, and external wheel contact need their own
validated motion constraints; they are not included in this first mechanism.
