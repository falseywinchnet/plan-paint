# Icon and cursor size production

Open an ICO or CUR document and select **Atlas → Icon sizes…** or **Cursor sizes…**.
The current image is the source for generated sizes. The dialog identifies its
dimensions, lists every proposed image change, and warns when a requested image
would be larger than the source. Choose **Generate** to apply the reviewed plan,
or **Cancel** to leave the document unchanged.

Three modes control existing representations:

| Mode | Existing images | Missing selected sizes |
|---|---|---|
| Add missing sizes | Keep every existing image, including manually edited and same-size variants. This is the default. | Generate from the current image. |
| Replace chosen sizes | Regenerate every existing square image whose dimensions are selected. Keep all other images. | Generate from the current image. |
| Exact profile | Keep every image matching a selected square size; remove off-profile images. Same-size variants remain separate. | Generate from the current image. |

The review names existing images by their one-based image number and dimensions.
It includes additions, replacements and removals before any pixels change.
Unchecked sizes are removed only in Exact profile mode. An empty profile is
rejected. Repeating a target size in the underlying generation request does not
create extra copies.

Generated representations contain RGBA pixels and use Paint's existing image
resampler. Replacements flatten the current source artwork into each selected
target. This is not palette-index editing or a promise to retain an imported
encoding after replacement. Unchanged representations retain their pixel and
legacy cursor data in the document.

New cursor hotspots follow the source cursor's pixel centers, rounded to the
nearest target pixel and clamped inside the target image. Converting an ordinary
image or icon into a cursor starts new hotspots at the upper-left pixel. A source
with background-dependent legacy XOR pixels cannot be resampled; the operation
reports that limitation before changing the document. Such images can still be
kept or removed by an exact profile that requires no generation.

Generation is one document transaction. Undo restores all affected images and
the source selection; Redo restores the generated set. A request that adds,
replaces and removes nothing does not add an Undo step. The current image stays
selected if it survives the operation. If it is removed, the last output image
becomes current.

The broader [icon production interview](icon-editor-study/mockups.html) covers
future palette, alpha, preview, batch and asset-browsing capabilities. Its
recommendations remain candidate designs; this size-production feature does not
establish answers to the remaining interview questions.
