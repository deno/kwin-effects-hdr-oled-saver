#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 3 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ || ! $2 =~ ^[1-9][0-9]*$ ]]; then
    echo "Usage: $0 VERSION PPA_REVISION OUTPUT_DIRECTORY" >&2
    exit 2
fi

version=$1
revision=$2
output_dir=$(realpath -m "$3")
repo_root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
package=kwin-effects-hdr-oled-saver
package_version="${version}-1ppa${revision}~ubuntu26.10.1"
mkdir -p "$output_dir"
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

# Rebuilds of the same tag must reuse the identical upstream tarball.
git -C "$repo_root" archive --format=tar --prefix="${package}-${version}/" HEAD -- \
    . ':(exclude)debian' ':(exclude).github' ':(exclude).obs' \
    | gzip -n > "$output_dir/${package}_${version}.orig.tar.gz"
cp "$output_dir/${package}_${version}.orig.tar.gz" "$work_dir/"
tar -xzf "$work_dir/${package}_${version}.orig.tar.gz" -C "$work_dir"
source_dir="$work_dir/${package}-${version}"
cp -a "$repo_root/debian" "$source_dir/debian"
chmod +x "$source_dir/debian/rules"

cd "$source_dir"
# Preserve the release notes while deriving upload versions from the tag.
changelog_version=$(dpkg-parsechangelog -S Version)
if [[ $changelog_version != "$version-"* ]]; then
    echo "Debian changelog version $changelog_version does not match release $version" >&2
    exit 1
fi
dch --maintmaint --newversion "$package_version" --distribution stonking --force-distribution \
    "Launchpad build for Ubuntu 26.10 (PPA revision $revision)."
# Source-only builds do not need KWin headers or execute the binary clean target.
dpkg-buildpackage -S -d -nc -us -uc -sa
cp "$work_dir"/*.debian.tar.* "$work_dir"/*.dsc \
    "$work_dir"/*_source.buildinfo "$work_dir"/*_source.changes "$output_dir/"
printf 'Source package: %s/%s_%s_source.changes\n' "$output_dir" "$package" "$package_version"
