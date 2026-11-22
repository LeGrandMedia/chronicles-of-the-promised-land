#!/bin/bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: Package-iOS.sh --engine-root /path/to/UE_5.3 [--output-dir /path/to/output]

Builds a signed Development IPA for sideload/device testing. Requires the Apple
Development certificate and matching device provisioning profile already
installed on this Mac; it never installs credentials or deploys to a device.

Required environment (values are not printed by this script):
  APPLE_SIGNING_IDENTITY       Exact Apple Development identity in the login keychain
  APPLE_PROVISIONING_PROFILE   Installed .mobileprovision filename
  APPLE_TEAM_ID                10-character Apple Developer Team ID
EOF
}
die() { printf 'Package-iOS: ERROR: %s\n' "$*" >&2; exit 2; }

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REVISION_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
ENGINE_ROOT="${UE_ENGINE_ROOT:-}"
OUTPUT_DIR="$REVISION_ROOT/Builds/Apple/iOS/v0.9"
while (($#)); do
  case "$1" in
    --engine-root) (($# >= 2)) || die 'missing value for --engine-root'; ENGINE_ROOT="$2"; shift 2 ;;
    --output-dir) (($# >= 2)) || die 'missing value for --output-dir'; OUTPUT_DIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
done

[[ "$(uname -s)" == Darwin ]] || die 'native iOS packaging must run on a Mac with Xcode; Windows remote-build is not configured in this source handoff.'
[[ -n "$ENGINE_ROOT" ]] || die 'provide --engine-root or UE_ENGINE_ROOT pointing to the installed UE 5.3 directory.'
[[ -d "$ENGINE_ROOT/Engine" ]] || die "Unreal Engine root is missing: $ENGINE_ROOT"
grep -Eq '"EngineAssociation"[[:space:]]*:[[:space:]]*"5\.3"' "$REVISION_ROOT/Game/Chronicles.uproject" || die 'this source handoff is pinned to Unreal Engine 5.3.'
ENGINE_VERSION_FILE="$ENGINE_ROOT/Engine/Build/Build.version"
[[ -f "$ENGINE_VERSION_FILE" ]] || die "Unreal engine version file is missing: $ENGINE_VERSION_FILE"
grep -Eq '"MajorVersion"[[:space:]]*:[[:space:]]*5([[:space:]]*[,}]|$)' "$ENGINE_VERSION_FILE" && grep -Eq '"MinorVersion"[[:space:]]*:[[:space:]]*3([[:space:]]*[,}]|$)' "$ENGINE_VERSION_FILE" || die 'the selected engine root is not Unreal Engine 5.3.'
UAT="$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
EDITOR_CMD="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
PROJECT="$REVISION_ROOT/Game/Chronicles.uproject"
IOS_CONFIG="$REVISION_ROOT/Game/Config/IOS/IOSEngine.ini"
[[ -x "$UAT" ]] || die "RunUAT.sh is not executable at $UAT; install UE 5.3 for macOS."
[[ -x "$EDITOR_CMD" ]] || die "UnrealEditor-Cmd is missing at $EDITOR_CMD; use a complete UE 5.3 macOS installation."
[[ -f "$PROJECT" && -d "$REVISION_ROOT/Game/Source" && -d "$REVISION_ROOT/Game/Content/Data" ]] || die 'source handoff is incomplete; Game project, C++ source, or Content/Data is missing.'
[[ -f "$IOS_CONFIG" ]] || die "iOS config is missing: $IOS_CONFIG"
BUNDLE_ID="$(awk -F= '/^BundleIdentifier=/ {print $2; exit}' "$IOS_CONFIG")"
[[ "$BUNDLE_ID" =~ ^[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+$ ]] || die 'IOSEngine.ini must contain a valid reverse-DNS BundleIdentifier.'
grep -q '^MinimumiOSVersion=IOS_15$' "$IOS_CONFIG" || die 'v0.9 iOS deployment target must remain iOS 15.'
grep -Eq '^\+DirectoriesToAlwaysStageAsNonUFS=\(Path="Data"\)' "$REVISION_ROOT/Game/Config/DefaultGame.ini" || die 'DefaultGame.ini must stage Content/Data as loose files.'

command -v xcodebuild >/dev/null 2>&1 || die 'Xcode is missing; install/select Xcode 14.x compatible with UE 5.3.'
command -v xcode-select >/dev/null 2>&1 || die 'xcode-select is unavailable; install Xcode before packaging.'
XCODE_PATH="$(xcode-select -p 2>/dev/null)" || die 'no active Xcode developer directory; select a compatible Xcode with xcode-select.'
[[ -d "$XCODE_PATH" ]] || die "active Xcode developer directory does not exist: $XCODE_PATH"
XCODE_VERSION="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}')"
[[ "$XCODE_VERSION" =~ ^14\.[1-9]([.][0-9]+)?$ ]] || die "UE 5.3's documented Apple build matrix is Xcode 14.1/14.x; selected Xcode '$XCODE_VERSION'. Select a compatible Xcode or use a newer Unreal release."
MACOS_VERSION="$(sw_vers -productVersion 2>/dev/null)"
[[ -n "$MACOS_VERSION" ]] || die 'could not read macOS version.'
MACOS_MAJOR="${MACOS_VERSION%%.*}"
MACOS_MINOR="${MACOS_VERSION#*.}"
MACOS_MINOR="${MACOS_MINOR%%.*}"
if (( MACOS_MAJOR < 12 || (MACOS_MAJOR == 12 && MACOS_MINOR < 5) )); then
  die "UE 5.3 requires macOS 12.5 or newer; detected $MACOS_VERSION."
fi
command -v xcrun >/dev/null 2>&1 || die 'xcrun is missing; install the Xcode command line tools.'
xcrun --sdk iphoneos --show-sdk-version >/dev/null 2>&1 || die 'Xcode iPhoneOS SDK is unavailable; install the iOS platform components for Xcode.'

: "${APPLE_SIGNING_IDENTITY:?Set APPLE_SIGNING_IDENTITY to the exact installed Apple Development certificate identity.}"
: "${APPLE_PROVISIONING_PROFILE:?Set APPLE_PROVISIONING_PROFILE to an installed development .mobileprovision filename.}"
: "${APPLE_TEAM_ID:?Set APPLE_TEAM_ID to the Apple Developer team ID for that profile.}"
[[ "$APPLE_TEAM_ID" =~ ^[A-Z0-9]{10}$ ]] || die 'APPLE_TEAM_ID must contain 10 uppercase letters/digits.'
command -v security >/dev/null 2>&1 || die 'macOS security command is unavailable; install the Xcode command line tools.'
if ! security find-identity -v -p codesigning 2>/dev/null | grep -Fq -- "$APPLE_SIGNING_IDENTITY"; then
  die 'the requested Apple Development signing identity is not valid in the current keychain; install it using Xcode/Apple Developer tooling.'
fi
PROFILE_ROOT="$HOME/Library/MobileDevice/Provisioning Profiles"
[[ "$APPLE_PROVISIONING_PROFILE" == *.mobileprovision ]] || die 'APPLE_PROVISIONING_PROFILE must be the installed .mobileprovision filename.'
PROFILE_PATH="$PROFILE_ROOT/$APPLE_PROVISIONING_PROFILE"
[[ -f "$PROFILE_PATH" ]] || die "the named provisioning profile is not installed in $PROFILE_ROOT; install it using Xcode before packaging."

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT
security cms -D -i "$PROFILE_PATH" -o "$TEMP_DIR/profile.plist" 2>/dev/null || die 'the selected provisioning profile could not be decoded; install a valid Apple development profile.'
PROFILE_APP_ID="$(/usr/libexec/PlistBuddy -c 'Print :Entitlements:application-identifier' "$TEMP_DIR/profile.plist" 2>/dev/null || true)"
PROFILE_TEAM_ID="$(/usr/libexec/PlistBuddy -c 'Print :TeamIdentifier:0' "$TEMP_DIR/profile.plist" 2>/dev/null || true)"
PROFILE_GET_TASK_ALLOW="$(/usr/libexec/PlistBuddy -c 'Print :Entitlements:get-task-allow' "$TEMP_DIR/profile.plist" 2>/dev/null || true)"
PROFILE_HAS_DEVICES="$(/usr/libexec/PlistBuddy -c 'Print :ProvisionedDevices:0' "$TEMP_DIR/profile.plist" >/dev/null 2>&1 && printf yes || printf no)"
PROFILE_EXPIRATION="$(/usr/libexec/PlistBuddy -c 'Print :ExpirationDate' "$TEMP_DIR/profile.plist" 2>/dev/null || true)"
PROFILE_EXPIRATION_EPOCH="$(date -j -f '%Y-%m-%d %H:%M:%S %z' "$PROFILE_EXPIRATION" '+%s' 2>/dev/null || true)"
EXPECTED_APP_ID="$APPLE_TEAM_ID.$BUNDLE_ID"
[[ "$PROFILE_TEAM_ID" == "$APPLE_TEAM_ID" ]] || die 'the provisioning profile team does not match APPLE_TEAM_ID.'
PROFILE_APP_ID_MATCHES=false
if [[ "$PROFILE_APP_ID" == "$EXPECTED_APP_ID" ]]; then
  PROFILE_APP_ID_MATCHES=true
elif [[ "$PROFILE_APP_ID" == "$APPLE_TEAM_ID."* ]]; then
  PROFILE_PATTERN="${PROFILE_APP_ID#"$APPLE_TEAM_ID."}"
  if [[ "$PROFILE_PATTERN" == '*' ]]; then
    PROFILE_APP_ID_MATCHES=true
  elif [[ "$PROFILE_PATTERN" == *'.*' ]]; then
    PROFILE_PREFIX="${PROFILE_PATTERN%'.*'}"
    if [[ "$PROFILE_PREFIX" != *'*'* && "$BUNDLE_ID" == "$PROFILE_PREFIX".* ]]; then
      PROFILE_APP_ID_MATCHES=true
    fi
  fi
fi
[[ "$PROFILE_APP_ID_MATCHES" == true ]] || die 'the provisioning profile application identifier is not an exact match or a valid wildcard that matches the configured iOS bundle identifier.'
[[ "$PROFILE_HAS_DEVICES" == yes ]] || die 'an iOS Development profile with registered device identifiers is required for this sideload IPA.'
[[ "$PROFILE_GET_TASK_ALLOW" == true ]] || die 'the selected provisioning profile is not marked for development signing (get-task-allow must be true).'
[[ "$PROFILE_EXPIRATION_EPOCH" =~ ^[0-9]+$ ]] || die 'the provisioning profile expiration date could not be validated; install a valid development profile.'
(( PROFILE_EXPIRATION_EPOCH > $(date '+%s') )) || die 'the selected provisioning profile has expired; renew it with Apple Developer tooling.'

mkdir -p "$OUTPUT_DIR/Archive"
if find "$OUTPUT_DIR/Archive" -type f -name '*.ipa' -print -quit | grep -q .; then
  die "output archive already contains an IPA; choose a fresh --output-dir to avoid mistaking an old build for a new one: $OUTPUT_DIR/Archive"
fi
LOG="$OUTPUT_DIR/Package-iOS-Development.log"
BUILD_MARKER="$TEMP_DIR/build-start"
touch "$BUILD_MARKER"
printf 'Packaging Chronicles v0.9 for iOS Development with UE 5.3 and Xcode %s.\n' "$XCODE_VERSION"
if ! "$UAT" BuildCookRun \
  "-project=$PROJECT" -noP4 -target=Chronicles -platform=IOS -clientconfig=Development \
  "-certificate=$APPLE_SIGNING_IDENTITY" "-provision=$APPLE_PROVISIONING_PROFILE" "-team=$APPLE_TEAM_ID" "-teamid=$APPLE_TEAM_ID" \
  -build -cook -stage -pak -package -archive "-archivedirectory=$OUTPUT_DIR/Archive" \
  -utf8output -unattended -stdout -FullStdOutLogOutput >"$LOG" 2>&1; then
  tail -n 100 "$LOG" >&2 || true
  die "Unreal iOS package failed. Review $LOG."
fi

IPA_CANDIDATES=()
while IFS= read -r candidate; do IPA_CANDIDATES+=("$candidate"); done < <(find "$OUTPUT_DIR/Archive" "$REVISION_ROOT/Game/Binaries/IOS" -type f -name '*.ipa' -newer "$BUILD_MARKER" -print 2>/dev/null || true)
[[ "${#IPA_CANDIDATES[@]}" -eq 1 ]] || die "expected one new IPA after UAT, found ${#IPA_CANDIDATES[@]}. Review $LOG."
IPA="${IPA_CANDIDATES[0]}"
unzip -q "$IPA" -d "$TEMP_DIR/unpacked" || die 'the generated IPA is not a readable ZIP archive.'
APP_PATH="$(find "$TEMP_DIR/unpacked/Payload" -maxdepth 1 -type d -name '*.app' -print -quit 2>/dev/null || true)"
[[ -n "$APP_PATH" && -f "$APP_PATH/Info.plist" ]] || die 'the IPA has no Payload/*.app/Info.plist.'
PACKAGE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP_PATH/Info.plist" 2>/dev/null || true)"
[[ "$PACKAGE_ID" == "$BUNDLE_ID" ]] || die 'the generated IPA bundle identifier does not match IOSEngine.ini.'
codesign --verify --deep --strict "$APP_PATH" >/dev/null 2>&1 || die 'the generated IPA app signature is invalid.'

FINAL_IPA="$OUTPUT_DIR/Chronicles-iOS-arm64-Development-v0.9.ipa"
cp -f "$IPA" "$FINAL_IPA"
shasum -a 256 "$FINAL_IPA"
printf 'iOS Development IPA created: %s\n' "$FINAL_IPA"
printf 'This is a signed sideload/testing build, not an App Store release. No device was installed or launched by this script.\n'
