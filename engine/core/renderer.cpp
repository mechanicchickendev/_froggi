#include "renderer.h"
#include "resource_manager.h"
#include "pond_interface.h"
#include "jolt_debug_renderer.h"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <chrono>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <sstream>
#include <string>
#include <array>
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "stb_image_write.h"
#include "stb_image.h" 
#pragma GCC diagnostic pop

using namespace wgpu;
using VertexAttributes = resource_manager::VertexAttributes;

// Engine configuration
constexpr float PI = 3.14159265358979323846f;
uint32_t renderWidth = 640;
uint32_t renderHeight = 360;
float displayWidth = renderWidth;
float displayHeight = renderHeight;
bool renderCheck = false;

namespace froggi {

///////////////////////////////////////////////////////////////////////////////
// Initialization

bool Renderer::init(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;
    m_lastTime = static_cast<float>(glfwGetTime());

    if (!initWindowAndDevice()) return false;
    if (!initSwapChain()) return false;
    if (!initDepthBuffer()) return false;
    if (!initRenderPipeline()) return false;
    if (!initSilhouettePipeline()) return false;
    if (!initTexture()) return false;
    if (!initGeometry()) return false;
    if (!initUniforms()) return false;
    if (!initBindGroup()) return false;
    if (!initGui()) return false;
    
    createRenderTarget(renderWidth, renderHeight);
    createSilhouetteTarget(renderWidth, renderHeight);
    
    if (!initBlitPipeline()) return false;
    if (!initOutlineComposePipeline()) return false;
    if (!initDebugPipeline()) return false; 
    
    std::cout << "Renderer initialized successfully!" << std::endl;
    return true;
}

void Renderer::shutdown() {
    terminateGui();
    terminateBindGroup();
    terminateUniforms();
    terminateGeometry();
    terminateTexture();
    terminateSilhouettePipeline();
    terminateOutlineComposePipeline();
    terminateRenderPipeline();
    terminateBlitPipeline();
    terminateDebugPipeline(); 
    terminateDepthBuffer();
    terminateSwapChain();
    terminateWindowAndDevice();
}

bool Renderer::isRunning() {
    return !glfwWindowShouldClose(m_window);
}

GLFWwindow* Renderer::getWindow() {
    return m_window;
}

float Renderer::getAspectRatio() const {
    return static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
}

///////////////////////////////////////////////////////////////////////////////
// Main Rendering Function - Called by Engine

void Renderer::renderScene(Scene* scene,
                           const glm::mat4& viewMatrix,
                           const glm::mat4& projectionMatrix,
                           UICallback uiCallback) {
    if (!scene) return;
    
    auto frameStart = std::chrono::high_resolution_clock::now();
    
    float currentTime = static_cast<float>(glfwGetTime());
    m_deltaTime = currentTime - m_lastTime;
    if (m_deltaTime <= 0.0f) m_deltaTime = 1.0f / 120.0f;
    m_lastTime = currentTime;
    m_time = currentTime;

    glfwPollEvents();

    // Store camera matrices
    m_viewMatrix = viewMatrix;
    m_projectionMatrix = projectionMatrix;

    CommandEncoderDescriptor encoderDesc{};
    encoderDesc.label = "Frame Encoder";
    CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

    auto t1 = std::chrono::high_resolution_clock::now();
    renderSilhouettePass(encoder, scene);
    auto t2 = std::chrono::high_resolution_clock::now();
    
    renderMainPass(encoder, scene);
    auto t3 = std::chrono::high_resolution_clock::now();
    
    renderOutlineComposePass(encoder);
    auto t4 = std::chrono::high_resolution_clock::now();

    if (scene && scene->collisionSystem && scene->collisionSystem->isDebugDrawEnabled()) {
        renderDebugPass(encoder, scene);
    }
    auto t5 = std::chrono::high_resolution_clock::now();

    if (uiCallback) {
        renderUIPass(encoder, uiCallback);
    }
    auto t6 = std::chrono::high_resolution_clock::now();

    renderBlitPass(encoder);
    auto t7 = std::chrono::high_resolution_clock::now();

    // Submit commands
    CommandBufferDescriptor cmdDesc{};
    cmdDesc.label = "Command Buffer";
    CommandBuffer cmd = encoder.finish(cmdDesc);
    m_queue.submit(cmd);
    m_swapChain.present();
    
    auto frameEnd = std::chrono::high_resolution_clock::now();
    
    // Print timing (every 60 frames to avoid spam)
    static int frameCount = 0;
    if (++frameCount % 60 == 0) {
        auto silhouetteTime = std::chrono::duration<float, std::milli>(t2 - t1).count();
        auto mainTime = std::chrono::duration<float, std::milli>(t3 - t2).count();
        auto outlineTime = std::chrono::duration<float, std::milli>(t4 - t3).count();
        auto debugTime = std::chrono::duration<float, std::milli>(t5 - t4).count();
        auto uiTime = std::chrono::duration<float, std::milli>(t6 - t5).count();
        auto blitTime = std::chrono::duration<float, std::milli>(t7 - t6).count();
        auto totalTime = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
    if (renderCheck){
        std::cout << "\n=== Render Timing ===" << std::endl;
        std::cout << "Silhouette: " << silhouetteTime << "ms" << std::endl;
        std::cout << "Main Pass:  " << mainTime << "ms" << std::endl;
        std::cout << "Outline:    " << outlineTime << "ms" << std::endl;
        std::cout << "Debug:      " << debugTime << "ms" << std::endl;
        std::cout << "UI:         " << uiTime << "ms" << std::endl;
        std::cout << "Blit:       " << blitTime << "ms" << std::endl;
        std::cout << "TOTAL:      " << totalTime << "ms" << std::endl;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Render Passes

void Renderer::renderSilhouettePass(CommandEncoder& encoder, Scene* scene) {
    RenderPassColorAttachment silhouetteAttachment{};
    silhouetteAttachment.view = m_silhouetteView;
    silhouetteAttachment.loadOp = LoadOp::Clear;
    silhouetteAttachment.storeOp = StoreOp::Store;
    silhouetteAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
    
    RenderPassDepthStencilAttachment depthAttachment{};
    depthAttachment.view = m_depthTextureView;
    depthAttachment.depthLoadOp = LoadOp::Clear;
    depthAttachment.depthStoreOp = StoreOp::Store;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.depthReadOnly = false;
    depthAttachment.stencilLoadOp = LoadOp::Clear;
    depthAttachment.stencilStoreOp = StoreOp::Store;
    depthAttachment.stencilClearValue = 0;
    depthAttachment.stencilReadOnly = false;

    RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &silhouetteAttachment;
    passDesc.depthStencilAttachment = &depthAttachment;

    RenderPassEncoder renderPass = encoder.beginRenderPass(passDesc);
    renderPass.setPipeline(m_silhouettePipeline);
    
    // Render all objects with unique IDs for outline detection
    size_t objectIndex = 0;
    for (auto* gameObject : scene->gameObjects) {
        if (!gameObject->active) continue;
        
        MeshComponent* meshComp = gameObject->getComponent<MeshComponent>();
        if (!meshComp || !meshComp->enabled) continue;
        
        Mesh* meshData = getMeshByName(meshComp->meshName);
        if (!meshData) continue;
        
        // Update uniforms
        meshData->uniforms.modelMatrix = gameObject->getWorldTransform();
        meshData->uniforms.viewMatrix = m_viewMatrix;
        meshData->uniforms.projectionMatrix = m_projectionMatrix;
        meshData->uniforms.time = m_time;
        meshData->uniforms.color = glm::vec4(float(objectIndex + 1) / 255.0f, 0.0f, 0.0f, 1.0f);
        
        m_queue.writeBuffer(meshData->uniformBuffer, 0, &meshData->uniforms, sizeof(MyUniforms));
        
        renderPass.setBindGroup(0, meshData->bindGroup, 0, nullptr);
        renderPass.setVertexBuffer(0, meshData->vertexBuffer, 0,
                                  meshData->vertexCount * sizeof(VertexAttributes));
        renderPass.draw(meshData->vertexCount, 1, 0, 0);
        
        objectIndex++;
    }
    
    renderPass.end();
}

void Renderer::renderMainPass(CommandEncoder& encoder, Scene* scene) {
    RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = m_colorView;
    colorAttachment.loadOp = LoadOp::Clear;
    colorAttachment.storeOp = StoreOp::Store;
    colorAttachment.clearValue = {0.00001f, 0.0003f, 0.0005f, 1.0f};

    RenderPassDepthStencilAttachment depthAttachment{};
    depthAttachment.view = m_depthTextureView;
    depthAttachment.depthLoadOp = LoadOp::Clear;
    depthAttachment.depthStoreOp = StoreOp::Store;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.depthReadOnly = false;
    depthAttachment.stencilLoadOp = LoadOp::Clear;
    depthAttachment.stencilStoreOp = StoreOp::Store;
    depthAttachment.stencilClearValue = 0;
    depthAttachment.stencilReadOnly = false;

    RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = &depthAttachment;

    RenderPassEncoder renderPass = encoder.beginRenderPass(passDesc);
    renderPass.setPipeline(m_pipeline);

    // Render all game objects with mesh components
    for (auto* gameObject : scene->gameObjects) {
        if (!gameObject->active) continue;
        
        MeshComponent* meshComp = gameObject->getComponent<MeshComponent>();
        if (!meshComp || !meshComp->enabled) continue;
        
        Renderer::Mesh* meshData = getMeshByName(meshComp->meshName);
        if (!meshData) continue;
        
        // Update uniforms with GameObject's world transform
        meshData->uniforms.modelMatrix = gameObject->getWorldTransform();
        meshData->uniforms.viewMatrix = m_viewMatrix;
        meshData->uniforms.projectionMatrix = m_projectionMatrix;
        meshData->uniforms.time = m_time;
        meshData->uniforms.color = meshComp->color;

        m_queue.writeBuffer(meshData->uniformBuffer, 0, &meshData->uniforms, sizeof(MyUniforms));

        renderPass.setBindGroup(0, meshData->bindGroup, 0, nullptr);
        renderPass.setVertexBuffer(0, meshData->vertexBuffer, 0,
                                 meshData->vertexCount * sizeof(VertexAttributes));
        renderPass.draw(meshData->vertexCount, 1, 0, 0);
    }

    renderPass.end();
}

void Renderer::renderOutlineComposePass(CommandEncoder& encoder) {
    RenderPassColorAttachment composeAttachment{};
    composeAttachment.view = m_colorView;
    composeAttachment.loadOp = LoadOp::Load;
    composeAttachment.storeOp = StoreOp::Store;

    RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &composeAttachment;

    RenderPassEncoder renderPass = encoder.beginRenderPass(passDesc);
    renderPass.setPipeline(m_outlineComposePipeline);
    renderPass.setBindGroup(0, m_outlineComposeBindGroup, 0, nullptr);
    renderPass.draw(6, 1, 0, 0);
    renderPass.end();
}

void Renderer::renderUIPass(CommandEncoder& encoder, UICallback uiCallback) {
    RenderPassColorAttachment uiAttachment{};
    uiAttachment.view = m_colorView;
    uiAttachment.loadOp = LoadOp::Load;
    uiAttachment.storeOp = StoreOp::Store;

    RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &uiAttachment;

    RenderPassEncoder renderPass = encoder.beginRenderPass(passDesc);
    
    // Start ImGui frame
    ImGui::SetCurrentContext(m_imguiContext);
    ImGuiIO& io = ImGui::GetIO();
    
    // CRITICAL FIX: Override the display size to match render target
    io.DisplaySize = ImVec2((float)renderWidth, (float)renderHeight);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = m_deltaTime;
    
    // Don't call ImGui_ImplGlfw_NewFrame() - it overwrites DisplaySize with window size!
    // ImGui_ImplGlfw_NewFrame();  // COMMENT THIS OUT
    
    ImGui_ImplWGPU_NewFrame();
    ImGui::NewFrame();
    
    // Call game's UI rendering
    uiCallback();
    
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
    
    renderPass.end();
}

void Renderer::renderBlitPass(CommandEncoder& encoder) {
    // Update zoom uniforms before rendering
    m_queue.writeBuffer(m_zoomUniformBuffer, 0, &m_zoomUniforms, sizeof(ZoomUniforms));
    
    TextureView swapView = m_swapChain.getCurrentTextureView();
    if (!swapView) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        return;
    }
    
    RenderPassColorAttachment swapAttachment{};
    swapAttachment.view = swapView;
    swapAttachment.loadOp = LoadOp::Clear;
    swapAttachment.storeOp = StoreOp::Store;

    RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &swapAttachment;

    RenderPassEncoder renderPass = encoder.beginRenderPass(passDesc);
    renderPass.setPipeline(m_blitPipeline);
    renderPass.setBindGroup(0, m_blitBindGroup, 0, nullptr);
    renderPass.draw(6, 1, 0, 0);
    renderPass.end();
}

///////////////////////////////////////////////////////////////////////////////
// Resource Management

Renderer::Mesh* Renderer::getMeshByName(const std::string& name) {
    for (auto& mesh : m_meshes) {
        if (mesh.name == name) {
            return &mesh;
        }
    }
    return nullptr;
}

bool Renderer::loadMesh(const std::string& name, const std::string& filepath) {
    std::vector<VertexAttributes> vertexData;
    if (!resource_manager::loadGeometryFromObj(filepath, vertexData)) {
        std::cerr << "Could not load geometry: " << filepath << std::endl;
        return false;
    }

    if (vertexData.empty()) {
        std::cerr << "No vertices loaded from: " << filepath << std::endl;
        return false;
    }

    BufferDescriptor bufferDesc{};
    bufferDesc.size = vertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;

    wgpu::Buffer vertexBuffer = m_device.createBuffer(bufferDesc);
    if (!vertexBuffer) {
        std::cerr << "Failed to create vertex buffer for " << filepath << std::endl;
        return false;
    }

    m_queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
    
    // Create mesh and setup uniforms/bind group
    m_meshes.emplace_back(vertexBuffer, static_cast<int>(vertexData.size()), name);
    Mesh& mesh = m_meshes.back();
    
    // Create uniform buffer for this mesh
    BufferDescriptor uniformDesc;
    uniformDesc.size = sizeof(MyUniforms);
    uniformDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    uniformDesc.mappedAtCreation = false;
    mesh.uniformBuffer = m_device.createBuffer(uniformDesc);
    
    mesh.uniforms.modelMatrix = glm::mat4(1.0f);
    mesh.uniforms.color = glm::vec4(1.0f);
    m_queue.writeBuffer(mesh.uniformBuffer, 0, &mesh.uniforms, sizeof(MyUniforms));
    
    // Create bind group
    std::vector<BindGroupEntry> bindings(3);
    bindings[0].binding = 0;
    bindings[0].buffer = mesh.uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(MyUniforms);
    
    bindings[1].binding = 1;
    bindings[1].textureView = m_textureView;
    
    bindings[2].binding = 2;
    bindings[2].sampler = m_sampler;
    
    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = m_bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    mesh.bindGroup = m_device.createBindGroup(bindGroupDesc);
    
    std::cout << "Loaded mesh: " << name << " (" << vertexData.size() << " vertices)" << std::endl;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Private initialization methods (unchanged from original)

bool Renderer::initWindowAndDevice() {
    m_instance = createInstance(InstanceDescriptor{});
    if (!m_instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(displayWidth, displayHeight, "_froggi", NULL, NULL);
    if (!m_window) {
        std::cerr << "Could not open window!" << std::endl;
        return false;
    }

    std::cout << "Requesting adapter..." << std::endl;
    m_surface = glfwGetWGPUSurface(m_instance, m_window);
    RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = m_surface;
    Adapter adapter = m_instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;

    SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

    std::cout << "Requesting device..." << std::endl;
    RequiredLimits requiredLimits = Default;
    requiredLimits.limits.maxVertexAttributes = 4;
    requiredLimits.limits.maxVertexBuffers = 1;
    requiredLimits.limits.maxBufferSize = 15000000 * sizeof(VertexAttributes);
    requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.maxInterStageShaderComponents = 8;
    requiredLimits.limits.maxBindGroups = 2;
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
    requiredLimits.limits.maxTextureDimension1D = 2048;
    requiredLimits.limits.maxTextureDimension2D = 2048;
    requiredLimits.limits.maxTextureArrayLayers = 1;
    requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
    requiredLimits.limits.maxSamplersPerShaderStage = 1;

    DeviceDescriptor deviceDesc;
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeaturesCount = 0;
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue.label = "The default queue";
    m_device = adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << m_device << std::endl;

    m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
        std::cout << "Device error: type " << type;
        if (message) std::cout << " (message: " << message << ")";
        std::cout << std::endl;
    });

    m_queue = m_device.getQueue();
    m_swapChainFormat = m_surface.getPreferredFormat(adapter);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
        auto that = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->onResize();
    });

    adapter.release();
    return m_device != nullptr;
}

void Renderer::terminateWindowAndDevice() {
    m_queue.release();
    m_device.release();
    m_surface.release();
    m_instance.release();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Renderer::initSwapChain() {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    std::cout << "Creating swapchain..." << std::endl;
    SwapChainDescriptor swapChainDesc;
    swapChainDesc.width = static_cast<uint32_t>(width);
    swapChainDesc.height = static_cast<uint32_t>(height);
    swapChainDesc.usage = TextureUsage::RenderAttachment;
    swapChainDesc.format = m_swapChainFormat;
    swapChainDesc.presentMode = PresentMode::Fifo;
    m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
    std::cout << "Swapchain: " << m_swapChain << std::endl;
    return m_swapChain != nullptr;
}

void Renderer::terminateSwapChain() {
    m_swapChain.release();
}

bool Renderer::initDepthBuffer() {
    TextureDescriptor depthTextureDesc;
    depthTextureDesc.dimension = TextureDimension::_2D;
    depthTextureDesc.format = m_depthTextureFormat;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.size = { renderWidth, renderHeight, 1 };
    depthTextureDesc.usage = TextureUsage::RenderAttachment;
    depthTextureDesc.viewFormatCount = 1;
    depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
    m_depthTexture = m_device.createTexture(depthTextureDesc);

    TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = TextureAspect::All;
    depthTextureViewDesc.baseArrayLayer = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel = 0;
    depthTextureViewDesc.mipLevelCount = 1;
    depthTextureViewDesc.dimension = TextureViewDimension::_2D;
    depthTextureViewDesc.format = m_depthTextureFormat;
    m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);

    return m_depthTextureView != nullptr;
}

void Renderer::terminateDepthBuffer() {
    m_depthTextureView.release();
    m_depthTexture.destroy();
    m_depthTexture.release();
}

bool Renderer::initRenderPipeline() {
    std::cout << "Creating shader module..." << std::endl;
    m_shaderModule = resource_manager::loadShaderModule("shaders/shader.wgsl", m_device);

    RenderPipelineDescriptor pipelineDesc;

    std::vector<VertexAttribute> vertexAttribs(4);
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3;
    vertexAttribs[0].offset = 0;

    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(resource_manager::VertexAttributes, normal);

    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(resource_manager::VertexAttributes, color);

    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(resource_manager::VertexAttributes, uv);

    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(resource_manager::VertexAttributes);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.vertex.module = m_shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;

    FragmentState fragmentState;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = m_shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    BlendState blendState;
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::One;
    blendState.alpha.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = m_depthTextureFormat;
    depthStencilState.stencilFront.compare = CompareFunction::Always;
    depthStencilState.stencilFront.failOp = StencilOperation::Keep;
    depthStencilState.stencilFront.depthFailOp = StencilOperation::Keep;
    depthStencilState.stencilFront.passOp = StencilOperation::Replace;
    depthStencilState.stencilBack = depthStencilState.stencilFront;
    depthStencilState.stencilReadMask = 0xFF;
    depthStencilState.stencilWriteMask = 0xFF;

    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    std::vector<BindGroupLayoutEntry> bindingLayoutEntries(3, Default);

    BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
    bindingLayout.binding = 0;
    bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    bindingLayout.buffer.type = BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

    BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
    textureBindingLayout.binding = 1;
    textureBindingLayout.visibility = ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType = TextureSampleType::Float;
    textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

    BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
    samplerBindingLayout.binding = 2;
    samplerBindingLayout.visibility = ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;

    BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
    bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
    m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;

    m_pipeline = m_device.createRenderPipeline(pipelineDesc);
    return m_pipeline != nullptr;
}

void Renderer::terminateRenderPipeline() {
    m_pipeline.release();
    m_shaderModule.release();
    m_bindGroupLayout.release();
}

bool Renderer::initSilhouettePipeline() {
    m_silhouetteShader = resource_manager::loadShaderModule(
        "shaders/silhouette.wgsl", m_device);
    
    RenderPipelineDescriptor pipelineDesc;
    
    std::vector<VertexAttribute> vertexAttribs(4);
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3;
    vertexAttribs[0].offset = 0;
    
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(resource_manager::VertexAttributes, normal);
    
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(resource_manager::VertexAttributes, color);
    
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(resource_manager::VertexAttributes, uv);
    
    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(resource_manager::VertexAttributes);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;
    
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.vertex.module = m_silhouetteShader;
    pipelineDesc.vertex.entryPoint = "vs_main";
    
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;
    
    FragmentState fragmentState;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = m_silhouetteShader;
    fragmentState.entryPoint = "fs_main";
    
    ColorTargetState colorTarget;
    colorTarget.format = TextureFormat::RGBA16Float;
    colorTarget.blend = nullptr;
    colorTarget.writeMask = ColorWriteMask::All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    DepthStencilState depthStencilState = Default;
    depthStencilState.depthCompare = CompareFunction::Less;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.format = m_depthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    
    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
    PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    pipelineDesc.layout = layout;
    
    m_silhouettePipeline = m_device.createRenderPipeline(pipelineDesc);
    return m_silhouettePipeline != nullptr;
}

void Renderer::terminateSilhouettePipeline() {
    m_silhouettePipeline.release();
    m_silhouetteShader.release();
}

bool Renderer::initOutlineComposePipeline() {
    m_outlineComposeShader = resource_manager::loadShaderModule("shaders/outline_compose.wgsl", m_device);
    
    BindGroupLayoutEntry bglEntries[2]{};
    bglEntries[0].binding = 0;
    bglEntries[0].visibility = ShaderStage::Fragment;
    bglEntries[0].texture.sampleType = TextureSampleType::Float;
    bglEntries[0].texture.viewDimension = TextureViewDimension::_2D;
    
    bglEntries[1].binding = 1;
    bglEntries[1].visibility = ShaderStage::Fragment;
    bglEntries[1].sampler.type = SamplerBindingType::Filtering;
    
    BindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 2;
    bglDesc.entries = bglEntries;
    m_outlineComposeBindGroupLayout = m_device.createBindGroupLayout(bglDesc);
    
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_outlineComposeBindGroupLayout;
    wgpu::PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);
    
    RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.layout = layout;
    pipelineDesc.vertex.module = m_outlineComposeShader;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.bufferCount = 0;
    
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;
    
    FragmentState fragmentState{};
    fragmentState.module = m_outlineComposeShader;
    fragmentState.entryPoint = "fs_main";
    
    BlendState blendState{};
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    blendState.alpha.srcFactor = BlendFactor::One;
    blendState.alpha.dstFactor = BlendFactor::Zero;
    blendState.alpha.operation = BlendOperation::Add;
    
    ColorTargetState colorTarget{};
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    m_outlineComposePipeline = m_device.createRenderPipeline(pipelineDesc);
    
    BindGroupEntry bgEntries[2]{};
    bgEntries[0].binding = 0;
    bgEntries[0].textureView = m_silhouetteView;
    bgEntries[1].binding = 1;
    bgEntries[1].sampler = m_sampler;
    
    BindGroupDescriptor bgDesc{};
    bgDesc.layout = m_outlineComposeBindGroupLayout;
    bgDesc.entryCount = 2;
    bgDesc.entries = bgEntries;
    m_outlineComposeBindGroup = m_device.createBindGroup(bgDesc);
    
    return m_outlineComposePipeline != nullptr;
}

void Renderer::terminateOutlineComposePipeline() {
    m_outlineComposeBindGroup.release();
    m_outlineComposePipeline.release();
    m_outlineComposeBindGroupLayout.release();
    m_outlineComposeShader.release();
    m_silhouetteView.release();
    m_silhouetteTexture.destroy();
    m_silhouetteTexture.release();
}

bool Renderer::initBlitPipeline() {
    m_blitShaderModule = resource_manager::loadShaderModule("shaders/blit.wgsl", m_device);
    
    // Create bind group layout with 3 entries: texture, sampler, and zoom uniform
    BindGroupLayoutEntry bglEntries[3]{};
    bglEntries[0].binding = 0;
    bglEntries[0].visibility = ShaderStage::Fragment;
    bglEntries[0].texture.sampleType = TextureSampleType::Float;
    bglEntries[0].texture.viewDimension = TextureViewDimension::_2D;
    bglEntries[0].texture.multisampled = false;
    
    bglEntries[1].binding = 1;
    bglEntries[1].visibility = ShaderStage::Fragment;
    bglEntries[1].sampler.type = SamplerBindingType::Filtering;
    
    // Add zoom uniform binding
    bglEntries[2].binding = 2;
    bglEntries[2].visibility = ShaderStage::Fragment;
    bglEntries[2].buffer.type = BufferBindingType::Uniform;
    bglEntries[2].buffer.minBindingSize = sizeof(ZoomUniforms);
    
    BindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 3;  // Changed from 2 to 3
    bglDesc.entries = bglEntries;
    m_blitBindGroupLayout = m_device.createBindGroupLayout(bglDesc);
    
    // Create zoom uniform buffer
    BufferDescriptor zoomUniformDesc;
    zoomUniformDesc.size = sizeof(ZoomUniforms);
    zoomUniformDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    zoomUniformDesc.mappedAtCreation = false;
    m_zoomUniformBuffer = m_device.createBuffer(zoomUniformDesc);

    // Initialize zoom uniforms
    m_zoomUniforms.zoom = 1.0f;
    m_zoomUniforms.centerX = 0.5f;
    m_zoomUniforms.centerY = 0.5f;
    m_queue.writeBuffer(m_zoomUniformBuffer, 0, &m_zoomUniforms, sizeof(ZoomUniforms));
    
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_blitBindGroupLayout;
    m_blitPipelineLayout = m_device.createPipelineLayout(layoutDesc);
    
    ColorTargetState colorTarget{};
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = nullptr;
    colorTarget.writeMask = ColorWriteMask::All;
    
    FragmentState fragmentState{};
    fragmentState.module = m_blitShaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.layout = m_blitPipelineLayout;
    pipelineDesc.vertex.module = m_blitShaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    m_blitPipeline = m_device.createRenderPipeline(pipelineDesc);
    
    // Create bind group with 3 entries
    BindGroupEntry bgEntries[3]{};
    bgEntries[0].binding = 0;
    bgEntries[0].textureView = m_colorView;
    bgEntries[1].binding = 1;
    bgEntries[1].sampler = m_sampler;
    bgEntries[2].binding = 2;
    bgEntries[2].buffer = m_zoomUniformBuffer;
    bgEntries[2].offset = 0;
    bgEntries[2].size = sizeof(ZoomUniforms);
    
    BindGroupDescriptor bgDesc{};
    bgDesc.layout = m_blitBindGroupLayout;
    bgDesc.entryCount = 3;  // Changed from 2 to 3
    bgDesc.entries = bgEntries;
    m_blitBindGroup = m_device.createBindGroup(bgDesc);
    
    return m_blitPipeline != nullptr;
}

void Renderer::terminateBlitPipeline() {
    if (m_zoomUniformBuffer) {
        m_zoomUniformBuffer.destroy();
        m_zoomUniformBuffer.release();
    }
    m_blitBindGroup.release();
    m_blitPipeline.release();
    m_blitPipelineLayout.release();
    m_blitBindGroupLayout.release();
    m_blitShaderModule.release();
}

bool Renderer::initTexture() {
    SamplerDescriptor samplerDesc;
    samplerDesc.addressModeU = AddressMode::Repeat;
    samplerDesc.addressModeV = AddressMode::Repeat;
    samplerDesc.addressModeW = AddressMode::Repeat;
    samplerDesc.magFilter = FilterMode::Nearest;
    samplerDesc.minFilter = FilterMode::Nearest;
    samplerDesc.mipmapFilter = MipmapFilterMode::Nearest;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 1.0f;
    samplerDesc.compare = CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    m_sampler = m_device.createSampler(samplerDesc);

    // Note: Texture loading is now done by the game via loadMesh()
    // The default texture is loaded here, but games should load their own
    m_texture = resource_manager::loadTexture(
        "assets/textures/master_spritesheet.png", m_device, &m_textureView);
    
    if (!m_texture) {
        std::cerr << "Warning: Could not load default texture. Game should load textures." << std::endl;
        // Create a dummy 1x1 white texture as fallback
        return true; // Don't fail, just warn
    }
    
    return m_textureView != nullptr;
}

void Renderer::terminateTexture() {
    m_textureView.release();
    m_texture.destroy();
    m_texture.release();
    m_sampler.release();
}

bool Renderer::initGeometry() {
    // Meshes will be loaded by game via loadMesh()
    return true;
}

void Renderer::terminateGeometry() {
    for (auto& mesh : m_meshes) {
        if (mesh.vertexBuffer) {
            mesh.vertexBuffer.destroy();
            mesh.vertexBuffer.release();
        }
        if (mesh.uniformBuffer) {
            mesh.uniformBuffer.destroy();
            mesh.uniformBuffer.release();
        }
        if (mesh.bindGroup) {
            mesh.bindGroup.release();
        }
    }
    m_meshes.clear();
}

bool Renderer::initUniforms() {
    m_viewMatrix = glm::mat4(1.0f);
    m_projectionMatrix = glm::ortho(
        -13.333f * 1.2f, 13.333f * 1.2f,
        7.5f * 1.2f, -7.5f * 1.2f,
        -150.0f, 100.0f
    );
    m_time = 1.0f;
    return true;
}

void Renderer::terminateUniforms() {
    // Individual mesh uniforms cleaned up in terminateGeometry
}

bool Renderer::initBindGroup() {
    // Bind groups created per-mesh in loadMesh()
    return true;
}

void Renderer::terminateBindGroup() {
    m_bindGroup.release();
}

bool Renderer::initGui() {
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiContext);
    
    ImGuiStyle& style = ImGui::GetStyle();
      style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // header
      style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // background
      style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // button
      style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // button hover
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.1f, 0.8f, 0.15f, 1.0f); // button hover
      style.Colors[ImGuiCol_Border] = ImVec4(0.1f, 0.8f, 0.15f, 1.0f); // green border
      style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // resize grip
      style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // resize grip hovered 
     style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // resize grip active
     
     style.FrameBorderSize = 1.0f;
    
    ImGui_ImplGlfw_InitForOther(m_window, true);
    ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, TextureFormat::Undefined);
    
    return true;
}

void Renderer::terminateGui() {
    ImGui::SetCurrentContext(m_imguiContext);
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
    ImGui::DestroyContext(m_imguiContext);
}

void Renderer::createRenderTarget(uint32_t width, uint32_t height) {
    wgpu::TextureDescriptor colorDesc{};
    colorDesc.size = { width, height, 1 };
    colorDesc.dimension = WGPUTextureDimension_2D;
    colorDesc.format = m_swapChainFormat;
    colorDesc.usage = WGPUTextureUsage_RenderAttachment | 
                      WGPUTextureUsage_CopySrc | 
                      WGPUTextureUsage_TextureBinding;
    colorDesc.mipLevelCount = 1;
    colorDesc.sampleCount = 1;
    
    m_colorTexture = m_device.createTexture(colorDesc);
    
    wgpu::TextureViewDescriptor viewDesc{};
    viewDesc.format = colorDesc.format;
    viewDesc.dimension = TextureViewDimension::_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = TextureAspect::All;
    
    m_colorView = m_colorTexture.createView(viewDesc);
}

void Renderer::createSilhouetteTarget(uint32_t width, uint32_t height) {
    wgpu::TextureDescriptor silhouetteDesc{};
    silhouetteDesc.size = { width, height, 1 };
    silhouetteDesc.dimension = WGPUTextureDimension_2D;
    silhouetteDesc.format = TextureFormat::RGBA16Float;
    silhouetteDesc.usage = WGPUTextureUsage_RenderAttachment | 
                           WGPUTextureUsage_TextureBinding;
    silhouetteDesc.mipLevelCount = 1;
    silhouetteDesc.sampleCount = 1;
    
    m_silhouetteTexture = m_device.createTexture(silhouetteDesc);
    
    wgpu::TextureViewDescriptor viewDesc{};
    viewDesc.format = silhouetteDesc.format;
    viewDesc.dimension = TextureViewDimension::_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = TextureAspect::All;
    
    m_silhouetteView = m_silhouetteTexture.createView(viewDesc);
}

void Renderer::onResize() {
    // Disabled for now
}

bool Renderer::initDebugPipeline() {
    std::cout << "Initializing debug pipeline..." << std::endl;
    
    // Load shader
    m_debugShader = resource_manager::loadShaderModule("shaders/debug.wgsl", m_device);
    if (!m_debugShader) {
        std::cerr << "Failed to load debug.wgsl" << std::endl;
        return false;
    }
    
    // Create bind group layout
    BindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = ShaderStage::Vertex;
    bglEntry.buffer.type = BufferBindingType::Uniform;
    bglEntry.buffer.minBindingSize = sizeof(glm::mat4);
    
    BindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    m_debugBindGroupLayout = m_device.createBindGroupLayout(bglDesc);
    
    // Create pipeline layout
    PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_debugBindGroupLayout;
    PipelineLayout pipelineLayout = m_device.createPipelineLayout(layoutDesc);
    
    // Create uniform buffer
    BufferDescriptor uniformDesc;
    uniformDesc.size = sizeof(glm::mat4);
    uniformDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
    uniformDesc.mappedAtCreation = false;
    m_debugUniformBuffer = m_device.createBuffer(uniformDesc);
    
    // Create bind group
    BindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = m_debugUniformBuffer;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(glm::mat4);
    
    BindGroupDescriptor bgDesc{};
    bgDesc.layout = m_debugBindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    m_debugBindGroup = m_device.createBindGroup(bgDesc);
    
    // Vertex attributes
    std::vector<VertexAttribute> vertexAttribs(2);
    
    vertexAttribs[0].shaderLocation = 0;
    vertexAttribs[0].format = VertexFormat::Float32x3;
    vertexAttribs[0].offset = 0;
    
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = VertexFormat::Float32x4;
    vertexAttribs[1].offset = sizeof(glm::vec3);
    
    VertexBufferLayout vertexBufferLayout;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(glm::vec3) + sizeof(glm::vec4);
    vertexBufferLayout.stepMode = VertexStepMode::Vertex;
    
    // Create pipeline descriptor
    RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.layout = pipelineLayout;
    
    // Vertex state
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.vertex.module = m_debugShader;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    
    // Primitive state
    pipelineDesc.primitive.topology = PrimitiveTopology::LineList;
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = FrontFace::CCW;
    pipelineDesc.primitive.cullMode = CullMode::None;
    
    // Fragment state
    FragmentState fragmentState{};
    fragmentState.module = m_debugShader;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    
    ColorTargetState colorTarget{};
    colorTarget.format = m_swapChainFormat;
    colorTarget.blend = nullptr;
    colorTarget.writeMask = ColorWriteMask::All;
    
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;
    
    // Multisample state
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    
    // No depth/stencil for debug lines (they should always be visible)
    pipelineDesc.depthStencil = nullptr;
    
    m_debugPipeline = m_device.createRenderPipeline(pipelineDesc);
    
    std::cout << "✓ Debug pipeline initialized" << std::endl;
    return m_debugPipeline != nullptr;
}

void Renderer::renderDebugPass(wgpu::CommandEncoder& encoder, Scene* scene) {
    if (!scene || !scene->collisionSystem) return;
    if (!scene->collisionSystem->isDebugDrawEnabled()) return;
    
    // Get debug lines from Jolt
    scene->collisionSystem->drawDebugShapes();
    auto* debugRenderer = scene->collisionSystem->getDebugRenderer();
    const auto& lines = debugRenderer->getLines();
    
    if (lines.empty()) return;
    
    // Build vertex buffer data
    struct DebugVertex {
        glm::vec3 position;
        glm::vec4 color;
        
        // Add constructor for easier initialization
        DebugVertex(const glm::vec3& pos, const glm::vec4& col) 
            : position(pos), color(col) {}
    };
    
    std::vector<DebugVertex> vertices;
    vertices.reserve(lines.size() * 2);
    
    for (const auto& line : lines) {
        vertices.push_back(DebugVertex(line.start, line.color));
        vertices.push_back(DebugVertex(line.end, line.color));
    }
    
    // Update vertex buffer
    if (m_debugVertexBuffer) {
        m_debugVertexBuffer.destroy();
    }
    
    BufferDescriptor bufferDesc{};
    bufferDesc.size = vertices.size() * sizeof(DebugVertex);
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
    m_debugVertexBuffer = m_device.createBuffer(bufferDesc);
    m_queue.writeBuffer(m_debugVertexBuffer, 0, vertices.data(), bufferDesc.size);
    
    // Update uniforms (view-projection matrix)
    glm::mat4 viewProj = m_projectionMatrix * m_viewMatrix;
    m_queue.writeBuffer(m_debugUniformBuffer, 0, &viewProj, sizeof(glm::mat4));
    
    // Render
    RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = m_colorView;
    colorAttachment.loadOp = LoadOp::Load;
    colorAttachment.storeOp = StoreOp::Store;
    
    RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    
    RenderPassEncoder renderPass = encoder.beginRenderPass(passDesc);
    renderPass.setPipeline(m_debugPipeline);
    renderPass.setBindGroup(0, m_debugBindGroup, 0, nullptr);
    renderPass.setVertexBuffer(0, m_debugVertexBuffer, 0, vertices.size() * sizeof(DebugVertex));
    renderPass.draw(vertices.size(), 1, 0, 0);
    renderPass.end();
}

void Renderer::terminateDebugPipeline() {
    if (m_debugVertexBuffer) {
        m_debugVertexBuffer.destroy();
        m_debugVertexBuffer.release();
    }
    m_debugBindGroup.release();
    m_debugPipeline.release();
    m_debugBindGroupLayout.release();
    m_debugShader.release();
    if (m_debugUniformBuffer) {
        m_debugUniformBuffer.destroy();
        m_debugUniformBuffer.release();
    }
}

} // namespace froggibinding = 2;
