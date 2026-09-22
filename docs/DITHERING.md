# Dithering and the dithering brush

Select artwork, open the Selection arrow, and choose **Dither / posterize…**.
Without an active canvas selection the command processes the canvas. A pasted
floating object remains above the canvas and does not restrict this operation.

Choose 2–32 colors and Crosswind, Weave, Scrambled, Drift, or Posterize. The
background job prepares the exact result shown by the preview. OK applies that
prepared image as one undoable edit; Cancel preserves the document. Changing an
option cancels the preceding job. The active selection, its holes, and alpha are
preserved; unselected pixels and fully transparent pixels are left byte-identical.

- Crosswind distributes quantization error to future selected neighbors with
  deterministic varied weights and less transport across source-color changes.
- Weave uses a regular Walsh threshold lattice.
- Scrambled varies local Walsh phases at multiple scales.
- Drift changes the finer weave allocation over larger regions.
- Posterize chooses the nearest palette color without a spatial pattern.

The automatic palette uses OKLab distance and a Student-t similarity with two
degrees of freedom. Sampled local color entropy and global color-family rarity
protect less common families. Candidate colors come from the selected source.
Linear RGB determines nonnegative mixtures of at most four accepted colors;
Walsh ranks select one whole color at each pixel. Each complete constant-mixture
64×64 tile has balanced ranks, with palette counts within one pixel of the
requested proportions. This is a practical allocator, not an optimal palette or
a promise that every photograph improves perceptually at every color count.

Native processing samples at most 4096 selected visible pixels, builds at most
192 candidate colors, uses 5×5 local entropy neighborhoods and fixed OKLab bins,
and greedily consolidates the candidate set. This bounded runtime allocator is a
native adaptation of the research experiments, not their larger offline proposal
pool. Palette geometry uses small active convex hulls; the repeated-RGB mixture
cache is bounded at 32768 entries. Crosswind uses two error rows. Cancellation is
checked during sample collection, palette construction, and row processing.

## Dithering brush

Open the Brushes arrow and choose **Dithering brush**. Its size uses the normal
size controls. The Tool ribbon's **Brightness / saturation noise** switch selects
between two operations:

- Switch off: Neighborhood. A sampled pixel moves halfway toward the alpha-weighted
  linear-RGB mean of its selected 5×5 neighborhood.
- Switch on: Noise. A sampled pixel receives up to ±0.025 OKLab lightness and
  ±10% chroma scaling. Gamut clipping can limit the adjustment.

Both operations act on existing pixels and preserve each pixel's alpha. Primary
and Alt ink do not supply replacement colors. This is not an additive peg medium.

The spray probability is 0.28 × (1−r²)² inside the normalized brush disk and zero
outside it. It is inspired by the requested center-heavy disk behavior; it is not
a uniform hyperbolic-area distribution. Dabs follow traveled distance, with one
initial dab per gesture, so extra stationary mouse events do not build up noise.
Each dab reads all its neighborhoods before applying changes. Selection holes
cannot supply neighborhood colors. Normal guide constraints, rotated-view inverse
coordinates, and atlas wrapping continue to apply. A stroke is one Undo step.
