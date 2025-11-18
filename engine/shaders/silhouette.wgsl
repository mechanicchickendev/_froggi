// ============================================================================
// silhouette.wgsl - Renders mesh with object ID and depth
// ============================================================================

const WORLD_PIXEL_SIZE: f32 = 0.0001;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,
    @location(3) uv: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
};

struct MyUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
    time: f32,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    
    // Same snapping logic as main shader for consistency
    var modelTranslation: vec3f = uMyUniforms.modelMatrix[3].xyz;
    modelTranslation = round(modelTranslation / WORLD_PIXEL_SIZE) * WORLD_PIXEL_SIZE;
    
    var worldPos: vec3f = (uMyUniforms.modelMatrix * vec4f(in.position, 1.0)).xyz;
    worldPos = round(worldPos / WORLD_PIXEL_SIZE) * WORLD_PIXEL_SIZE;
    
    out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * vec4f(worldPos, 1.0);
    
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // Pack data into RGBA channels:
    // R: Object ID (from uniform color.r)
    // G: 1.0 (indicates "has object")
    // B: Depth (builtin fragment depth)
    // A: 1.0 (opaque)
    return vec4f(uMyUniforms.color.r, 1.0, in.position.z, 1.0);
}
