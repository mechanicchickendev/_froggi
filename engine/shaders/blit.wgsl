//vertex shader--------------------
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

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

// -------------------- Uniforms --------------------
// Texture & sampler
@group(0) @binding(0) var myTexture: texture_2d<f32>;
@group(0) @binding(1) var mySampler: sampler;

// Zoom parameters
struct ZoomParams {
    zoom: f32,           // Zoom level (1.0 = no zoom, 2.0 = 2x zoom, etc.)
    centerX: f32,        // Center point X in UV space (0.0 to 1.0)
    centerY: f32,        // Center point Y in UV space (0.0 to 1.0)
    _padding: f32,
};

@group(0) @binding(2) var<uniform> zoomParams: ZoomParams;

// -------------------- Fragment --------------------
@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
    // Get texture dimensions
    let texSize = textureDimensions(myTexture);
    let texSizeF = vec2<f32>(texSize);
    
    // Calculate the zoomed UV coordinates
    let invZoom = 1.0 / zoomParams.zoom;
    let uvRange = vec2<f32>(invZoom, invZoom);
    let uvOffset = vec2<f32>(zoomParams.centerX, zoomParams.centerY) - uvRange * 0.5;
    
    // Remap UV coordinates to the zoomed region
    var zoomedUV = uv * uvRange + uvOffset;
    zoomedUV = clamp(zoomedUV, vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0));
    
    // Convert UV to pixel coordinates (CRITICAL: use textureLoad for speed!)
    let pixelCoord = vec2<i32>(zoomedUV * texSizeF);
    
    // Direct texture read - NO filtering, MUCH faster!
    var color: vec4<f32> = textureLoad(myTexture, pixelCoord, 0);
    
    return color;
}
