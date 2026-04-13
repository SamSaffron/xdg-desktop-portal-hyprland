#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Generate .SRCINFO
makepkg --printsrcinfo > .SRCINFO

REPO_DIR="/tmp/aur-hyprland-share-picker-preview-git"

# Clone or update the AUR repo
if [ -d "$REPO_DIR" ]; then
  cd "$REPO_DIR"
  git pull --rebase
else
  git clone ssh://aur@aur.archlinux.org/hyprland-share-picker-preview-git.git "$REPO_DIR"
  cd "$REPO_DIR"
fi

# Copy files and push
cp "$SCRIPT_DIR/PKGBUILD" "$SCRIPT_DIR/.SRCINFO" .
git add PKGBUILD .SRCINFO
git commit -m "Update to $(grep pkgver= PKGBUILD | head -1 | cut -d= -f2)" || echo "Nothing to commit"
git push
echo "Published!"
