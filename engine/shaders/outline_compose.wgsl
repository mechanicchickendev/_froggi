// ============================================================================
// outline_compose.wgsl - Edge Detection and Outline Composition
// ============================================================================
// This shader detects edges in the silhouette texture and draws an outline

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@group(0) @binding(0) var silhouetteTexture: texture_2d<f32>;
@group(0) @binding(1) var silhouetteSampler: sampler;

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 6>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(1.0, -1.0),
        vec2<f32>(-1.0, 1.0),
        vec2<f32>(-1.0, 1.0),
        vec2<f32>(1.0, -1.0),
        vec2<f32>(1.0, 1.0)
    );
    
    var uv = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(1.0, 1.0),
        vec2<f32>(0.0, 0.0),
        vec2<f32>(0.0, 0.0),
        vec2<f32>(1.0, 1.0),
        vec2<f32>(1.0, 0.0)
    );
    
    var output: VertexOutput;
    output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    output.uv = uv[vertexIndex];
    return output;
}
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let texSize = vec2f(1280.0, 720.0);
    let pixelSize = 1.0 / texSize;
    
    let center = textureSample(silhouetteTexture, silhouetteSampler, in.uv);
    let centerHasObject = center.g;
    let centerDepth = center.b;
    
    let outlineWidth = 0.7;
    let depthThreshold = 0.003;
    var isEdge = false;
    let samples = 8;
    
    for (var i = 0; i < samples; i = i + 1) {
        let angle = f32(i) * 6.28318 / f32(samples);
        let offset = vec2f(cos(angle), sin(angle)) * pixelSize * outlineWidth;
        let neighbor = textureSample(silhouetteTexture, silhouetteSampler, in.uv + offset);
        let neighborHasObject = neighbor.g;
        let neighborDepth = neighbor.b;
        
        // Edge if: object boundary OR depth discontinuity
        if (abs(centerHasObject - neighborHasObject) > 0.5 ||
            abs(centerDepth - neighborDepth) > depthThreshold) {
            isEdge = true;
            break;
        }
    }
    
    if (isEdge) {
        return vec4f(0.003, 0.005, 0.01, 1.0);
     //  figleaf glow colour -> return vec4f(0.33, 0.65, 0.91, 1.0);
    } else {
        return vec4f(0.0, 0.0, 0.0, 0.0);
    }
}
