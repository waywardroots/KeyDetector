# Installer resources

This directory holds the files needed to build the end-user installers for
**Key Detector** on Windows and macOS. The license text that each installer
shows during setup lives here so both platforms draw from a single source.

## Contents

| Path                      | Purpose                                                           |
|---------------------------|------------------------------------------------------------------|
| `EULA.txt`                | License text, pure ASCII (NSIS / Inno / macOS `productbuild`)     |
| `EULA.rtf`                | License text, rich text (WiX / Inno / macOS `.pkg` formatted)     |
| `make_license_files.py`   | Regenerates `EULA.txt` / `EULA.rtf` from the signed PDF           |
| `windows/KeyDetector.iss` | Inno Setup 6 script -> Windows `setup.exe`                        |
| `macos/build_pkg.sh`      | `pkgbuild` + `productbuild` script -> macOS `.pkg`                |

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

## Building the installers

Both installers consume the plug-ins built by CI. Download the artifact from the
relevant GitHub Actions run and unzip it into an `artifacts/` folder next to the
script (these folders are git-ignored).

### Windows (`installer/windows/KeyDetector.iss`, Inno Setup 6.3+)
Stage the build, then compile with the Inno Setup command-line compiler `iscc`:

```
installer\windows\artifacts\Key Detector.vst3\    (from "KeyDetector-VST3-Windows")
installer\windows\artifacts\Key Detector.exe      (optional Standalone)

iscc /DAppVersion=1.0.0 installer\windows\KeyDetector.iss
```

Output: `installer\windows\output\KeyDetector-1.0.0-Windows-x64.exe`. It installs
the VST3 to `C:\Program Files\Common Files\VST3\Key Detector.vst3` (and the
Standalone to `C:\Program Files\Fuzzy Audio\Key Detector\` if staged), showing
`EULA.rtf` on the license page. The Standalone component appears only when the
`.exe` is present at compile time.

### macOS (`installer/macos/build_pkg.sh`, `pkgbuild` + `productbuild`)
Stage the build, then run the script with the version:

```
installer/macos/artifacts/Key Detector.component  (AU)   from "KeyDetector-macOS-AU-VST3"
installer/macos/artifacts/Key Detector.vst3       (VST3)
installer/macos/artifacts/Key Detector.app        (optional Standalone)

installer/macos/build_pkg.sh 1.0.0
```

Output: `installer/macos/output/KeyDetector-1.0.0-macOS.pkg`. It installs AU to
`/Library/Audio/Plug-Ins/Components`, VST3 to `/Library/Audio/Plug-Ins/VST3`
(and the Standalone to `/Applications` if staged), showing `EULA.txt` on the
license pane. Set `APP_SIGN_ID`, `INSTALLER_SIGN_ID`, and `NOTARY_PROFILE` to
sign and notarize for public distribution (see the header comment in the script).

## TODO before shipping a real installer
- [ ] Finalize the bracketed placeholders still present in the signed PDF:
      `[Key Detector]`, `[Colorado]`, and `[download/install]`.
- [ ] Set the real `AppURL` in `windows/KeyDetector.iss`.
- [ ] Wire code signing (Windows Authenticode via `signtool`; macOS Developer ID
      + notarization via the env vars above).
