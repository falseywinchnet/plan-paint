# Rainstar Paint 0.1.2 — idle CPU and GPU work

Version 0.1.1 rebuilt and submitted the interface continuously, even when the picture and UI were unchanged. Version 0.1.2 waits for SDL events and compares the complete draw lists against the last submitted frame. It skips renderer work and presentation when vertices, indices, commands, clipping, framebuffer density and texture contents are unchanged.

Input wakes the wait immediately. Held input, queued ImGui input and settling frames use the interactive cadence. A 250 ms idle timer keeps tooltips and text-caret blinking alive; only visible changes reach the GPU. Native file-dialog completion and CONV worker completion push wake events. Hidden, minimized and occluded windows do not submit frames. Window events force repaint when necessary.

## Measurements

On the Apple M4 with its 1× display, an untouched visible blank window used 16.91% of one CPU core in 0.1.1 and 0.33% in 0.1.2. Both samples used `ps` cumulative process CPU time divided by monotonic elapsed time over 15 seconds after warmup. These launches did not have keyboard focus; the measurement is not a claim about every focus, document, display or machine configuration.

The 0.1.2 frame counter recorded 80 lightweight UI updates and **zero GPU presentations** during a separate 20.027-second idle interval. A visibility-instrumented run confirmed 40 visible frames and zero GPU presentations over 10.013 seconds. Presentation count describes this application's submissions, not system-wide GPU utilization.

A repeat launch through macOS LaunchServices measured 16.58% → 0.30% CPU, with 68 visible UI updates and zero GPU presentations over 17.013 seconds. The environment still reported no keyboard focus in that run. The [raw measurements](idle-measurements.json) preserve the samples and their limits.

## Checks

- Release: all three CTest suites passed, 1.18 seconds.
- AddressSanitizer and UndefinedBehaviorSanitizer: all three suites passed, 8.52 seconds.
- New checks cover unchanged-frame suppression, texture-only painting changes, display-density and clip changes, callback redraws, stationary tooltips, text-caret blinking, queued input, interactive wait cadence and worker-completion wakeup.
- Existing drawing, selection, paths, stamps, rotation, reshape and Retina pixel-readback regressions remain enabled.

`--idle-report 15 report.json` collects frame, presentation, visibility, focus and framebuffer-density counts after a three-second warmup. CPU samples are taken separately with host process accounting.

The macOS 26+ requirement and ad-hoc application signature remain unchanged. Native build and package identities are recorded with the release manifest and checksums.

## Compiled release

The three packages use application source `8792afefbee7625c07e95c37f5df38d926c17cf1`. The release tag adds verification documentation and hashes only.

- [Native CI run 35197466290](https://github.com/falseywinchnet/rainstar-paint/actions/runs/35197466290) passed on Windows Server 2022 and Ubuntu 22.04: all three test suites, the new idle checks, graphical startup and the Linux portable launcher.
- The Mac installer was expanded. Its app reports version 0.1.2, passed strict ad-hoc signature verification, and launched to render its actual demo screenshot.
- Downloaded Windows/Linux archives contain native PE/ELF executables and required license notices.

[Package manifest](release-manifest.json) and [SHA-256 checksums](SHA256SUMS).
