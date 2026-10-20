# Icon production: custodial interview and behavioral portfolio

Status: **CANDIDATE design, with GIVEN scope and OBSERVED behavior.** Research date: 20 October 2026.

This portfolio proposes professional icon production inside Plan Paint's existing painting model. It preserves the classic Paint ribbon, path, stamp, pattern and reshape tools. It introduces no persistent layer stack. The requested scope includes batch production and asset browsing.

Start with the [scrolling HTML custodial interview](mockups.html). All 28 questions have named multiple-choice answers and comments. Twenty questions include three visual alternatives and proposed command locations. Large previews sit beside their answers in the reading layout; **Compare side by side** switches to a three-column overview on wide screens. Narrow screens stack each complete choice. Four additional illustrated comparisons explain alpha inspection, shadow geometry, state variants and proofing backgrounds. These specimens illustrate proposed behavior; they are not reference-application output.

The eight behavior questions use full-width answer rows. Each question shows the selected direction above its comments field and links to the next question. The question map shows answered decisions, and **Continue** jumps to the next unanswered question. Recommendations are never preselected. Answers save in the current browser. **Save answer sheet** downloads readable HTML; **Export JSON** in the review section produces a structured decision record. Both include unanswered questions and comments. The page works as a local HTML file or through a local web server; browser storage is specific to the origin used to open it. Export answers before switching origins or clearing browser data.

The [capability inventory](CAPABILITIES.md) maps the reference product's capabilities to proposed locations and dispositions. The [behavioral contracts](SPECIFICATIONS.md) define independently implementable behavior and acceptance cases. The [evidence and source register](EVIDENCE.md) separates direct application observations from documentation and historical reports. The [control conservation ledger](CONSERVATION.md) specifies the GUI.Forms ownership, styling, interaction and verification obligations. The [text question reference](INTERVIEW.md) supports the HTML interview; it is not the answer-entry interface.

The proposed first release centers on:

1. An **Icon** contextual ribbon for ICO documents, using the existing Atlas representation model.
2. Explicit image identity: dimensions, color depth, transparency and storage encoding.
3. Generation and replacement of selected sizes, with protection for hand-tuned small images.
4. Alpha and palette inspection, reversible effect previews, and true-size comparison.
5. A folder-based asset browser in an owned popup.
6. A batch production window with an output manifest, collision handling and per-file results.
7. PNG export, contact sheets, toolbar strips and reusable flattened stamps.

These are recommendations to answer in the interview, not owner decisions. The HTML remains a design interview. Implemented size-generation behavior is documented separately in [icon and cursor size production](../ICON_EDITING.md); the rest of this portfolio is not a claim of implemented features.

## What was actually exercised

The installed IconWorkshop **6.97, 64-bit Professional Edition, 30-day trial** was launched and used through its normal interface. Plan Paint's own artwork was imported, converted into a ten-image ICO, saved, exported into four PNG size folders, and used to produce hot and disabled variants. Blur and shadow previews were inspected and cancelled. A contact sheet was generated. Batch normalization and compositing options were inspected without applying them.

The ICO was opened in Plan Paint. All ten images appeared in Atlas; its size-generation dialog and Gallery popup were inspected. This establishes a useful baseline, not complete codec equivalence or a round-trip guarantee.

Application binaries, private implementation details and proprietary file formats were not inspected. The specifications concern observable behavior and independently chosen contracts. The proposed UI is Plan Paint's own composition; the reference application's arrangement, branding and artwork are not design assets.

## Reading the status labels

| Label | Meaning |
|---|---|
| GIVEN | Explicit owner requirement: broad production, no layers, existing Paint identity, no proprietary replication. |
| OBSERVED | Seen in the installed application's interface or in an actual operation. Observation scope is stated. |
| MEASURED | A reproducible quantitative result. File counts and UI-reported dimensions qualify; visual similarity does not establish pixel equality. |
| DOCUMENTED | Public vendor documentation; recorded as evidence, not promoted to a locally verified result. |
| HISTORICAL REPORT | A dated user's report. Useful for test design, not proof of current defects or capabilities. |
| CANDIDATE | Proposed independent Plan Paint behavior awaiting the owner's choice. |
| REJECTED | Outside the explicit study scope, such as layers or proprietary output replication. |
| DECIDED | Reserved for an explicit accepted interview answer. Recommendations do not acquire this status automatically. |

## Coverage boundary

The inventory covers the major production, editing, browsing, interchange and integration families found in the installed UI and the linked public material. It does **not** claim to have read every website or forum, every historical manual page, or every possible state of the application. Exact proprietary filter kernels, nearest-format tie breaking, collision behavior for every command, malformed-file handling, large-job cancellation, clipboard interoperability and non-Windows platform behavior remain unmeasured. Those gaps are named alongside proposed tests instead of being filled with guesses.

The source register includes conflicting and dated claims. For example, the current application exposes more than 256-pixel ICO options, while the proposed interoperability profile is limited to 256 pixels until target consumers are tested independently.

## Implementation discipline

Use the pinned standalone GUI.Forms dependency and the public Application/Threading targets described in [the native build guide](../GUI_FORMS_PORT.md). First-party C++ remains C++20 with explicit types, named callbacks, dot member access, visible ownership, double-precision numerical kernels and no fast-math. Do not modify the shared toolkit to implement Paint-specific behavior.

Before releasing an implementation, run the consumer `check` target, CTest and `python3 scripts/check-style.py`, inspect native rendered output, and validate each released platform independently. The HTML mockups demonstrate composition and choices; they are not native rendering evidence.
