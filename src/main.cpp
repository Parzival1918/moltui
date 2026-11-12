#include "moltui/core/structure.hpp"
#include "moltui/io/loader.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/rasterizer.hpp"
#include "moltui/render/protocol.hpp"
#include "moltui/render/view_utils.hpp"
#include "moltui/ui/app.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

#ifndef MOLTUI_VERSION
#define MOLTUI_VERSION "0.1.0"
#endif

int main(int argc, char** argv) {
    CLI::App app{"MolTUI: High-Performance Terminal Molecular & Crystal Visualizer"};
    app.set_version_flag("-V,--version", MOLTUI_VERSION);
    app.validate_optional_arguments();

    std::vector<std::string> filepaths;
    app.add_option("files", filepaths, "Structure file(s) (CIF, mmCIF, PDB, POSCAR, XYZ, RES)")
       ->check(CLI::ExistingFile);

    // Visual styles & shading
    std::string style = "ball";
    app.add_option("-s,--style", style, "Rendering style: ball, vdw, licorice, wire");

    std::string shading = "lit";
    app.add_option("--shading", shading, "Surface shading: lit (Blinn-Phong), flat (rim)");

    bool depth_cue = true;
    app.add_flag("--depth-cue,!--no-depth-cue", depth_cue,
                 "Aerial-perspective depth fading (default: on)");

    bool show_bonds = true;
    app.add_flag("--bonds,!--no-bonds", show_bonds, "Show/hide bonds (default: on)");

    bool show_unit_cell = true;
    app.add_flag("--unitcell,!--no-cell,!--no-unitcell", show_unit_cell,
                 "Show/hide unit cell box and axes (default: on)");

    std::string color_mode = "element";
    app.add_option("--color", color_mode,
                   "Coloring mode: element, symop, asym, molecule");

    bool asym_only = false;
    app.add_flag("--asym,--asymmetric-unit", asym_only,
                 "Show only the crystallographic asymmetric unit");

    std::vector<int> hkl_plane;
    app.add_option("--hkl", hkl_plane, "Display Miller plane (e.g. --hkl 1 1 1)")
       ->expected(3)
       ->allow_extra_args(false);

    // Crystal & PBC
    bool unwrap = false;
    app.add_flag("-u,--unwrap", unwrap, "Unwrap molecules across periodic boundary conditions (Make Whole)");

    bool wrap = false;
    app.add_flag("-w,--wrap", wrap, "Wrap coordinates back into primary [0, 1) unit cell");

    std::vector<int> supercell;
    app.add_option("--cell,--supercell", supercell, "Supercell expansion (e.g. --supercell 2 2 2)")
       ->expected(3)
       ->allow_extra_args(false);

    // Camera & orientation
    std::string view_expr;
    app.add_option("-v,--view,--direction", view_expr,
                   "View direction: x, y, z, a, b, c, a*, b*, c*, [uvw], (hkl) (prefix - to flip)");

    bool orthographic = true;
    app.add_flag("--ortho,!--persp", orthographic,
                 "Orthographic (--ortho, default) or perspective (--persp) projection");

    std::vector<float> rotate_xy;
    app.add_option("--rotate", rotate_xy, "Rotate camera around COM in step units (dx dy)")
       ->expected(2)
       ->allow_extra_args(false);

    std::vector<float> pan_xy;
    app.add_option("--pan", pan_xy, "Displace structure on screen (dx dy)")
       ->expected(2)
       ->allow_extra_args(false);

    float zoom_mag = 1.0f;
    app.add_option("-z,--zoom", zoom_mag, "Zoom magnification (e.g. 2.0 = closer, 0.5 = farther)");

    // Frames / Ensemble / Selection
    std::string frame_spec;
    app.add_option("-f,--frame", frame_spec, "1-based structure/frame index, or 'all'");

    bool all_frames = false;
    app.add_flag("--all-frames", all_frames, "Render all structures/frames in -p snapshot mode");

    std::vector<size_t> select_ids;
    app.add_option("--select", select_ids, "Highlight 1-based atom IDs (up to 4)")
       ->check(CLI::PositiveNumber);

    // Output & Snapshot options
    bool print_snapshot = false;
    app.add_flag("-p,--print,--snapshot", print_snapshot, "Print rendered snapshot(s) to stdout and exit");

    std::string render_mode = "auto";
    app.add_option("-m,--mode,--render", render_mode,
                   "Rendering mode: auto, kitty, half-blocks, ascii");

    std::optional<int> width_opt;
    app.add_option("-W,--width", width_opt,
                   "Snapshot width (cells for half-blocks/ascii, pixels for kitty)");

    std::optional<int> height_opt;
    app.add_option("-H,--height", height_opt,
                   "Snapshot height (cells for half-blocks/ascii, pixels for kitty)");

    std::optional<int> cols_opt;
    app.add_option("--cols", cols_opt,
                   "Target snapshot width in terminal columns (auto-calculates height)");

    bool adaptive_quality = true;
    app.add_flag("--draft,!--no-draft", adaptive_quality,
                 "Lower resolution while rotating/displacing for smoother motion (default: on)");

    CLI11_PARSE(app, argc, argv);

    if (unwrap && wrap) {
        std::cerr << "MolTUI Error: --unwrap and --wrap cannot be used together.\n";
        return 1;
    }
    if (zoom_mag <= 0.0f) {
        std::cerr << "MolTUI Error: --zoom must be positive.\n";
        return 1;
    }
    if ((width_opt.has_value() && *width_opt <= 0) ||
        (height_opt.has_value() && *height_opt <= 0) ||
        (cols_opt.has_value() && *cols_opt <= 0)) {
        std::cerr << "MolTUI Error: --width, --height, and --cols must be positive.\n";
        return 1;
    }

    auto parsed_mode = moltui::render::ProtocolRenderer::parse_mode(render_mode);
    if (!parsed_mode.has_value()) {
        std::cerr << "MolTUI Error: Unknown rendering mode '" << render_mode
                  << "'. Valid options: auto, kitty, half-blocks, ascii\n";
        return 1;
    }
    moltui::render::TerminalGraphicsMode graphics_mode = *parsed_mode;

    auto parsed_style = moltui::render::parse_render_style(style);
    if (!parsed_style.has_value()) {
        std::cerr << "MolTUI Error: Unknown style '" << style
                  << "'. Valid options: ball, vdw, licorice, wire\n";
        return 1;
    }

    auto parsed_shading = moltui::render::parse_render_shading(shading);
    if (!parsed_shading.has_value()) {
        std::cerr << "MolTUI Error: Unknown shading '" << shading
                  << "'. Valid options: lit, flat\n";
        return 1;
    }

    auto parsed_color = moltui::render::parse_color_mode(color_mode);
    if (!parsed_color.has_value()) {
        std::cerr << "MolTUI Error: Unknown color mode '" << color_mode
                  << "'. Valid options: element, symop, asym, molecule\n";
        return 1;
    }

    std::optional<size_t> frame_index_1based;
    if (!frame_spec.empty()) {
        std::string f_low = frame_spec;
        for (char& ch : f_low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (f_low == "all") {
            all_frames = true;
        } else {
            try {
                size_t pos = 0;
                long long val = std::stoll(frame_spec, &pos);
                if (pos != frame_spec.size() || val < 1) {
                    std::cerr << "MolTUI Error: --frame must be a positive integer or 'all'.\n";
                    return 1;
                }
                frame_index_1based = static_cast<size_t>(val);
            } catch (...) {
                std::cerr << "MolTUI Error: Invalid --frame value '" << frame_spec << "'.\n";
                return 1;
            }
        }
    }

    auto apply_rasterizer_options = [&](moltui::render::Rasterizer& rast) {
        rast.settings().style = *parsed_style;
        rast.settings().shading = *parsed_shading;
        rast.settings().depth_cue = depth_cue;
        rast.settings().show_bonds = show_bonds;
        rast.settings().show_unit_cell = show_unit_cell;
        rast.settings().color_mode = *parsed_color;
        rast.settings().asymmetric_unit_only = asym_only;
        if (hkl_plane.size() == 3) {
            rast.settings().miller_plane = glm::ivec3(hkl_plane[0], hkl_plane[1], hkl_plane[2]);
        }
    };

    try {
        std::vector<moltui::core::Structure> structures;
        if (!filepaths.empty()) {
            for (const auto& path : filepaths) {
                auto loaded = moltui::io::StructureLoader::load_structures(path);
                structures.insert(
                    structures.end(),
                    std::make_move_iterator(loaded.begin()),
                    std::make_move_iterator(loaded.end())
                );
            }
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
                st.is_molecular_crystal = true;
            }
            if (wrap && st.is_periodic()) {
                st.wrap_pbc();
                st.is_molecular_crystal = false;
            }
        }

        if (print_snapshot) {
            // Build list of frames/structures to render
            std::vector<moltui::core::Structure> to_render;
            if (all_frames) {
                for (const auto& st : structures) {
                    if (st.num_frames() > 1) {
                        for (size_t fi = 0; fi < st.num_frames(); ++fi) {
                            moltui::core::Structure frame_st = st;
                            frame_st.set_frame(fi);
                            frame_st.title = st.title + " [frame " + std::to_string(fi + 1) + "/" +
                                             std::to_string(st.num_frames()) + "]";
                            to_render.push_back(std::move(frame_st));
                        }
                    } else {
                        to_render.push_back(st);
                    }
                }
            } else if (frame_index_1based.has_value()) {
                size_t idx = *frame_index_1based;
                if (structures.size() > 1) {
                    if (idx > structures.size()) {
                        std::cerr << "MolTUI Error: --frame " << idx << " out of range (1-"
                                  << structures.size() << ").\n";
                        return 1;
                    }
                    to_render.push_back(structures[idx - 1]);
                } else if (structures[0].num_frames() > 1) {
                    if (idx > structures[0].num_frames()) {
                        std::cerr << "MolTUI Error: --frame " << idx << " out of range (1-"
                                  << structures[0].num_frames() << ").\n";
                        return 1;
                    }
                    moltui::core::Structure frame_st = structures[0];
                    frame_st.set_frame(idx - 1);
                    to_render.push_back(std::move(frame_st));
                } else if (idx > 1) {
                    std::cerr << "MolTUI Error: --frame " << idx << " out of range (only 1 structure/frame).\n";
                    return 1;
                } else {
                    to_render.push_back(structures[0]);
                }
            } else {
                to_render.push_back(structures.front());
            }

            int snap_w = 80;
            int snap_h = 50;
            int kitty_cols = 0;
            int kitty_rows = 0;
            if (graphics_mode == moltui::render::TerminalGraphicsMode::KittyGraphics) {
                if (cols_opt.has_value() && !width_opt.has_value() && !height_opt.has_value()) {
                    kitty_cols = *cols_opt;
                    kitty_rows = std::max(1, static_cast<int>(std::round(kitty_cols * 0.3125)));
                    snap_w = kitty_cols * 8;
                    snap_h = kitty_rows * 16;
                } else {
                    snap_w = width_opt.value_or(640);
                    snap_h = height_opt.value_or(400);
                    if (cols_opt.has_value()) {
                        kitty_cols = *cols_opt;
                        kitty_rows = std::max(
                            1,
                            static_cast<int>(std::round(kitty_cols * static_cast<double>(snap_h) / snap_w / 2.0))
                        );
                    }
                }
            } else {
                int char_w = width_opt.value_or(cols_opt.value_or(80));
                int char_h = height_opt.value_or(
                    cols_opt.has_value()
                        ? std::max(1, static_cast<int>(std::round(char_w * 0.3125)))
                        : 25
                );
                snap_w = char_w;
                snap_h = char_h * 2;
            }

            moltui::render::Rasterizer rast;
            apply_rasterizer_options(rast);

            for (size_t ri = 0; ri < to_render.size(); ++ri) {
                moltui::core::Structure st = to_render[ri];
                for (size_t one_based : select_ids) {
                    if (one_based >= 1 && one_based <= st.atoms.size()) {
                        st.atoms[one_based - 1].selected = true;
                    }
                }

                if (to_render.size() > 1) {
                    std::cout << "[" << (ri + 1) << "/" << to_render.size() << "] " << st.title << "\n";
                }

                moltui::render::Camera cam;
                cam.orthographic = orthographic;
                cam.set_viewport(snap_w, snap_h);
                const auto& b = st.get_bounds();
                glm::vec3 com = st.center_of_mass();
                cam.reset(com, b.radius);

                if (!view_expr.empty()) {
                    std::string err;
                    if (!moltui::render::aim_camera_along(st, cam, view_expr, nullptr, &err)) {
                        std::cerr << "MolTUI Error: " << err << "\n";
                        return 1;
                    }
                }
                if (rotate_xy.size() == 2) {
                    cam.rotate(rotate_xy[0], rotate_xy[1]);
                }

                moltui::render::fit_camera_to_structure(st, cam, rast.settings(), true);

                if (zoom_mag != 1.0f) {
                    cam.zoom(1.0f / zoom_mag);
                }
                if (pan_xy.size() == 2) {
                    cam.pan(pan_xy[0], pan_xy[1]);
                }

                moltui::render::Framebuffer fb(snap_w, snap_h);
                rast.render(st, cam, fb);
                switch (graphics_mode) {
                    case moltui::render::TerminalGraphicsMode::KittyGraphics:
                        std::cout << moltui::render::ProtocolRenderer::render_kitty(fb, 0, kitty_cols, kitty_rows);
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
            }
            return 0;
        }

        moltui::ui::App ui_app(std::move(structures), graphics_mode);
        ui_app.set_adaptive_quality(adaptive_quality);
        apply_rasterizer_options(ui_app.rasterizer());
        ui_app.camera().orthographic = orthographic;

        if (frame_index_1based.has_value()) {
            ui_app.set_initial_frame(*frame_index_1based);
        }
        if (!view_expr.empty()) {
            if (!ui_app.set_initial_view(view_expr)) {
                std::cerr << "MolTUI Error: Invalid --view direction '" << view_expr << "'.\n";
                return 1;
            }
        }
        if (rotate_xy.size() == 2) {
            ui_app.set_initial_rotation(rotate_xy[0], rotate_xy[1]);
        }
        if (pan_xy.size() == 2) {
            ui_app.set_initial_pan(pan_xy[0], pan_xy[1]);
        }
        if (zoom_mag != 1.0f) {
            ui_app.set_initial_zoom_mag(zoom_mag);
        }
        if (!select_ids.empty()) {
            ui_app.select_atoms(select_ids);
        }

        ui_app.run();

    } catch (const std::exception& e) {
        std::cerr << "MolTUI Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
