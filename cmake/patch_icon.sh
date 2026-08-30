#!/bin/sh
# Patch CFBundleIconFile in the built app's Info.plist.
# Usage: patch_icon.sh <path-to-Info.plist>
PLIST="$1"
/usr/libexec/PlistBuddy -c "Set :CFBundleIconFile app" "$PLIST" 2>/dev/null || \
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string app" "$PLIST"
