# Rainstar Paint 0.1.1 — Retina rendering repair

A native Retina display exposed a coordinate mismatch in 0.1.0: ImGui supplied logical drawing coordinates and a 2× physical framebuffer, but the application left SDL's geometry scale at 1×. Clip rectangles were scaled independently by the backend. The result was a half-width, half-height interface with canvas pixels escaping their intended viewport.

Version 0.1.1 sets SDL's renderer scale from each frame's framebuffer density before drawing. The backend detects that scale and applies clipping in the same coordinate system. Repeating this each frame also handles density changes rather than assuming the display present at startup.

## Regression evidence

- The new pixel-readback regression failed before applying the fix and passed afterward.
- It renders the real application frame and an oversized canvas at 1×, 1.5× and 2× into physical framebuffers. Checks compare window coverage, ribbon pixels, canvas content and clipping above the status bar against the 1× reference.
- It checks logical window sizes 1280×850, 1160×600 and 1440×900, returning from high density to 1× between cases.
- The software-renderer test runs in the regular interaction suite. The same test separately passed through the Apple M4's native Metal renderer using `paint-ui-tests --native-render-test`.
- The M4 Release build passed all three CTest suites in 0.82 seconds. AddressSanitizer and UndefinedBehaviorSanitizer also passed all three suites.

The M4's attached display is 1×. The higher-density checks use actual rendered 2× framebuffer targets; they are not a claim of testing the user's MacBook Neo or its physical display.

Package hashes, application source and native CI evidence accompany the release in `SHA256SUMS` and `release-manifest.json`. The macOS 26+ requirement and ad-hoc application signature remain as documented in the README.
