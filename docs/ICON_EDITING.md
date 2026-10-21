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
assembled sheet. Separate-file representation export is described below;
multi-file batch jobs remain future work.

## Exporting separate PNG files

Open an ICO or CUR and choose **Atlas → Export…**. Choose all representations,
the current image, or the selected Atlas sequence. Enter a filename prefix and
choose an existing destination folder. The window takes a document snapshot when
opened and prepares the complete output manifest before enabling **Export PNG…**.
Review every name and status before running. Merely reviewing creates no files.

Each representation becomes a separate RGBA PNG at its original dimensions.
No resampling or palette conversion is requested. Names include dimensions,
imported source depth when available, and the original one-based image number:
`sample-32x32-src8bpp-003.png`. Here `src8bpp` describes imported provenance;
the exported PNG contains current RGBA pixels. New representations use `rgba`.
Same-size variants remain distinct. Repeated sequence entries get an additional
copy suffix. PNG does not carry cursor hotspots or background-dependent XOR
operations; XOR images produce a preflight error. An opaque contact sheet can
show their appearance against a chosen background.

Choose how to handle existing output names:

| Policy | Reviewed behavior |
|---|---|
| Skip existing | Default. Existing destinations are listed as skipped and remain unchanged. |
| Rename outputs | Resolve numeric suffixes during review. If a reviewed name becomes occupied before publication, report failure and leave that file unchanged; review again for a new name. |
| Replace existing | Explicitly permit replacement of regular files. Replacements are marked in the manifest. Directories and symbolic links are not valid replacement targets. |

Skip and Rename use an atomic publication step that cannot overwrite an existing
destination, including a file created after review. Each completed PNG is first
written and closed as a temporary sibling, then published. An output failure
preserves the earlier destination and is reported individually; other outputs
can still complete. Filesystems that cannot support the required publication
operation report failure rather than falling back to a non-atomic overwrite.

**Cancel** stops further outputs and keeps the results window open. A file already
being published may finish; completed files remain on disk and in the report.
Closing the window also cancels pending work and waits for the worker to stop.
There is no whole-job rollback. **Save report…** exports JSON with each source
ordinal, dimensions, source depth, destination name, state and diagnostic.
**Review outputs** prepares a fresh plan after a job or filesystem change.
Document pixels, filename and Undo history remain unchanged.

This command exports the representations that actually exist in one open
document. Requested-size fallback, choosing one depth per distinct size, and
multi-document batch conversion remain separate work.

## Browsing source artwork and stamps

Choose **File → Browse assets…** from any document. The browser uses ordinary
folders; it neither imports copies into a private library nor modifies files
while browsing. Enter a folder and filename search, then press **Search /
refresh**, or use **Choose folder…**, **Up**, and folder activation. Search
matches a substring of the current folder's filenames with ASCII case folding;
other UTF-8 characters match literally. Type filters distinguish ICO, CUR,
bitmap images and SVG. Folders remain available through type filters.

The list includes PNG, JPEG, BMP, TGA, TIFF, WebP, AVIF, HEIF, SVG/SVGZ, ICO and
CUR extensions. Import support and limits are the same as opening these files
normally, including platform-dependent HEIF support and the restricted TIFF
profile. Corrupt or unsupported image contents produce a visible error.
GIF, PSD, HDR and portable-map formats are not advertised as supported inputs.
Each scan is confined to one folder and stops after 8,192 matching entries or
50,000 inspected entries. A limited listing is explicitly labeled; narrow the
search or choose a subfolder. Results are sorted folders first, then by name.
An inaccessible or missing folder is an error, not an empty result.

Select a file to decode a preview in the background. ICO/CUR files retain all
representations, including duplicate dimensions, and the representation chooser
identifies each ordinal, dimensions and imported depth. The largest image is
selected initially. The preview fits inside its well using nearest sampling and
does not enlarge small images. It composites alpha over a checkerboard and
evaluates legacy XOR against that background without modifying source pixels.
Cursor hotspots appear in the details. Preview loading initially refuses files
larger than 32 MB; larger images can still be opened through the ordinary Open
command, subject to the normal decoder limits.

**Open** opens the complete image or icon container and honors the existing
unsaved-document prompt. **Use as stamp** loads the chosen representation into
the existing Stamp tool at its original dimensions and alpha. Loading the stamp
does not itself change canvas pixels or create an Undo entry; placement uses the
normal painting transaction and Undo. Background-dependent XOR images cannot
be used as ordinary alpha stamps, and that action is visibly unavailable.
**Copy path** copies the selected file or folder path.

Size and modification-time checks before/after decoding and before using a
preview detect ordinary external changes. A changed or missing file requires a
fresh preview. These checks are not a cryptographic guarantee against replacing
content while preserving both metadata values. Navigation keeps only the latest
pending request; completed work carries a generation that must still match the
window. Closing revokes queued results and waits for the active decode to finish.

**Bookmark folder** and **Remove bookmark** maintain explicit bookmarks in
`asset-folders.txt` beside the application preferences. The open UTF-8 format
starts with `Plan Paint asset folders v1` followed by a newline, then one absolute
path per line. Percent signs and control bytes are escaped as uppercase `%HH`.
Missing folders remain in the bookmark list. There are at most 64 bookmarks;
failed or malformed storage is reported and is not silently replaced.

This browser currently provides a filename list and a selected-image preview.
Thumbnail grids, multiple-asset job selection, metadata indexing and file-manager
integration remain part of the broader production work.

## Inspecting and transferring opacity

For an ICO or CUR document, choose **Atlas → Alpha tools…**. Like other Atlas
commands, opening the tool settles the current painting gesture and editable
geometry. The tool operates on the **entire current representation**, even if a
selection outline is present. Other sizes and depths are not targets.

The owned window shows the original and proposed result. **Alpha** displays
opacity as opaque grayscale: black is transparent, white is opaque and gray is
partial opacity. **Color** composites the actual RGBA image over a checkerboard.
Both previews fit without enlarging small artwork, using nearest sampling.
Statistics show the minimum and maximum alpha and counts for alpha 0, 1–254,
and 255. Changing the view does not change pixels or create history.

**Copy alpha** copies the original representation's opacity as an ordinary,
opaque grayscale clipboard image. **Paste alpha** reads an image from the
clipboard and prepares a replacement opacity mask. The conversion is defined
as `(R + G + B + 1) / 3` using integer division on encoded 8-bit channels. The
mask's own alpha is ignored. Grayscale values therefore transfer exactly;
colored masks use a rounded arithmetic mean, not perceptual luminance.

Mismatched dimensions leave Apply unavailable until **Resize mask to image**
is explicitly enabled. Resizing samples nearest pixel centers independently
on each axis; it can change aspect ratio and does not interpolate. The original
and mask dimensions remain visible. Disabling resizing restores the mismatch
error without changing the document. **Reset** discards the staged mask.

**Apply** changes alpha only, retaining every RGB byte, including hidden colors
under fully transparent pixels. It creates one Undo entry and closes the window.
An identical mask leaves Apply disabled. **Close** or Escape discards the preview.
An intervening document or representation change rejects Apply and requires
reopening the tool. Cursor hotspots and other representations remain unchanged.

Legacy background-dependent XOR artwork cannot be treated as an ordinary alpha
mask; the command reports that limitation before opening the tool. Clipboard
geometry and host errors are validated before preparing a result. This workflow
adds no layer or persistent channel document. Painting directly into an alpha
mask and viewing alpha on the main canvas remain separate work.

## Indexed palette conversion

Open an ICO or CUR and choose **Atlas → Palette…**. The owned window compares
the current image with a 1-, 4-, or 8-bit indexed result. Set the alpha threshold
(1–255, initially 128), an RGB matte color (initially white), and either no
dithering or Floyd–Steinberg error diffusion. The preview reports palette entries,
transparent pixels, and the number of partially transparent source pixels that
will be converted. The displayed threshold rule identifies the resulting binary
alpha. Previews cover the entire current representation, including when a
selection outline is present.

**Apply** replaces that representation with the displayed indexed image in one
Undo step. Other representations, their encoded payloads, and cursor hotspots
remain intact. Saving the unchanged converted representation emits an indexed
DIB at the chosen depth, including for a 256 × 256 image. Subsequent painting
invalidates that encoded snapshot and saves the edited image as RGBA; conversion
can be run again when the artwork is ready. This is explicit pixel conversion,
not an index-aware painting mode or a permanently linked export recipe.

Changing preview settings does not alter the document or its Undo history.
**Close** and Escape discard the preview. Invalid matte text disables Apply.
A document change after opening the tool prevents applying stale pixels. A
conversion with the same encoded payload leaves Apply disabled. The tool rejects
background-dependent legacy XOR pixels rather than silently flattening them.

The independently specified conversion is:

1. Source pixels with alpha below the threshold become transparent black. With
   the default threshold, 127 is transparent and 128 is opaque. Pixels at or
   above the threshold are composited against the matte in encoded RGB byte
   space, using `round(channel * alpha/255 + matte * (1-alpha/255))`; their output
   alpha is 255. Matte alpha is ignored. Hidden RGB is intentionally discarded
   for transparent output pixels. The original source is untouched until Apply.
2. A histogram counts every distinct opaque RGB color after matting. No sampling
   is used. When transparency requires a black palette entry, opaque black pixels
   use that entry and are excluded from the color histogram. Weighted median-cut
   splits the box with the greatest
   `population × largest channel range`; ties keep the earlier box. Channel-range
   ties prefer red, then green, then blue. Samples sort by that channel, then
   ascending packed `0xRRGGBB`. The split is the first cumulative count reaching
   half the population, clamped to keep both boxes nonempty. The left box keeps
   its position and the right box is appended. Each representative is the
   population-weighted encoded-RGB mean, rounded to the nearest byte.
3. Palette matching minimizes squared encoded-RGB distance, with lower palette
   index winning ties. With dithering, pixels scan left to right, top to bottom.
   Clamp the error-adjusted sample to 0–255 before matching; distribute the
   difference to right, lower-left, lower, and lower-right with weights
   7/16, 3/16, 5/16, and 1/16. Error that reaches a transparent pixel or crosses
   the image boundary is discarded. Calculations use double precision.
4. If any pixel is transparent, palette index zero is reserved as black, leaving
   1, 15, or 255 entries for additional colors at the three depths. Opaque black
   shares index zero, distinguished from transparency by its clear AND bit.
   The reserved black entry participates in opaque color matching; a 1-bit image
   can therefore contain black, white, and transparent pixels together. A completely
   transparent image has only that reserved entry in the active palette. Without transparency, all 2, 16, or 256 entries
   are available for opaque colors. Unused encoded palette slots are padded with
   black; the file carries the full table for its depth.
5. Output uses an uncompressed 40-byte DIB header, bottom-up color rows and AND
   mask rows, DWORD-aligned row strides, BGR0 palette entries, and most-significant
   bits first for packed indices. Transparent color pixels use the reserved
   black index and a set AND bit, producing ordinary transparency without XOR
   effects. The standard ICO/CUR directory retains representation order and
   cursor hotspots.

The public Windows bitmap layout is described by Microsoft's
[BITMAPINFOHEADER documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)
and [bitmap header types](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-header-types).
Palette construction and dithering above are Plan Paint behavior, not a claim
to reproduce another editor's numerical output. Shared batch palettes, palette
file interchange, and direct palette-entry editing remain separate capabilities.

## Normal, hot and disabled icon families

Open an ICO or CUR and choose **Atlas → States…**. The window captures the
current document as a preview snapshot. Choose all representations, the current
image, or the selected Atlas sequence. Sequence order and repeated entries are
preserved. Select a representation in the Preview list to compare its original
pixels with its generated state.

The presets are starting recipes with adjustable saturation, brightness and
opacity. **Reset** restores the selected preset's numbers while retaining the
chosen scope and output policy.

| Preset | Saturation | Brightness | Opacity |
| --- | ---: | ---: | ---: |
| Normal | 100% | 0% | 100% |
| Hot | 120% | +10% | 100% |
| Disabled | 0% | 0% | 55% |

Saturation accepts 0–200%, brightness −100–100%, and opacity 0–100%. These are
Plan Paint recipes, independently defined in encoded RGB byte space. Version
`plan-paint-icon-state-v1` computes gray as `0.2126R + 0.7152G + 0.0722B`.
Each color channel becomes
`gray + saturation/100 × (channel − gray) + brightness × 255/100`, clamped to
0–255 and rounded to the nearest byte. Alpha becomes
`round(alpha × opacity/100)`. Fully transparent source pixels retain their hidden
RGB under the RGBA output policy. Calculations use double precision.

**RGBA** emits the transformed pixels with continuous opacity. **Indexed sources
→ DIB** converts 1-, 4- and 8-bit sources back to their corresponding indexed DIB
depths; 2-bit indexed sources become 4-bit DIB. Other source depths produce RGBA.
Indexed conversion uses the palette algorithm above, an alpha threshold of 128,
a white matte and no dithering. The policy note remains visible, and the report
identifies each representation's source and output format and counts partially
transparent pixels converted by this policy.

Every requested representation receives a result. Background-dependent legacy
XOR pixels cannot be transformed by these recipes; a failed representation stays
in the report and disables export of the requested family. Choosing a scope that
excludes it can produce a complete export. The tool does not silently omit failed
images. Images must fit the supported 1–256 pixel ICO/CUR dimensions, and cursor
hotspots must remain within their image.

**Export…** writes a separate standard ICO or CUR, retaining dimensions, order
and cursor hotspots. The suggested filename adds `-normal`, `-hot` or `-disabled`
to the source stem. A missing extension is added; a mismatched extension is
rejected. The source path, its existing filesystem aliases, and the current
document's path cannot be replaced by a state export. Replacement of another
existing output follows the save dialog's overwrite confirmation and atomic
per-file writing behavior.

**Save report…** writes JSON containing the versioned recipe, output policy and
per-image ready, failed or cancelled decisions. It requires a JSON filename.
Preview changes and exports do not modify the document or its Undo history.
Changing the document after opening the window does not change the captured
snapshot: reopen States to capture newer artwork. Closing the window cancels
pending work, and late results cannot replace a newer recipe preview.

The broader [icon production interview](icon-editor-study/mockups.html) covers
additional palette, alpha, batch and asset-browsing capabilities. Its recommendations
remain candidate designs; implemented features do not establish owner answers
to the remaining interview questions.
