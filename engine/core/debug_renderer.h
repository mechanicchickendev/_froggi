#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace froggi {

class DebugRenderer {
public:
    struct Line {
        glm::vec3 start;
        glm::vec3 end;
        glm::vec4 color;
    };
    
    static std::vector<Line> lines;
    
    // Draw a box (for colliders)
    static void drawBox(const glm::vec3& center, const glm::vec3& size, 
                       const glm::vec4& color = glm::vec4(0, 1, 0, 1));
    
    // Draw a capsule (for character colliders)
    static void drawCapsule(const glm::vec3& center, float radius, float height,
                           const glm::vec4& color = glm::vec4(0, 1, 0, 1));
    
    // Draw a sphere
    static void drawSphere(const glm::vec3& center, float radius,
                          const glm::vec4& color = glm::vec4(0, 1, 0, 1));
    
    // Clear all debug lines
    static void clear() { lines.clear(); }
};

} // namespace froggi
