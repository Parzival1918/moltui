# MolTUI 🧪✨

A high-performance 3D Molecular and Crystal Structure visualizer in your terminal, written in modern C++20.

Inspired by [chmpy](https://github.com/peterspackman/chmpy), **MolTUI** combines the crystallographic power of **Gemmi**, the UI architecture of **FTXUI**, and a **custom software rasterizer** featuring analytical sphere impostors and universal 24-bit TrueColor ANSI half-block (`▀`) rendering.

---

## Features

- **Crystal & Space Group Support (via Gemmi)**:
  - Load CIF, mmCIF, PDB, SHELX (`.res`/`.ins`), VASP (`POSCAR`/`CONTCAR`), and XYZ files.
  - Automatic symmetry expansion from space group operations.
  - Unit cell wireframe box with color-coded crystallographic axes ($a$ in Red, $b$ in Green, $c$ in Blue).
  - Miller slicing plane visualization (e.g. `:hkl 1 1 1`).
- **Periodic Boundary Unwrapping ("Make Whole")**:
  - Automatically reconstruct connected molecules split across unit cell faces via graph breadth-first search and the minimum image convention (`-u` flag or hotkey `u` / `:unwrap`).
- **Supercell Expansion**:
  - Dynamically tile crystals in 3D (e.g. `--cell 2 2 2` or `:cell 2 2 2`).
- **Interactive Geometric Measurements**:
  - Click on 1 atom: Element, Cartesian coordinates, Fractional coordinates, Occupancy, B-factor.
  - Click on 2 atoms: Distance ($d$ in Å).
  - Click on 3 atoms: Bond angle ($\theta$ in degrees).
  - Click on 4 atoms: Dihedral / Torsion angle ($\phi$ in degrees).
- **Fast 3D Software Rasterizer**:
  - Analytical sphere impostors with per-pixel ray-sphere intersection, Z-buffer, and Phong lighting.
  - Multiple representation styles: `Ball-and-stick`, `Space-filling (VDW)`, `Licorice`, and `Wireframe`.
- **Reproducible Build Environment**:
  - Fully managed with **Pixi** for cross-platform reproducibility.

---

## Getting Started

### Prerequisites

Make sure you have [Pixi](https://pixi.sh) installed:
```bash
curl -fsSL https://pixi.sh/install.sh | bash
```

### Build & Run

Clone the repository and build:
```bash
pixi run build
```

Run tests:
```bash
pixi run test
```

Launch the interactive viewer:
```bash
# View an organic molecule
pixi run moltui tests/data/caffeine.xyz

# View a crystal structure
pixi run moltui tests/data/nacl.cif

# View a crystal with unwrapped whole molecules across periodic boundaries
pixi run moltui tests/data/nacl.cif --unwrap

# View a 2x2x2 supercell
pixi run moltui tests/data/nacl.cif --cell 2 2 2

# Print a quick ANSI snapshot directly to stdout
pixi run moltui tests/data/caffeine.xyz --snapshot
```

---

## Interactive Controls

### Mouse
| Action | Description |
|---|---|
| **Left-Click + Drag** | Arcball virtual trackball 3D rotation |
| **Right-Click / Shift + Drag** | Screen pan |
| **Scroll Wheel** | Zoom in / out |
| **Left-Click on Atom** | Atom picking & geometric measurement |

### Keybindings
| Key | Action |
|---|---|
| `u` | Toggle periodic boundary unwrapping (**Make Whole**) |
| `s` | Cycle rendering style (Ball-and-stick $\to$ VDW $\to$ Licorice $\to$ Wireframe) |
| `c` | Toggle unit cell wireframe box |
| `b` | Toggle bonds |
| `m` | Clear active measurements |
| `r` | Reset camera view |
| `Space` | Play / Pause multi-frame trajectory |
| `[` / `]` | Previous / Next trajectory frame |
| `:` | Enter command mode |
| `h` or `?` | Toggle help overlay |
| `q` or `Esc` | Quit |

### Command Mode (`:`)
| Command | Description |
|---|---|
| `:load <filepath>` | Load a new molecule or crystal |
| `:cell <nx> <ny> <nz>` | Expand crystal into an $N_x \times N_y \times N_z$ supercell |
| `:unwrap` / `:wrap` | Toggle PBC molecule unwrapping |
| `:style <ball\|vdw\|licorice\|wire>` | Set display style |
| `:hkl <h> <k> <l>` | Display Miller lattice plane |
| `:clear` | Clear selections and Miller planes |
| `:reset` | Reset camera to center on structure |
| `:q` / `:quit` | Exit MolTUI |
