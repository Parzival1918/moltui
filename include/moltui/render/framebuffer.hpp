#pragma once

#include <vector>
#include <cstdint>
#include <glm/vec3.hpp>

namespace moltui::render {

class Framebuffer {
public:
    Framebuffer(int width = 80, int height = 40);

    void resize(int width, int height);
    void clear(const glm::u8vec3& bg_color = {15, 17, 26}, float depth = 1e9f);

    int width() const { return width_; }
    int height() const { return height_; }

    inline bool test_and_set(int x, int y, float depth, const glm::u8vec3& color, int atom_id = -1) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return false;
        size_t idx = static_cast<size_t>(y * width_ + x);
        if (depth < depth_buffer_[idx]) {
            depth_buffer_[idx] = depth;
            color_buffer_[idx] = color;
            atom_id_buffer_[idx] = atom_id;
            return true;
        }
        return false;
    }

    const glm::u8vec3& get_color(int x, int y) const {
        return color_buffer_[y * width_ + x];
    }

    float get_depth(int x, int y) const {
        return depth_buffer_[y * width_ + x];
    }

    int get_atom_id(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return -1;
        return atom_id_buffer_[y * width_ + x];
    }

    const std::vector<glm::u8vec3>& colors() const { return color_buffer_; }

private:
    int width_{80};
    int height_{40};
    std::vector<glm::u8vec3> color_buffer_;
    std::vector<float> depth_buffer_;
    std::vector<int32_t> atom_id_buffer_;
};

} // namespace moltui::render
