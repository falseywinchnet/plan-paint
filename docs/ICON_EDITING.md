# Icon and cursor production

## Representation formats and preservation

The Atlas strip identifies each image by its ordinal, dimensions and imported
format, such as **1 bpp DIB** or **2 bpp PNG**. Same-size images remain separate
representations. The strip status distinguishes **Source** from **Save**: Source
describes the imported encoding; Save describes the encoding that the current
image would use if saved now. Newly generated images show RGBA as their source.
Painting always edits RGBA pixels; these labels do not imply palette-index editing.

An untouched imported representation retains its original standard PNG or DIB
payload when saved to ICO or CUR. Editing a different representation does not
convert it. A cursor hotspot-only change updates the directory coordinates while
preserving the image payload. The container directory is rebuilt, so this is
payload preservation rather than a whole-file byte-identity guarantee.

Changing pixels, hidden RGB under transparent pixels, dimensions or legacy XOR
data causes that representation to be encoded from its current pixels. Edited
and generated images use 32-bit DIB, except 256-by-256 ICO images without legacy
XOR data, which use PNG. Other representations keep their imported payloads.
Undoing the edit restores original-payload eligibility when the complete image
and XOR data match again. Source describes the import in the current session;
reopening a saved file reads the formats actually written to that file.

## Generating sizes

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
rejected. A plan exceeding the supported 256 representations is rejected before
the document changes. Repeating a target size in the underlying generation
request does not create extra copies.

Generated representations contain RGBA pixels and use Paint's existing image
resampler. Replacements flatten the current source artwork into each selected
target. This is not palette-index editing or a promise to retain an imported
encoding after replacement. Unchanged representations retain their pixels,
legacy cursor data and original imported payloads.

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

## Comparison and contact sheets

Open an ICO or CUR and choose **Atlas → Compare…**. The owned comparison window
uses a snapshot of the document at opening. Choose all representations, the
current image, or the sequence selected in Atlas. Sequence order and repeated
entries are retained; an empty sequence reports an error. Labels retain the
original one-based image numbers, dimensions and imported format. Newly generated
images are labeled RGBA. Imported format describes source provenance, as in Atlas.

Choose a light, dark, checkerboard, transparent or primary-color background.
The primary color is captured when the window opens and used as an opaque
background. Columns, padding, spacing, label visibility and text size determine
the generated sheet. Images keep their original pixel dimensions, centered in
uniform cells; the sheet renderer does not resize the artwork. Layout is checked
before allocation and rejects outputs larger than 16 megapixels.

**Fit sheet** fits the result into the preview area. **Actual pixels** maps one
image pixel to one physical display pixel, accounting for the host's display
scale. **4× pixels** maps each image pixel to four display pixels. Enlarged
previews use nearest-pixel sampling; scroll when the sheet exceeds the viewport.
The status reports both output dimensions and the display-to-image pixel ratio.

**Export PNG…** saves the prepared sheet through the normal save dialog. Background
and labels become part of that PNG; Transparent preserves alpha outside the
artwork and labels. Legacy background-dependent XOR pixels are evaluated against
the chosen opaque background. Transparent output is unavailable for those images
because ordinary PNG alpha cannot represent a background-dependent XOR operation.

Changing settings prepares a new preview and disables export until it is ready.
Closing cancels pending work. Preview settings and exported sheets do not change
the source document, its filename or Undo history. This command exports one
assembled sheet; separate-file representation export and multi-file batch jobs
remain future work.

The broader [icon production interview](icon-editor-study/mockups.html) covers
future palette, alpha, batch and asset-browsing capabilities. Its recommendations
remain candidate designs; implemented features do not establish owner answers
to the remaining interview questions.
