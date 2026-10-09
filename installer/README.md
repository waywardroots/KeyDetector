# Installer resources

This directory holds the files needed to build the end-user installers for
**Key Detector** on Windows and macOS. The license text that each installer
shows during setup lives here so both platforms draw from a single source.

## Contents

| File        | Format     | Used by                                                      |
|-------------|------------|-------------------------------------------------------------|
| `EULA.txt`  | Plain text (ASCII) | NSIS `LicenseData`, Inno Setup `LicenseFile`, macOS `productbuild` `<license>` |
| `EULA.rtf`  | Rich text  | WiX `WixUILicenseRtf`, Inno Setup `LicenseFile`, macOS `.pkg` license (formatted) |

The **canonical EULA** is the signed PDF supplied by Fuzzy Audio LLC:
[`../End User License Agreement - FuzzyAudio - Key Detector.pdf`](../End%20User%20License%20Agreement%20-%20FuzzyAudio%20-%20Key%20Detector.pdf).
Installers cannot display a PDF in their license pane, so `EULA.txt` and
`EULA.rtf` are generated from it. If the PDF is updated, regenerate them with:

```sh
python3 installer/make_license_files.py
```

The script (`make_license_files.py`) extracts the PDF text with `pdftotext`,
strips page numbers / zero-width spaces, segments on clause markers, and writes
both files. `EULA.txt` is normalized to **pure ASCII** so it renders correctly in
every installer's license box regardless of code page. The generated text is
verified word-for-word against the PDF (2730 words, exact match).

## Planned installers

### Windows (`.exe`)
Recommended tooling: **Inno Setup** (free, simple) or **WiX Toolset** (MSI).
The installer should place the built artifacts in the standard locations:

- VST3  -> `C:\Program Files\Common Files\VST3\Key Detector.vst3`
- Standalone -> `C:\Program Files\Fuzzy Audio\Key Detector\Key Detector.exe`

The license page reads `EULA.rtf` (Inno/WiX) or `EULA.txt` (NSIS). The signed
VST3/Standalone binaries come from the Windows CI job
(`.github/workflows/windows-vst3.yml`).

### macOS (`.pkg`)
Recommended tooling: **`pkgbuild` + `productbuild`**, producing a distribution
`.pkg` with the EULA shown via the distribution XML `<license file="EULA.txt"/>`
(or `EULA.rtf`). Component install locations:

- AU   -> `/Library/Audio/Plug-Ins/Components/Key Detector.component`
- VST3 -> `/Library/Audio/Plug-Ins/VST3/Key Detector.vst3`
- Standalone -> `/Applications/Key Detector.app`

The universal (arm64 + x86_64) binaries come from the macOS CI job
(`.github/workflows/macos-au-vst3.yml`). For public distribution the `.pkg`
should be **signed** ("Developer ID Installer") and **notarized**.

## TODO before shipping a real installer
- [ ] Finalize the bracketed placeholders still present in the signed PDF:
      `[Key Detector]`, `[Colorado]`, and `[download/install]`.
- [ ] Retire the old `../EULA.md` template (it still names *AudioFuzz* /
      *JamesandtheCat* and is superseded by the Fuzzy Audio LLC PDF).
- [ ] Add the Inno/WiX script under `installer/windows/`.
- [ ] Add the `pkgbuild`/`productbuild` script under `installer/macos/`.
- [ ] Wire code signing (Windows Authenticode, macOS Developer ID + notarization).
