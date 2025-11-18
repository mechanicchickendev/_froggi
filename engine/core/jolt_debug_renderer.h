#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <glm/glm.hpp>
#include <vector>

namespace froggi {

class JoltDebugRenderer : public JPH::DebugRenderer {
public:
    struct DebugLine {
        glm::vec3 start;
        glm::vec3 end;
        glm::vec4 color;
    };
    
    JoltDebugRenderer() = default;
    ~JoltDebugRenderer() override = default;
    void Initialize() {
    // Called before drawing
}
    // DrawLine - capture lines for rendering
    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override {
        DebugLine line;
        line.start = glm::vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
        line.end = glm::vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ());
        line.color = glm::vec4(inColor.r / 255.0f, inColor.g / 255.0f, 
                              inColor.b / 255.0f, inColor.a / 255.0f);
        lines.push_back(line);
    }
    
    // DrawTriangle - draw as wireframe
    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, 
                      JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override {
        (void)inCastShadow;
        DrawLine(inV1, inV2, inColor);
        DrawLine(inV2, inV3, inColor);
        DrawLine(inV3, inV1, inColor);
    }
    
    // DrawText3D - not implemented
    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, 
                    JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override {
        (void)inPosition; (void)inString; (void)inColor; (void)inHeight;
    }
    
    // CreateTriangleBatch - not used for simple line rendering
    Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) override {
        (void)inTriangles; (void)inTriangleCount;
        return Batch(); // Return empty batch
    }
    
    Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, 
                             const JPH::uint32 *inIndices, int inIndexCount) override {
        (void)inVertices; (void)inVertexCount; (void)inIndices; (void)inIndexCount;
        return Batch(); // Return empty batch
    }
    
    // DrawGeometry - not implemented (we only do line rendering)
    void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds, 
                     float inLODScaleSq, JPH::ColorArg inModelColor, 
                     const GeometryRef &inGeometry, ECullMode inCullMode = ECullMode::CullBackFace,
                     ECastShadow inCastShadow = ECastShadow::On, 
                     EDrawMode inDrawMode = EDrawMode::Solid) override {
        (void)inModelMatrix; (void)inWorldSpaceBounds; (void)inLODScaleSq;
        (void)inModelColor; (void)inGeometry; (void)inCullMode;
        (void)inCastShadow; (void)inDrawMode;
        // Not implemented - we only do simple line rendering
    }
    
    const std::vector<DebugLine>& getLines() const { return lines; }
    
    void clear() { 
        lines.clear();
    }
    
private:
    std::vector<DebugLine> lines;
};

} // namespace froggi
