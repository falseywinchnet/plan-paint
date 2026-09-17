# Release verification — 0.1.0

These are observations from September 17, 2026. The release contains compiled native applications. The screenshots in this repository were rendered by the application itself.

## Source and artifacts

All three applications are built from application commit `20898c90176ebaff602b94489723ded077648181`. The release tag adds verification documentation and package hashes without changing application code. `release-manifest.json` records the build source and byte length of every downloadable package, and `SHA256SUMS` records their SHA-256 digests.

## Apple Silicon Mac

- Native Release build on an Apple M4, macOS 26.5 (25F71).
- All three CTest suites pass: core/codec tests, ImGui interaction tests, and CONV warp tests. Final Release test time: 0.62 seconds.
- The same suites pass under AddressSanitizer and UndefinedBehaviorSanitizer: 7.41 seconds, with no reported sanitizer error.
- First-party C++ house-style exclusions and `git diff --check` pass.
- The installer was expanded with `pkgutil --expand-full`. The extracted application passed `codesign --verify --deep --strict` and ran `--demo --screenshot` successfully.
- The bundle includes SDL3, TIFF, WebP, SharpYUV, JPEG, Zstandard and LZMA libraries and their notices. Its linked library paths contain no required Homebrew or user-directory paths.
- The bundled SDL build sets the minimum to macOS 26.0. The app is signed ad hoc; it has no Developer ID signature or Apple notarization. This check exercised the installer payload without claiming a system-wide installation.

## Windows and Linux

Native CI run [35194686928](https://github.com/falseywinchnet/rainstar-paint/actions/runs/35194686928) passed for application source `20898c90176ebaff602b94489723ded077648181`.

- **Windows x64:** compiled and ran on Windows Server 2022 using the native MSVC toolchain. All three CTest suites passed in 0.69 seconds; house-style checks and the graphical `--demo --screenshot` startup passed. The downloaded ZIP contains a PE32+ x86-64 GUI executable, license notices and a folder-launch layout. The [actual Windows capture](screenshots/windows-home.png) was inspected after downloading the artifact.
- **Linux x64:** compiled and ran on Ubuntu 22.04. All three CTest suites passed in 0.75 seconds; house-style checks and graphical startup passed. The packaged launcher itself ran under Xvfb and produced the [actual Linux capture](screenshots/linux-home.png), inspected after download. The tarball contains an ELF x86-64 executable, a launcher, bundled WebP library and license notices. It uses the host desktop/GTK and standard system runtime.
- Both downloaded archives include the embedded-font source notices and application/dependency licenses. No source archive is presented as a compiled application.

## Package identities

The checked-in [manifest](releases/0.1.0/release-manifest.json) and [SHA256SUMS](releases/0.1.0/SHA256SUMS) describe the same packages attached to the release.

| Package | Bytes | SHA-256 |
|---|---:|---|
| `rainstar-paint-0.1.0-macos-arm64.pkg` | 3268601 | `2a9a438a1d5e8d19bd0dd55ab01233756470d5cf30ad50fbe73580be55bac032` |
| `rainstar-paint-0.1.0-windows-x64.zip` | 3013995 | `4394b261ed4ecefca6d38dc2ce6f4c200d1116bc15fed0b277181ae2ec60d23d` |
| `rainstar-paint-0.1.0-linux-x64.tar.gz` | 3253121 | `2c9c80936f7a2989b2a6ed3dda17f5b16aa1e3c95cd25010128b9bacc9db0ebb` |

## What the tests establish

Core tests cover document changes and undo/redo, selection masks, raster tools and patterns, OKLab conversion, native CONV resize against independently retained website outputs, and round trips in seven output formats. Each format also exercises non-ASCII filenames and replacement of an existing file.

Interaction tests feed mouse and keyboard events through a real ImGui context and the application frame loop. They cover drawing, undo/redo, ribbon actions, help, selection, continuous paths and junction snapping, stamp preparation/rotation/scale, font and text entry, reshape/commit, discarding obsolete asynchronous work, Unicode recent-file persistence, unsaved-work protection for recent files, wallpaper composition, persistent custom colors, stable menu cursor requests, stamp reset, arbitrary-angle rotation through the corner handle and numeric input, Shift snapping, and stale rotation-result rejection. They are not a claim of manual end-to-end testing of every OS dialog.

Warp tests cover independent research-reference samples, nodal identity, affine fields, support limits, shared edges, alpha, mesh construction, concave boundaries, fold rejection, knob motion, quadrature ownership and exact quarter-turn behavior. See [WARP_MATH.md](WARP_MATH.md) for numerical tolerances and research provenance.

## Practical boundaries

The tested Windows environment is a native CI machine; Windows 7/10 names the reference Paint interface, and does not assert Windows 7 operating-system compatibility. Linux targets Ubuntu 22.04 or newer and uses the host desktop/GTK print service. Physical printer/scanner hardware, configured third-party mail clients, desktop-background changes and every vendor-specific clipboard/file-manager combination have not been certified by this run.

The application reproduces the classic painting workflow with its own ribbon artwork and Portsmouth typography. The classic File commands include recent pictures, scanner/camera acquisition, email composition, and desktop backgrounds through platform adapters. Mac and Linux acquisition opens a separate capture application and uses save-then-drop to import its result. This is not a pixel-for-pixel or exhaustive Microsoft Paint compatibility certification.

Joint CONV warp sources are capped at one million pixels, with about 800 bytes of retained atlas per source pixel. Area rendering uses bounded positive quadrature, and geometry uses a coarse piecewise-affine mesh; exact clipped moment integration and physical cloth simulation are outside this implementation. The resize implementation independently matches the website demonstrator byte for byte on its retained fixtures.
