#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Generate .SRCINFO
makepkg --printsrcinfo > .SRCINFO

REPO_DIR="/tmp/aur-hyprland-share-picker-preview-git"

# Clone fresh each time
rm -rf "$REPO_DIR"
git clone ssh://aur@aur.archlinux.org/hyprland-share-picker-preview-git.git "$REPO_DIR"
cd "$REPO_DIR"
git checkout -B master

# Copy files and push
cp "$SCRIPT_DIR/PKGBUILD" "$SCRIPT_DIR/.SRCINFO" .
git add PKGBUILD .SRCINFO
git commit -m "Update to $(grep pkgver= PKGBUILD | head -1 | cut -d= -f2)" || echo "Nothing to commit"
git push origin master
echo "Published!"
