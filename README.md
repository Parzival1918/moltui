# MolTUI

A fast, interactive 3D molecular and crystal structure viewer for the terminal, written in modern C++20.

Strongly inspired by **[chmpy](https://github.com/peterspackman/chmpy)** (`chmpy.tui`), **MolTUI** brings crystal symmetry expansion, periodic boundary unwrapping, publication-style 3D rendering, and interactive geometry measurements into a native C++ terminal application powered by **Gemmi**, **FTXUI**, **GLM**, and a custom software rasterizer.

---

## Features

- **Multiple Terminal Rendering Protocols**:
  - **`auto` (default)**: Automatically selects the **Kitty Graphics Protocol** in Kitty, Ghostty, or WezTerm, and falls back to universal 24-bit TrueColor ANSI half-blocks elsewhere.
  - **`kitty`**: Crisp high-DPI 24-bit RGB pixel rendering with synchronized terminal updates and button-event tracking to eliminate hover flicker.
  - **`half-blocks`**: Universal TrueColor ANSI rendering using `▀` upper half-block characters.
  - **`ascii`**: Monochrome luminance ramp (`.:-=+*#%@`) for plain-text or piped output.
- **Custom 3D Software Rasterizer**:
  - Analytical ray-intersected sphere impostors and 3D cylinder bonds with per-pixel Z-buffering and pixel-accurate atom picking.
  - **4 Visual Styles**: `Ball-and-stick` (`ball`), `Space-filling / VDW` (`vdw`), `Licorice` (`licorice`), and `Wireframe` (`wire`).
  - **2 Surface Shading Modes**: `Lit` (Blinn-Phong with specular damping on bright colors) and `Flat` (rim-darkened flat shading).
  - **Aerial-Perspective Depth Cueing** and **Orthographic** (default) / **Perspective** projections.
  - **Adaptive Draft Quality** (on by default): Halves pixel resolution during active rotation, displacement, or zoom for smooth motion in Kitty mode, then refines cleanly when idle.
  - **COM-Centered Rotation**: Orbiting rotates around the structure's center of mass even after panning/displacing it on screen.
- **Crystallography, Symmetry & Periodic Boundaries (via Gemmi)**:
  - Loads **CIF** (including multi-block CIF files), **mmCIF**, **PDB** / **ENT** (including multi-model ensembles), **SHELX** (`.res`/`.ins`), **VASP** (`POSCAR`/`CONTCAR`), and **XYZ** (including multi-frame trajectories), or multiple files at once.
  - Automatic space-group symmetry expansion, periodic covalent bonding, and PBC molecule unwrapping (**Make Whole**).
  - **Supercell Expansion**: Expand crystals into arbitrary $N_x \times N_y \times N_z$ supercells (`--cell` / `--supercell` / `:cell`). Subcell wireframe edges are drawn for every cell in the grid, while the solid 3D cylinder + cone-arrowhead axes (**a** in Red, **b** in Green, **c** in Blue) are drawn strictly for the origin cell alongside a title-bar `Cell: ■a ■b ■c` legend.
  - **Miller Lattice Planes**: Visualize arbitrary $(hkl)$ planes inside the unit cell (`--hkl` / `:hkl`).
  - **Axis & Lattice Alignment**: Instant camera alignment along Cartesian (`x`, `y`, `z`), direct lattice (`a`, `b`, `c`, `[uvw]`), or reciprocal lattice (`a*`, `b*`, `c*`, `(hkl)`) directions.
- **Interactive Geometric Measurements**:
  - Select up to 4 atoms via mouse click, CLI `--select`, or `:select` / `:measure`:
    - **1 atom**: Element, label, Cartesian coordinates (Å), fractional coordinates (when periodic), B-factor, and charge.
    - **2 atoms**: Interatomic distance (Å).
    - **3 atoms**: Bond angle (°).
    - **4 atoms**: Dihedral / torsion angle (°).
- **Non-Interactive Snapshot Mode (`-p` / `--snapshot`)**:
  - Render single frames, specific ensemble indices, or all frames directly to stdout with configurable rendering protocol, style, camera orientation, zoom, pan, atom selection, and target pixel or column dimensions.

---

## Getting Started

### Prerequisites

Install [Pixi](https://pixi.sh):
```bash
curl -fsSL https://pixi.sh/install.sh | bash
```

### Build & Test

```bash
# Configure and build (Release with Ninja via Pixi)
pixi run build

# Run unit tests
pixi run test
```

You can also invoke `./build/moltui` directly after building, or run through `pixi run moltui`.

### Quick Examples

```bash
# Launch interactive viewer (opens a demo H2O molecule if no file is given)
pixi run moltui tests/data/caffeine.xyz

# View a crystal structure or multi-block CIF
pixi run moltui tests/data/nacl.cif

# View an unwrapped molecular crystal as a 2x2x2 supercell along the b axis
pixi run moltui tests/data/nacl.cif --unwrap --cell 2 2 2 -v b

# Force a specific terminal graphics protocol
pixi run moltui tests/data/caffeine.xyz -m kitty
pixi run moltui tests/data/caffeine.xyz -m half-blocks

# Print a 100-column snapshot directly to stdout and exit
pixi run moltui tests/data/nacl.cif --cell 2 2 2 -p --cols 100 -s ball --shading lit
```

---

## CLI Reference

```
moltui [OPTIONS] [files...]
```

### Visual Styles & Crystal Options
| Flag | Description |
|---|---|
| `-s, --style <mode>` | Rendering style: `ball`, `vdw`, `licorice`, `wire` |
| `--shading <mode>` | Surface shading: `lit` (Blinn-Phong) or `flat` (rim-darkened) |
| `--depth-cue` / `--no-depth-cue` | Toggle aerial-perspective depth fading (default: on) |
| `--bonds` / `--no-bonds` | Show or hide bonds (default: on) |
| `--unitcell` / `--no-cell` / `--no-unitcell` | Show or hide unit cell box and axes (default: on) |
| `--hkl <h> <k> <l>` | Display Miller lattice plane `(h k l)` |
| `-u, --unwrap` | Unwrap molecules across periodic boundaries (Make Whole) |
| `-w, --wrap` | Wrap coordinates back into the primary `[0, 1)` unit cell |
| `--cell, --supercell <nx> <ny> <nz>` | Expand crystal into an $N_x \times N_y \times N_z$ supercell |

### Camera, Trajectory & Selection
| Flag | Description |
|---|---|
| `-v, --view, --direction <expr>` | Aim along `x`, `y`, `z`, `a`, `b`, `c`, `a*`, `b*`, `c*`, `[uvw]`, `(hkl)` (prefix `-` to flip) |
| `--ortho` / `--persp` | Orthographic (`--ortho`, default) or perspective (`--persp`) projection |
| `--rotate <dx> <dy>` | Initial camera rotation around COM in step units |
| `--pan <dx> <dy>` | Initial screen displacement |
| `-z, --zoom <factor>` | Zoom magnification (`2.0` = closer, `0.5` = farther) |
| `-f, --frame <index\|all>` | 1-based structure/frame index, or `all` |
| `--all-frames` | Render all structures/frames in `-p` snapshot mode |
| `--select <id...>` | Highlight up to four 1-based atom IDs |

### Rendering Mode & Snapshot Output
| Flag | Description |
|---|---|
| `-m, --mode, --render <mode>` | Graphics protocol: `auto`, `kitty`, `half-blocks`, `ascii` |
| `--draft` / `--no-draft` | Adaptive draft resolution while moving in TUI (default: on) |
| `-p, --print, --snapshot` | Render snapshot(s) to stdout and exit |
| `-W, --width <n>` | Snapshot width (cells for `half-blocks`/`ascii`, pixels for `kitty`) |
| `-H, --height <n>` | Snapshot height (cells for `half-blocks`/`ascii`, pixels for `kitty`) |
| `--cols <n>` | Target snapshot width in terminal columns (auto-calculates height) |

---

## Interactive Controls

### Mouse
| Action | Description |
|---|---|
| **Left-Click + Drag** | Rotate around structure center of mass (COM) |
| **Right-Click / Shift + Drag** | Displace structure on screen (pan) |
| **Scroll Wheel** | Zoom in / out |
| **Left-Click on Atom** | Pick / unpick atom for measurements (click background to clear) |

### Keybindings
| Key | Action |
|---|---|
| `h` / `j` / `k` / `l` | Rotate around COM (left / down / up / right) |
| `Shift + h/j/k/l` (`H`/`J`/`K`/`L`) | Displace structure on screen (pan left / down / up / right) |
| `x` / `y` / `z` | View along Cartesian `x` / `y` / `z` (press again to flip `+`/`-`) |
| `1` / `2` / `3` | View along lattice `a` / `b` / `c` (falls back to `x`/`y`/`z` if non-periodic; repeat flips) |
| `+` / `=` and `-` / `_` | Zoom in / out |
| `s` | Cycle rendering style (`Ball-and-stick` $\to$ `VDW` $\to$ `Licorice` $\to$ `Wireframe`) |
| `f` | Toggle shading (`Lit` Blinn-Phong $\leftrightarrow$ `Flat` rim) |
| `o` | Toggle projection (`Orthographic` $\leftrightarrow$ `Perspective`) |
| `d` | Toggle adaptive draft quality while moving |
| `u` | Toggle periodic boundary unwrapping (**Make Whole**) |
| `c` | Toggle unit cell box and axis arrows |
| `b` | Toggle bonds |
| `m` | Clear active atom measurements |
| `r` | Reset camera view and re-fit structure |
| `Space` | Play / pause multi-frame trajectory |
| `[` / `]` or `p` / `n` | Previous / next frame or CIF structure |
| `:` | Enter command mode (`Up` / `Down` recalls command history) |
| `?` or `F1` | Toggle quick-reference help overlay |
| `q` or `Esc` | Quit |

### Command Mode (`:`)
All interactive actions and settings are also available through the `:` command bar:

| Command | Description |
|---|---|
| `:load <filepath>` | Load a new structure file |
| `:cell <nx> <ny> <nz>` | Expand crystal into an $N_x \times N_y \times N_z$ supercell (alias `:supercell`) |
| `:unwrap` / `:wrap` | Unwrap or wrap molecules across periodic boundaries |
| `:hkl <h> <k> <l>` / `:hkl off` | Show or hide a Miller lattice plane |
| `:view <expr>` / `:aim <expr>` | Aim along `x`, `y`, `z`, `a`, `b`, `c`, `a*`, `b*`, `c*`, `[uvw]`, `(hkl)` |
| `:x` / `:y` / `:z` / `:a` / `:b` / `:c` / `:a*` / `:b*` / `:c*` | Shorthand view commands (supports `+` / `-` prefix, e.g. `:-c`) |
| `:rotate <dx> <dy>` / `:rot` | Rotate camera around COM |
| `:pan <dx> <dy>` / `:move` / `:displace` | Displace structure on screen |
| `:zoom <factor>` | Zoom camera (e.g. `0.8` closer, `1.2` farther) |
| `:reset` / `:r` | Reset camera view |
| `:style [ball\|vdw\|licorice\|wire]` | Set or cycle rendering style |
| `:shading [lit\|flat]` | Set or toggle surface shading |
| `:ortho` / `:persp` / `:proj [ortho\|persp]` | Switch camera projection |
| `:mode [auto\|kitty\|half-blocks\|ascii]` | Switch or query terminal graphics protocol (alias `:render`) |
| `:draft [on\|off]` | Toggle or set adaptive draft quality (alias `:adaptive`) |
| `:unitcell [on\|off]` / `:bonds [on\|off]` | Toggle or set unit cell or bond visibility |
| `:frame <n>` / `:next` / `:prev` / `:play` / `:pause` | Navigate or play multi-structure/trajectory ensembles |
| `:select <id...>` / `:measure <id...>` | Select up to four 1-based atom IDs for measurement |
| `:clear` | Clear atom selections and Miller plane |
| `:help [on\|off]` / `:?` | Toggle help overlay |
| `:q` / `:quit` | Exit MolTUI |

---

## Acknowledgements

Special credit goes to **[chmpy](https://github.com/peterspackman/chmpy)** by Peter Spackman (`chmpy.tui`), whose terminal crystal visualization design, visual styles (ball-and-stick/VDW/licorice/wireframe, Blinn-Phong and rim shading, depth cueing, axis alignment views), and crystallographic PBC conventions directly inspired MolTUI.

MolTUI also builds on the following open-source C++ libraries:
- **[Gemmi](https://gemmi.readthedocs.io/)** — macromolecular and small-molecule crystallography parsing & symmetry operations
- **[FTXUI](https://github.com/ArthurSonzogni/FTXUI)** — C++ terminal user interface library
- **[GLM](https://github.com/g-truc/glm)** — OpenGL Mathematics for 3D transforms and projections
- **[CLI11](https://github.com/CLIUtils/CLI11)** — command-line argument parsing

---

## License

MolTUI is free software licensed under the **GNU General Public License v3.0 (GPLv3)**. See [LICENSE](LICENSE) for the full license text.
