#include "moltui/render/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace moltui::render {

Camera::Camera() {
    orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void Camera::reset(const glm::vec3& center, float radius) {
    target_ = center;
    pan_offset_ = glm::vec2(0.0f);
    distance_ = std::max(2.5f * radius, 5.0f);
    initial_distance_ = distance_;
    ortho_half_height_ = std::max(radius * 1.06f, 0.75f);
    orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void Camera::fit_extent(
    const glm::vec3& center,
    const std::vector<glm::vec3>& points,
    const std::vector<float>& radii,
    bool expand_only,
    bool reset_pan_offset
) {
    if (points.empty()) return;

    if (!expand_only) {
        target_ = center;
        if (reset_pan_offset) {
            pan_offset_ = glm::vec2(0.0f);
        }
    }

    glm::mat3 rot = glm::mat3_cast(orientation_);
    float reach_x = 0.1f;
    float reach_y = 0.1f;
    float max_r = 0.5f;

    for (size_t i = 0; i < points.size(); ++i) {
        float r = (i < radii.size()) ? radii[i] : 0.0f;
        glm::vec3 d = points[i] - target_;
        glm::vec3 p = rot * d;
        reach_x = std::max(reach_x, std::abs(p.x) + r);
        reach_y = std::max(reach_y, std::abs(p.y) + r);
        max_r = std::max(max_r, glm::length(d) + r);
    }

    float aspect = static_cast<float>(width_) / static_cast<float>(std::max(1, height_));
    constexpr float kMargin = 1.06f;
    float needed_h = std::max(reach_y, reach_x / std::max(aspect, 0.1f)) * kMargin;
    needed_h = std::max(needed_h, 0.5f);

    if (expand_only) {
        ortho_half_height_ = std::max(ortho_half_height_, needed_h);
    } else {
        ortho_half_height_ = needed_h;
        distance_ = std::max(2.5f * max_r, 5.0f);
        initial_distance_ = distance_;
    }
}

void Camera::look_along(const glm::vec3& view_dir, const glm::vec3& up_hint) {
    float len = glm::length(view_dir);
    if (len < 1e-6f) return;
    glm::vec3 w = view_dir / len;

    glm::vec3 hint = up_hint;
    if (glm::length(hint) < 1e-6f) {
        hint = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    glm::vec3 right = glm::cross(hint, w);
    if (glm::length(right) < 1e-5f) {
        // Fallback to cardinal axis least parallel to w
        glm::vec3 abs_w = glm::abs(w);
        if (abs_w.x <= abs_w.y && abs_w.x <= abs_w.z) {
            hint = glm::vec3(1.0f, 0.0f, 0.0f);
        } else if (abs_w.y <= abs_w.z) {
            hint = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            hint = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        right = glm::cross(hint, w);
    }
    right = glm::normalize(right);
    glm::vec3 up = glm::normalize(glm::cross(w, right));

    // Rows of rotation matrix R are (right, up, w)
    glm::mat3 R(
        glm::vec3(right.x, up.x, w.x),
        glm::vec3(right.y, up.y, w.y),
        glm::vec3(right.z, up.z, w.z)
    );
    orientation_ = glm::normalize(glm::quat_cast(R));
}

void Camera::rotate(float delta_x, float delta_y) {
    // Screen-space camera-relative trackball rotation
    float sens = 0.008f;
    glm::quat rot_x = glm::angleAxis(delta_y * sens, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat rot_y = glm::angleAxis(delta_x * sens, glm::vec3(0.0f, 1.0f, 0.0f));
    orientation_ = glm::normalize(rot_x * rot_y * orientation_);
}

void Camera::pan(float delta_x, float delta_y) {
    float pan_speed = orthographic ? (ortho_half_height_ * 0.004f) : (distance_ * 0.002f);
    pan_offset_.x += delta_x * pan_speed;
    pan_offset_.y += delta_y * pan_speed;
}

void Camera::zoom(float factor) {
    distance_ = std::clamp(distance_ * factor, 0.5f, 1000.0f);
    ortho_half_height_ = std::clamp(ortho_half_height_ * factor, 0.25f, 500.0f);
}

void Camera::set_viewport(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

float Camera::pixels_per_world() const {
    return static_cast<float>(height_) / (2.0f * std::max(ortho_half_height_, 1e-4f));
}

glm::mat4 Camera::view_matrix() const {
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(pan_offset_.x, pan_offset_.y, -distance_));
    view *= glm::mat4_cast(orientation_);
    view = glm::translate(view, -target_);
    return view;
}

glm::mat4 Camera::projection_matrix() const {
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    if (orthographic) {
        float h = ortho_half_height_;
        float w = h * aspect;
        return glm::ortho(-w, w, -h, h, -500.0f, 500.0f);
    } else {
        return glm::perspective(glm::radians(fov_degrees_), aspect, near_clip_, far_clip_);
    }
}

glm::vec3 Camera::world_to_screen(const glm::vec3& world) const {
    glm::vec4 clip = projection_matrix() * view_matrix() * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0f) {
        return glm::vec3(-10000.0f, -10000.0f, -1.0f);
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float sx = (ndc.x * 0.5f + 0.5f) * width_;
    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * height_;
    return glm::vec3(sx, sy, ndc.z);
}

glm::vec3 Camera::screen_to_world_ray(float screen_x, float screen_y) const {
    float ndc_x = (2.0f * screen_x) / width_ - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y) / height_;

    glm::mat4 inv_vp = glm::inverse(projection_matrix() * view_matrix());
    glm::vec4 near_pt = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv_vp * glm::vec4(ndc_x, ndc_y,  1.0f, 1.0f);

    glm::vec3 p_near = glm::vec3(near_pt) / near_pt.w;
    glm::vec3 p_far  = glm::vec3(far_pt)  / far_pt.w;

    return glm::normalize(p_far - p_near);
}

} // namespace moltui::render
