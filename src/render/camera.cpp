#include "moltui/render/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace moltui::render {

Camera::Camera() {
    orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void Camera::reset(const glm::vec3& center, float radius) {
    target_ = center;
    distance_ = std::max(2.5f * radius, 5.0f);
    initial_distance_ = distance_;
    orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void Camera::rotate(float delta_x, float delta_y) {
    // Arcball rotation
    float sens = 0.01f;
    glm::quat rot_y = glm::angleAxis(delta_x * sens, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat rot_x = glm::angleAxis(delta_y * sens, glm::vec3(1.0f, 0.0f, 0.0f));
    orientation_ = glm::normalize(rot_y * orientation_ * rot_x);
}

void Camera::pan(float delta_x, float delta_y) {
    float pan_speed = distance_ * 0.002f;
    glm::mat4 rot = glm::mat4_cast(glm::conjugate(orientation_));
    glm::vec3 right = glm::vec3(rot[0]);
    glm::vec3 up = glm::vec3(rot[1]);

    target_ -= right * (delta_x * pan_speed);
    target_ += up * (delta_y * pan_speed);
}

void Camera::zoom(float factor) {
    distance_ *= factor;
    if (distance_ < 0.5f) distance_ = 0.5f;
    if (distance_ > 1000.0f) distance_ = 1000.0f;
}

void Camera::set_viewport(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

glm::mat4 Camera::view_matrix() const {
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -distance_));
    view *= glm::mat4_cast(orientation_);
    view = glm::translate(view, -target_);
    return view;
}

glm::mat4 Camera::projection_matrix() const {
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    if (orthographic) {
        float h = distance_ * 0.5f;
        float w = h * aspect;
        return glm::ortho(-w, w, -h, h, near_clip_, far_clip_);
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
