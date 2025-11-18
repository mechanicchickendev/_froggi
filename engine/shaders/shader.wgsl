struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,
    @location(3) uv: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
    @location(1) normal: vec3f,
    @location(2) uv: vec2f,
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
    
    // Apply model transform to vertex
    var worldPos: vec3f = (uMyUniforms.modelMatrix * vec4f(in.position, 1.0)).xyz;
    
    // Transform normal to world space
    var worldNormal: vec3f = normalize((uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz);
    
    // Transform to clip space
    out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * vec4f(worldPos, 1.0);
    
    // Output normal
    out.normal = worldNormal;
    
    // Pass-through
    out.color = in.color;
    out.uv = in.uv;
    
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // Sample full RGBA from the texture
    let texColor: vec4f = textureSample(gradientTexture, textureSampler, in.uv);
    
    // Gamma correction (if desired)
    let corrected_rgb: vec3f = pow(texColor.rgb, vec3f(2.0));
    
    // Combine texture alpha with uniform alpha for fading control
    let final_alpha: f32 = texColor.a * uMyUniforms.color.a;
    
    // Return final color
    return vec4f(corrected_rgb, final_alpha);
}
