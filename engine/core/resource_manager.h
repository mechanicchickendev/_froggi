#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <filesystem>

class resource_manager {
public:
	// (Just aliases to make notations lighter)
	using path = std::filesystem::path;
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;

	/**
	 * A structure that describes the data layout in the vertex buffer,
	 * used by loadGeometryFromObj and used it in `sizeof` and `offsetof`
	 * when uploading data to the GPU.
	 */
	struct VertexAttributes {
		vec3 position;
		vec3 normal;
		vec3 color;
		vec2 uv;
	};
	// Load a shader from a WGSL file into a new shader module
	static wgpu::ShaderModule loadShaderModule(const path& path, wgpu::Device device);

	// Load an 3D mesh from a standard .obj file into a vertex data buffer
	static bool loadGeometryFromObj(const path& path, std::vector<VertexAttributes>& vertexData);

	// Load an image from a standard image file into a new texture object
	// NB: The texture must be destroyed after use
	static wgpu::Texture loadTexture(const path& path, wgpu::Device device, wgpu::TextureView* pTextureView = nullptr);


    static float CamRotation[16];
    static float CamRotationState;

};
