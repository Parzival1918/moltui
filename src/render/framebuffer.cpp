#include "moltui/render/framebuffer.hpp"
#include <algorithm>

namespace moltui::render {

Framebuffer::Framebuffer(int width, int height) {
    resize(width, height);
}

void Framebuffer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    size_t size = static_cast<size_t>(width_ * height_);
    color_buffer_.resize(size, glm::u8vec3(15, 17, 26));
    depth_buffer_.resize(size, 1e9f);
    atom_id_buffer_.resize(size, -1);
}

void Framebuffer::clear(const glm::u8vec3& bg_color, float depth) {
    std::fill(color_buffer_.begin(), color_buffer_.end(), bg_color);
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), depth);
    std::fill(atom_id_buffer_.begin(), atom_id_buffer_.end(), -1);
}

} // namespace moltui::render
