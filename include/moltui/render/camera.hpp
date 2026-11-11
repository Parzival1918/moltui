#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

namespace moltui::render {

class Camera {
public:
    Camera();

    void reset(const glm::vec3& center, float radius);
    void rotate(float delta_x, float delta_y); // Arcball drag
    void pan(float delta_x, float delta_y);    // Screen-space pan
    void zoom(float factor);                  // Distance scale

    void set_viewport(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }

    glm::mat4 view_matrix() const;
    glm::mat4 projection_matrix() const;
    glm::vec3 world_to_screen(const glm::vec3& world) const;
    glm::vec3 screen_to_world_ray(float screen_x, float screen_y) const;

    const glm::vec3& target() const { return target_; }
    void set_target(const glm::vec3& t) { target_ = t; }

    float distance() const { return distance_; }
    void set_distance(float d) { distance_ = d; }

    bool orthographic{false};

private:
    int width_{80};
    int height_{40};
    
    glm::vec3 target_{0.0f};
    glm::quat orientation_{1.0f, 0.0f, 0.0f, 0.0f};
    float distance_{10.0f};
    float initial_distance_{10.0f};

    float fov_degrees_{45.0f};
    float near_clip_{0.1f};
    float far_clip_{500.0f};
};

} // namespace moltui::render
