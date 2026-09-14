#!/bin/sh
# Rewrite Exec/TryExec in the installed desktop entry to the absolute
# launcher path for the PREFIX used at install time.
# Usage: fix-desktop-exec.sh <desktop-file> <prefix>
set -eu
file="$1"
prefix="$2"
sed -i "s|^Exec=.*|Exec=${prefix}/bin/hyprplay|;s|^TryExec=.*|TryExec=${prefix}/bin/hyprplay|" "$file"
