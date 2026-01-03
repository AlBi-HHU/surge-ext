#!/usr/bin/env bash
#
# setup.sh
# Downloads nauty, configures, and builds it.
# Usage:
#   ./setup.sh [-v VERSION] [-d DIR] [--force] [--insecure]
#
# Examples:
#   ./setup.sh
#   ./setup.sh -v 2_9_0 -d /opt
#   ./setup.sh -v 2_9_0 --sha256 abcdef... -j 8
#   ./setup.sh --force   # forces rebuild / make clean
#   ./setup.sh --insecure # download without SSL verification
#
set -eu

### ────────────────────────────── Styling / Banner ──────────────────────────────
is_tty() { [[ -t 1 ]] && [[ -n "${TERM-}" ]] && [[ "${TERM}" != "dumb" ]]; }

if is_tty; then
  C_RESET=$'\033[0m'; C_DIM=$'\033[2m'
  C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YEL=$'\033[33m'; C_BLU=$'\033[34m'; C_CYN=$'\033[36m'
else
  C_RESET=""; C_DIM=""; C_RED=""; C_GRN=""; C_YEL=""; C_BLU=""; C_CYN=""
fi

banner() {
  cat <<'ASCII'
.__   __.      ___      __    __  .___________.____    ____
|  \ |  |     /   \    |  |  |  | |           |\   \  /   /
|   \|  |    /  ^  \   |  |  |  | `---|  |----` \   \/   /
|  . `  |   /  /_\  \  |  |  |  |     |  |       \_    _/
|  |\   |  /  _____  \ |  `--'  |     |  |         |  |
|__| \__| /__/     \__\ \______/      |__|         |__|
                                                                                                                                              

    Nauty Fetch & Build Script
    Version: 1.0.0
    © 2025 Federico Nellen


ASCII
}

log()  { printf "%s%s%s\n" "${C_DIM}" "$*" "${C_RESET}"; }
ok()   { printf "%s✔ %s%s\n" "${C_GRN}" "$*" "${C_RESET}"; }
warn() { printf "%s⚠ %s%s\n" "${C_YEL}" "$*" "${C_RESET}"; }
err()  { printf "%s✖ %s%s\n" "${C_RED}" "$*" "${C_RESET}" >&2; }

VERSION="2_9_0"                # Default version
PARENT_DIR="$(pwd)"            # Target directory to unpack into
FORCE=0
INSECURE=0                     # Skip TLS certificate check during download

usage() {
  cat <<EOF
${C_BLU}Nauty Fetch & Build${C_RESET}

Nutzung: $(basename "$0") [-v VERSION] [-d DIR] [--force] [--insecure]

Optionen:
  -v, --version    Nauty-Version (Standard: ${VERSION})
  -d, --dir        Zielverzeichnis (Standard: aktuelles Verzeichnis)
      --force      Neu-Build erzwingen (führt ggf. 'make clean' aus)
      --insecure   TLS-Zertifikatsprüfung beim Download überspringen
  -h, --help       Hilfe anzeigen
EOF
}

# --- Parse arguments ---
while [ $# -gt 0 ]; do
  case "$1" in
    -v|--version) VERSION="$2"; shift 2;;
    -d|--dir)     PARENT_DIR="$2"; shift 2;;
    --force)      FORCE=1; shift 1;;
    --insecure)   INSECURE=1; shift 1;;
    -h|--help) usage; exit 0;;
    *) err "Unknown option: $1"; usage; exit 2;;
  esac
done

### ────────────────────────────── Helpers ───────────────────────────────────────
have() { command -v "$1" >/dev/null 2>&1; }

download() {
  URL="$1"
  OUT="$2"
  CURL_OPTS="-L --fail"
  WGET_OPTS=""
  if [ "$INSECURE" -eq 1 ]; then
    log "WARNUNG: --insecure ist aktiv – TLS-Zertifikatsprüfung wird übersprungen." >&2
    CURL_OPTS="$CURL_OPTS -k"
    WGET_OPTS="--no-check-certificate"
  fi
  if have curl; then
      curl $CURL_OPTS -o "$OUT" "$URL"
  elif have wget; then
      wget $WGET_OPTS -O "$OUT" "$URL"
  else
    err "Weder curl noch wget vorhanden – bitte installieren." >&2
    exit 3
  fi
}


# --- Paths / filenames ---
ARCHIVE="nauty${VERSION}.tar.gz"
URL="https://pallini.di.uniroma1.it/${ARCHIVE}"
DEST_DIR="${PARENT_DIR}/nauty"

# --- Preparation ---
mkdir -p "$PARENT_DIR"
cd "$PARENT_DIR"

printf "%s" "${C_CYN}"; banner; printf "%s" "${C_RESET}"
log "==> Zielordner:       $PARENT_DIR"
log "==> Version:          $VERSION"
log "==> Download-URL:     $URL"
log "==> Entpacken nach:   $DEST_DIR"

# If archive is missing -> download
if [ ! -f "$ARCHIVE" ]; then
  log "==> Lade Archiv..."
  download "$URL" "$ARCHIVE"
else
  ok "==> Archiv bereits vorhanden: $ARCHIVE"
fi

# Extract (if directory not present yet)
if [ ! -d "$DEST_DIR" ]; then
  log "==> Verzeichnis existiert nicht, entpacke Archiv..."
  mkdir -p "$DEST_DIR"
  log "==> Entpacke Archiv..."
  tar xzf "$ARCHIVE" -C "${DEST_DIR}" --strip-components=1
else
  ok "==> Verzeichnis existiert bereits: $DEST_DIR"
fi

# Build
cd "$DEST_DIR"

if [ $FORCE -eq 1 ]; then
  log "==> --force aktiv: make clean"
  make clean || true
fi

# If library already exists and FORCE is not set -> do nothing
if [ -f "nautyW1.a" ] && [ $FORCE -eq 0 ]; then
  ok "==> nautyW1.a existiert bereits – überspringe Build. (Nutze --force für Neu-Build.)"
else
  log "==> Führe ./configure aus..."
  ./configure
  log "==> make"
  make
fi

# Summary
ABS_DIR="$(pwd)"
if [ ! -f "$ABS_DIR/nautyW1.a" ]; then
  err "Fehler: $ABS_DIR/nautyW1.a wurde nicht erzeugt." >&2
  exit 4
fi

cat <<EOF

========================================
Nauty erfolgreich gebaut.

NAUTY_HOME: $ABS_DIR
NAUTY_LIB:  $ABS_DIR/nautyW1.a

EOF

ok "Nauty Build abgeschlossen."

exit 0
