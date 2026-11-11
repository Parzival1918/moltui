#include "moltui/core/structure.hpp"
#include "moltui/io/loader.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/rasterizer.hpp"
#include "moltui/render/protocol.hpp"
#include "moltui/ui/app.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    CLI::App app{"MolTUI: High-Performance Terminal Molecular & Crystal Visualizer"};

    std::string filepath;
    app.add_option("file", filepath, "Structure file (CIF, mmCIF, PDB, POSCAR, XYZ, RES)")
       ->check(CLI::ExistingFile);

    bool unwrap = false;
    app.add_flag("-u,--unwrap", unwrap, "Unwrap molecules across periodic boundary conditions (Make Whole)");

    std::string style = "ball";
    app.add_option("-s,--style", style, "Rendering style: ball, vdw, licorice, wire");

    std::vector<int> supercell;
    app.add_option("--cell,--supercell", supercell, "Supercell expansion (e.g. --supercell 2 2 2)")
       ->expected(3);

    bool hide_cell = false;
    app.add_flag("--no-cell", hide_cell, "Hide unit cell box wireframe");

    bool print_snapshot = false;
    app.add_flag("-p,--print,--snapshot", print_snapshot, "Print rendered snapshot to stdout and exit");

    std::string render_mode = "auto";
    app.add_option("-m,--mode,--render", render_mode,
                   "Rendering mode: auto, kitty, half-blocks, ascii");

    bool adaptive_quality = true;
    app.add_flag("--draft,!--no-draft", adaptive_quality,
                 "Lower resolution while rotating/displacing for smoother motion (default: on)");

    CLI11_PARSE(app, argc, argv);

    auto parsed_mode = moltui::render::ProtocolRenderer::parse_mode(render_mode);
    if (!parsed_mode.has_value()) {
        std::cerr << "MolTUI Error: Unknown rendering mode '" << render_mode
                  << "'. Valid options: auto, kitty, half-blocks, ascii\n";
        return 1;
    }
    moltui::render::TerminalGraphicsMode graphics_mode = *parsed_mode;

    auto apply_rasterizer_options = [&](moltui::render::Rasterizer& rast) {
        if (style == "vdw" || style == "space") {
            rast.settings().style = moltui::render::RenderStyle::SpaceFilling;
        } else if (style == "licorice" || style == "stick") {
            rast.settings().style = moltui::render::RenderStyle::Licorice;
        } else if (style == "wire" || style == "wireframe") {
            rast.settings().style = moltui::render::RenderStyle::Wireframe;
        } else {
            rast.settings().style = moltui::render::RenderStyle::BallAndStick;
        }
        if (hide_cell) {
            rast.settings().show_unit_cell = false;
        }
    };

    try {
        std::vector<moltui::core::Structure> structures;
        if (!filepath.empty()) {
            structures = moltui::io::StructureLoader::load_structures(filepath);
        } else {
            // Default demo molecule if no file is passed: create a water molecule
            moltui::core::Structure st;
            st.title = "H2O (Demo Molecule)";
            st.atoms = {
                {0, "O", "O1", {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
                {1, "H", "H1", {0.757f, 0.586f, 0.0f}, {0.0f, 0.0f, 0.0f}},
                {2, "H", "H2", {-0.757f, 0.586f, 0.0f}, {0.0f, 0.0f, 0.0f}}
            };
            st.compute_bonds(0.28f, false);
            structures.push_back(std::move(st));
        }

        for (auto& st : structures) {
            if (supercell.size() == 3) {
                st = st.generate_supercell(supercell[0], supercell[1], supercell[2]);
            }
            if (unwrap && st.is_periodic() && !st.is_molecular_crystal) {
                st.unwrap_pbc();
            }
        }

        if (print_snapshot) {
            const auto& st = structures.front();
            int snap_w = 80;
            int snap_h = 50;
            if (graphics_mode == moltui::render::TerminalGraphicsMode::KittyGraphics) {
                snap_w = 640;
                snap_h = 400;
            }

            moltui::render::Rasterizer rast;
            apply_rasterizer_options(rast);

            moltui::render::Camera cam;
            cam.set_viewport(snap_w, snap_h);
            const auto& b = st.get_bounds();
            glm::vec3 com = st.center_of_mass();
            cam.reset(com, b.radius);

            std::vector<glm::vec3> pts;
            std::vector<float> radii;
            for (const auto& atom : st.atoms) {
                pts.push_back(atom.pos);
                radii.push_back(0.4f);
            }
            if (rast.settings().show_unit_cell && st.unit_cell.has_value()) {
                for (int i = 0; i < 8; ++i) {
                    glm::vec3 frac((i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f, (i & 4) ? 1.0f : 0.0f);
                    pts.push_back(st.unit_cell->fractional_to_cartesian(frac));
                    radii.push_back(0.2f);
                }
            }
            cam.fit_extent(com, pts, radii, false);

            moltui::render::Framebuffer fb(snap_w, snap_h);
            rast.render(st, cam, fb);
            switch (graphics_mode) {
                case moltui::render::TerminalGraphicsMode::KittyGraphics:
                    std::cout << moltui::render::ProtocolRenderer::render_kitty(fb);
                    break;
                case moltui::render::TerminalGraphicsMode::Ascii:
                    std::cout << moltui::render::ProtocolRenderer::render_ascii(fb);
                    break;
                case moltui::render::TerminalGraphicsMode::Sixel:
                    std::cout << moltui::render::ProtocolRenderer::render_sixel(fb);
                    break;
                case moltui::render::TerminalGraphicsMode::HalfBlockAnsi:
                default:
                    std::cout << moltui::render::ProtocolRenderer::render_half_blocks(fb);
                    break;
            }
            return 0;
        }

        moltui::ui::App ui_app(std::move(structures), graphics_mode);
        ui_app.set_adaptive_quality(adaptive_quality);
        apply_rasterizer_options(ui_app.rasterizer());
        ui_app.run();

    } catch (const std::exception& e) {
        std::cerr << "MolTUI Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
