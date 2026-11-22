#!/bin/bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: Package-Mac.sh --engine-root /path/to/UE_5.3 [--output-dir /path/to/output]

Builds and archives a Development macOS app using the local Mac/Xcode toolchain.
It does not sign, notarize, or submit the app.
EOF
}
die() { printf 'Package-Mac: ERROR: %s\n' "$*" >&2; exit 2; }

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REVISION_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
ENGINE_ROOT="${UE_ENGINE_ROOT:-}"
OUTPUT_DIR="$REVISION_ROOT/Builds/Apple/Mac/v0.9"
while (($#)); do
  case "$1" in
    --engine-root) (($# >= 2)) || die 'missing value for --engine-root'; ENGINE_ROOT="$2"; shift 2 ;;
    --output-dir) (($# >= 2)) || die 'missing value for --output-dir'; OUTPUT_DIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ "$(uname -s)" == Darwin ]] || die 'native macOS packaging must run on macOS with Xcode; Windows/Linux hosts are unsupported.'
[[ -n "$ENGINE_ROOT" ]] || die 'provide --engine-root or UE_ENGINE_ROOT pointing to the installed UE 5.3 directory.'
[[ -d "$ENGINE_ROOT/Engine" ]] || die "Unreal Engine root is missing: $ENGINE_ROOT"
grep -Eq '"EngineAssociation"[[:space:]]*:[[:space:]]*"5\.3"' "$REVISION_ROOT/Game/Chronicles.uproject" || die 'this source handoff is pinned to Unreal Engine 5.3.'
ENGINE_VERSION_FILE="$ENGINE_ROOT/Engine/Build/Build.version"
[[ -f "$ENGINE_VERSION_FILE" ]] || die "Unreal engine version file is missing: $ENGINE_VERSION_FILE"
grep -Eq '"MajorVersion"[[:space:]]*:[[:space:]]*5([[:space:]]*[,}]|$)' "$ENGINE_VERSION_FILE" && grep -Eq '"MinorVersion"[[:space:]]*:[[:space:]]*3([[:space:]]*[,}]|$)' "$ENGINE_VERSION_FILE" || die 'the selected engine root is not Unreal Engine 5.3.'

UAT="$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
EDITOR_CMD="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
PROJECT="$REVISION_ROOT/Game/Chronicles.uproject"
[[ -x "$UAT" ]] || die "RunUAT.sh is not executable at $UAT; install the UE 5.3 macOS engine/toolchain."
[[ -x "$EDITOR_CMD" ]] || die "UnrealEditor-Cmd is missing at $EDITOR_CMD; use a complete UE 5.3 macOS installation."
[[ -f "$PROJECT" && -d "$REVISION_ROOT/Game/Source" && -d "$REVISION_ROOT/Game/Content/Data" ]] || die 'source handoff is incomplete; Game project, C++ source, or Content/Data is missing.'
grep -Eq '^\+DirectoriesToAlwaysStageAsNonUFS=\(Path="Data"\)' "$REVISION_ROOT/Game/Config/DefaultGame.ini" || die 'DefaultGame.ini must stage Content/Data as loose files for parity with Windows/Android.'

command -v xcodebuild >/dev/null 2>&1 || die 'Xcode command line tools are missing; install/select Xcode 14.x compatible with UE 5.3.'
command -v xcode-select >/dev/null 2>&1 || die 'xcode-select is unavailable; install Xcode before packaging.'
XCODE_PATH="$(xcode-select -p 2>/dev/null)" || die 'no active Xcode developer directory; run xcode-select -s /Applications/Xcode_14.app/Contents/Developer.'
[[ -d "$XCODE_PATH" ]] || die "active Xcode developer directory does not exist: $XCODE_PATH"
XCODE_VERSION="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}')"
[[ "$XCODE_VERSION" =~ ^14\.[1-9]([.][0-9]+)?$ ]] || die "UE 5.3's documented Mac toolchain is Xcode 14.1/14.x; selected Xcode '$XCODE_VERSION'. Select a compatible Xcode before packaging."
MACOS_VERSION="$(sw_vers -productVersion 2>/dev/null)"
[[ -n "$MACOS_VERSION" ]] || die 'could not read macOS version.'
MACOS_MAJOR="${MACOS_VERSION%%.*}"
MACOS_MINOR="${MACOS_VERSION#*.}"
MACOS_MINOR="${MACOS_MINOR%%.*}"
if (( MACOS_MAJOR < 12 || (MACOS_MAJOR == 12 && MACOS_MINOR < 5) )); then
  die "UE 5.3 requires macOS 12.5 or newer; detected $MACOS_VERSION."
fi

mkdir -p "$OUTPUT_DIR/Archive"
LOG="$OUTPUT_DIR/Package-Mac-Development.log"
printf 'Packaging Chronicles v0.9 for macOS with UE 5.3 and Xcode %s.\n' "$XCODE_VERSION"
if ! "$UAT" BuildCookRun \
  "-project=$PROJECT" -noP4 -target=Chronicles -platform=Mac -clientconfig=Development \
  -build -cook -stage -pak -package -archive "-archivedirectory=$OUTPUT_DIR/Archive" \
  -utf8output -unattended -stdout -FullStdOutLogOutput >"$LOG" 2>&1; then
  tail -n 100 "$LOG" >&2 || true
  die "Unreal macOS package failed. Review $LOG."
fi

APP_COUNT="$(find "$OUTPUT_DIR/Archive" -type d -name '*.app' -prune | wc -l | tr -d ' ')"
[[ "$APP_COUNT" == 1 ]] || die "UAT completed but expected one .app under $OUTPUT_DIR/Archive; found $APP_COUNT. Review $LOG."
APP_PATH="$(find "$OUTPUT_DIR/Archive" -type d -name '*.app' -prune -print -quit)"
printf 'macOS Development app created: %s\n' "$APP_PATH"
printf 'Package log: %s\n' "$LOG"
printf 'This app is not code-signed for distribution or notarized.\n'
