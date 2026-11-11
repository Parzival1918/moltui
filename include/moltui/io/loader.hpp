#pragma once

#include "moltui/core/structure.hpp"
#include <string>
#include <filesystem>

namespace moltui::io {

class StructureLoader {
public:
    static core::Structure load_file(const std::filesystem::path& path);

private:
    static core::Structure load_pdb_or_mmcif(const std::filesystem::path& path);
    static core::Structure load_cif_small(const std::filesystem::path& path);
    static core::Structure load_xyz(const std::filesystem::path& path);
    static core::Structure load_poscar(const std::filesystem::path& path);
};

} // namespace moltui::io
