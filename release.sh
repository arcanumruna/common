#\!/usr/bin/env bash
# Usage: ./release.sh [major|minor|patch]
set -euo pipefail

RELEASE_TYPE="${1:?Usage: ./release.sh [major|minor|patch]}"
VERSION=$(< VERSION.txt)

if [[ \! $VERSION =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)-SNAPSHOT$ ]]; then
  echo "❌ VERSION.txt must be X.Y.Z-SNAPSHOT, got: $VERSION"
  exit 1
fi

MAJOR="${BASH_REMATCH[1]}"
MINOR="${BASH_REMATCH[2]}"
PATCH="${BASH_REMATCH[3]}"
RELEASE_VERSION="$MAJOR.$MINOR.$PATCH"

case "$RELEASE_TYPE" in
  major) NEXT="$((MAJOR + 1)).0.0-SNAPSHOT" ;;
  minor) NEXT="$MAJOR.$((MINOR + 1)).0-SNAPSHOT" ;;
  patch) NEXT="$MAJOR.$MINOR.$((PATCH + 1))-SNAPSHOT" ;;
  *) echo "❌ Invalid release type: $RELEASE_TYPE (use major|minor|patch)"; exit 1 ;;
esac

echo "📦 Releasing: $VERSION → $RELEASE_VERSION"
echo "📝 Next development version: $NEXT"

# 1. Set release version
echo "$RELEASE_VERSION" > VERSION.txt
git add VERSION.txt
git commit -m "release: $RELEASE_VERSION"

# 2. Tag
git tag -a "v$RELEASE_VERSION" -m "Release $RELEASE_VERSION"

# 3. Set next SNAPSHOT
echo "$NEXT" > VERSION.txt
git add VERSION.txt
git commit -m "chore: set next development version $NEXT"

# 4. Push commits + tag
git push origin HEAD
git push origin "v$RELEASE_VERSION"

echo "✅ Released $RELEASE_VERSION, next development version: $NEXT"
echo "🏷️  Tag: v$RELEASE_VERSION"

