#!/usr/bin/env bash
# ============================================================================
#  Key Detector - macOS installer builder  (.pkg)
#  (c) 2026 Fuzzy Audio LLC
# ----------------------------------------------------------------------------
#  Produces a distribution .pkg that installs the AU and VST3 plug-ins (and the
#  Standalone .app if present) into the standard macOS locations, showing the
#  Fuzzy Audio EULA during install.
#
#  USAGE (on macOS):
#    1. Download the "KeyDetector-macOS-AU-VST3" artifact from the GitHub
#       Actions run and unzip it so you have:
#           installer/macos/artifacts/Key Detector.component   (AU)
#           installer/macos/artifacts/Key Detector.vst3        (VST3)
#       Optionally also add a Standalone build:
#           installer/macos/artifacts/Key Detector.app
#    2. Run:
#           installer/macos/build_pkg.sh 1.0.0
#       The finished installer lands in installer/macos/output/.
#
#  OPTIONAL signing / notarization (for public distribution) via env vars:
#    APP_SIGN_ID        "Developer ID Application: Name (TEAMID)"  - signs bundles
#    INSTALLER_SIGN_ID  "Developer ID Installer: Name (TEAMID)"    - signs the .pkg
#    NOTARY_PROFILE     a notarytool keychain profile              - notarize+staple
# ============================================================================
set -euo pipefail

VERSION="${1:-1.0.0}"
HERE="$(cd "$(dirname "$0")" && pwd)"
INSTALLER_DIR="$(cd "$HERE/.." && pwd)"
ART="$HERE/artifacts"
BUILD="$HERE/build"
OUT="$HERE/output"

APP_NAME="Key Detector"
PKGID="com.fuzzyaudio.keydetector"
TITLE="Key Detector"

AU_SRC="$ART/$APP_NAME.component"
VST3_SRC="$ART/$APP_NAME.vst3"
APP_SRC="$ART/$APP_NAME.app"
AAX_SRC="$ART/$APP_NAME.aaxplugin"

rm -rf "$BUILD" "$OUT"
mkdir -p "$BUILD" "$OUT"

# --- helper: optionally codesign a bundle ----------------------------------
sign_bundle () {
  local bundle="$1"
  if [[ -n "${APP_SIGN_ID:-}" ]]; then
    echo "  codesign: $bundle"
    codesign --force --deep --options runtime --timestamp \
      --sign "$APP_SIGN_ID" "$bundle"
  fi
}

# --- helper: build one component pkg (bundle -> install location) ------------
# args: <source-bundle> <install-location> <id-suffix> <out-var-name>
build_component () {
  local src="$1" dest="$2" suffix="$3" __outvar="$4"
  [[ -d "$src" ]] || { eval "$__outvar=''"; return; }
  local root="$BUILD/root-$suffix"
  mkdir -p "$root"
  cp -R "$src" "$root/"
  sign_bundle "$root/$(basename "$src")"
  local pkg="$BUILD/KeyDetector-$suffix.pkg"
  echo "pkgbuild: $suffix -> $dest"
  pkgbuild --identifier "$PKGID.$suffix" --version "$VERSION" \
           --install-location "$dest" --root "$root" "$pkg"
  eval "$__outvar=\"\$pkg\""
}

AU_PKG=""; VST3_PKG=""; APP_PKG=""; AAX_PKG=""
build_component "$AU_SRC"   "/Library/Audio/Plug-Ins/Components"            "au"         AU_PKG
build_component "$VST3_SRC" "/Library/Audio/Plug-Ins/VST3"                  "vst3"       VST3_PKG
build_component "$APP_SRC"  "/Applications"                                 "standalone" APP_PKG
# AAX (Pro Tools) - only packaged if you staged a (wraptool-signed) .aaxplugin
build_component "$AAX_SRC"  "/Library/Application Support/Avid/Audio/Plug-Ins" "aax"     AAX_PKG

if [[ -z "$AU_PKG$VST3_PKG$APP_PKG$AAX_PKG" ]]; then
  echo "ERROR: no artifacts found in $ART" >&2
  echo "       expected '$APP_NAME.component', '$APP_NAME.vst3', '$APP_NAME.app' and/or '$APP_NAME.aaxplugin'" >&2
  exit 1
fi

# --- generate distribution.xml (only the components we actually built) -------
DIST="$BUILD/distribution.xml"
{
  echo '<?xml version="1.0" encoding="utf-8"?>'
  echo '<installer-gui-script minSpecVersion="2">'
  echo "  <title>$TITLE</title>"
  echo "  <organization>com.fuzzyaudio</organization>"
  echo '  <license file="EULA.txt"/>'
  echo '  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>'
  echo '  <choices-outline>'
  [[ -n "$AU_PKG"   ]] && echo '    <line choice="au"/>'
  [[ -n "$VST3_PKG" ]] && echo '    <line choice="vst3"/>'
  [[ -n "$AAX_PKG"  ]] && echo '    <line choice="aax"/>'
  [[ -n "$APP_PKG"  ]] && echo '    <line choice="standalone"/>'
  echo '  </choices-outline>'
  if [[ -n "$AU_PKG" ]]; then
    echo '  <choice id="au" title="Audio Unit (AU)">'
    echo "    <pkg-ref id=\"$PKGID.au\"/>"
    echo '  </choice>'
    echo "  <pkg-ref id=\"$PKGID.au\" version=\"$VERSION\" onConclusion=\"none\">$(basename "$AU_PKG")</pkg-ref>"
  fi
  if [[ -n "$VST3_PKG" ]]; then
    echo '  <choice id="vst3" title="VST3">'
    echo "    <pkg-ref id=\"$PKGID.vst3\"/>"
    echo '  </choice>'
    echo "  <pkg-ref id=\"$PKGID.vst3\" version=\"$VERSION\" onConclusion=\"none\">$(basename "$VST3_PKG")</pkg-ref>"
  fi
  if [[ -n "$AAX_PKG" ]]; then
    echo '  <choice id="aax" title="AAX (Pro Tools)">'
    echo "    <pkg-ref id=\"$PKGID.aax\"/>"
    echo '  </choice>'
    echo "  <pkg-ref id=\"$PKGID.aax\" version=\"$VERSION\" onConclusion=\"none\">$(basename "$AAX_PKG")</pkg-ref>"
  fi
  if [[ -n "$APP_PKG" ]]; then
    echo '  <choice id="standalone" title="Standalone application">'
    echo "    <pkg-ref id=\"$PKGID.standalone\"/>"
    echo '  </choice>'
    echo "  <pkg-ref id=\"$PKGID.standalone\" version=\"$VERSION\" onConclusion=\"none\">$(basename "$APP_PKG")</pkg-ref>"
  fi
  echo '</installer-gui-script>'
} > "$DIST"

# --- combine into a single distribution .pkg --------------------------------
FINAL="$OUT/KeyDetector-$VERSION-macOS.pkg"
echo "productbuild: $FINAL"
if [[ -n "${INSTALLER_SIGN_ID:-}" ]]; then
  productbuild --distribution "$DIST" --package-path "$BUILD" \
    --resources "$INSTALLER_DIR" --sign "$INSTALLER_SIGN_ID" "$FINAL"
else
  productbuild --distribution "$DIST" --package-path "$BUILD" \
    --resources "$INSTALLER_DIR" "$FINAL"
fi

# --- optional notarization + stapling ---------------------------------------
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  echo "notarytool: submitting $FINAL"
  xcrun notarytool submit "$FINAL" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$FINAL"
fi

echo "Done: $FINAL"
