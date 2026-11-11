# Distributing MolTUI

This directory contains packaging definitions for distributing **MolTUI** across **conda-forge**, the **Arch User Repository (AUR)**, and **Homebrew**, paired with automated GitHub Releases.

## 1. Cut a GitHub Release First

All downstream package managers pull from a tagged GitHub release (for example `v0.1.0`):

```bash
git tag v0.1.0
git push origin v0.1.0
```

[`.github/workflows/release.yml`](../.github/workflows/release.yml) automatically builds self-contained `.tar.gz` archives for Linux (`x86_64`, `aarch64`) and macOS (`arm64`, `x86_64`) along with `SHA256SUMS.txt` and publishes the GitHub Release.

Compute the source tarball SHA256 for recipes:

```bash
curl -sL https://github.com/Parzival1918/moltui/archive/refs/tags/v0.1.0.tar.gz | sha256sum
```

---

## 2. conda-forge (`pixi global`, `conda`, `mamba`)

[`conda-forge/recipe.yaml`](conda-forge/recipe.yaml) uses the v1 `rattler-build` format accepted by `conda-forge/staged-recipes`.

1. Fork [`conda-forge/staged-recipes`](https://github.com/conda-forge/staged-recipes).
2. Copy `packaging/conda-forge/recipe.yaml` to `recipes/moltui/recipe.yaml` in your fork.
3. Replace `REPLACE_WITH_RELEASE_TARBALL_SHA256` with the source tarball SHA256.
4. Open a PR to `conda-forge/staged-recipes`. Once merged, users can install MolTUI globally with:

```bash
pixi global install moltui
# or
conda install -c conda-forge moltui
```

---

## 3. Arch User Repository (AUR)

Two `PKGBUILD` templates are provided:

- [`aur/moltui-bin/PKGBUILD`](aur/moltui-bin/PKGBUILD): Installs the prebuilt GitHub Release binary (fastest install; zero extra AUR dependencies).
- [`aur/moltui/PKGBUILD`](aur/moltui/PKGBUILD): Builds from source using Arch `extra` packages (`glm`, `cli11`) and `CMake` `FetchContent` for `gemmi` and `ftxui`.

To publish either package to AUR:

```bash
cd packaging/aur/moltui-bin   # or packaging/aur/moltui
updpkgsums                    # updates sha256sums from the release URL
makepkg --printsrcinfo > .SRCINFO
# Test build locally:
makepkg -si
```

Then push `PKGBUILD` and `.SRCINFO` to `ssh://aur@aur.archlinux.org/moltui-bin.git` (or `moltui.git`).

---

## 4. Homebrew (macOS & Linux)

[`homebrew/moltui.rb`](homebrew/moltui.rb) is ready for a personal tap repository (for example `Parzival1918/homebrew-tap` at `Formula/moltui.rb`):

1. Replace `REPLACE_WITH_RELEASE_TARBALL_SHA256` with the tag tarball SHA256.
2. Users can then install via:

```bash
brew install Parzival1918/tap/moltui
```
