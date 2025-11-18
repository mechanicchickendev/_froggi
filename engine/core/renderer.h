#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>
#include <resource_manager.h>
#include <string>
#include <vector>
#include <functional>

// Forward declarations
struct GLFWwindow;
struct ImGuiContext;

namespace froggi {

// Forward declare game types
class Scene;
class GameObject;
class Component;
class MeshComponent;
class JoltDebugRenderer;

///////////////////////////////////////////////////////////////////////////////
// Renderer - Pure engine rendering system

class Renderer {
public:
    // ═══════════════════════════════════════════════════════════════════════
    // Internal Structures (must be public for getMeshByName return type)
    // ═══════════════════════════════════════════════════════════════════════
    
    struct MyUniforms {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 color;
        float time;
        float _pad[3];
    };
    static_assert(sizeof(MyUniforms) % 16 == 0);
    
    struct Mesh {
        wgpu::Buffer vertexBuffer;
        int vertexCount = 0;
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        std::string name;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        MyUniforms uniforms{};
        
        Mesh(const wgpu::Buffer& buffer, int count, const std::string& n = "")
            : vertexBuffer(buffer), vertexCount(count), name(n) {}
    };

    struct ZoomUniforms {
        float zoom;       // Zoom level
        float centerX;    // UV center X
        float centerY;    // UV center Y
        float _padding;
    };
    static_assert(sizeof(ZoomUniforms) % 16 == 0);

public:
    Renderer() = default;
    ~Renderer() = default;

    // ═══════════════════════════════════════════════════════════════════════
    // Public API - Called by Engine
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Initialize the renderer
     * @param width Window width
     * @param height Window height
     * @return true if initialization succeeded
     */
    bool init(int width, int height);
    
    /**
     * Shutdown and cleanup all resources
     */
    void shutdown();
    
    /**
     * Check if the window should close
     */
    bool isRunning();
    
    /**
     * Get the GLFW window handle (for input system)
     */
    GLFWwindow* getWindow();
    
    /**
     * Get the current aspect ratio
     */
    float getAspectRatio() const;
    
    /**
     * UI callback function type - game provides this
     */
    using UICallback = std::function<void()>;
    
    /**
     * Main render function - renders an entire scene
     * @param scene The scene to render
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param uiCallback Optional callback for game UI rendering
     */
    void renderScene(Scene* scene,
                    const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    UICallback uiCallback = nullptr);
    
    /**
     * Load a mesh from file and register it by name
     * @param name Identifier for the mesh
     * @param filepath Path to OBJ file
     * @return true if loaded successfully
     */
    bool loadMesh(const std::string& name, const std::string& filepath);
    
    /**
     * Get mesh by name (for internal use)
     */
    Renderer::Mesh* getMeshByName(const std::string& name);

    // Zoom controls
    void setZoom(float zoom) { m_zoomUniforms.zoom = zoom; }
    void setZoomCenter(float x, float y) { 
        m_zoomUniforms.centerX = x; 
        m_zoomUniforms.centerY = y; 
    }
    float getZoom() const { return m_zoomUniforms.zoom; }

private:
    // ═══════════════════════════════════════════════════════════════════════
    // Render Passes
    // ═══════════════════════════════════════════════════════════════════════
    
    void renderSilhouettePass(wgpu::CommandEncoder& encoder, Scene* scene);
    void renderMainPass(wgpu::CommandEncoder& encoder, Scene* scene);
    void renderOutlineComposePass(wgpu::CommandEncoder& encoder);
    void renderUIPass(wgpu::CommandEncoder& encoder, UICallback uiCallback);
    void renderBlitPass(wgpu::CommandEncoder& encoder);
    void renderDebugPass(wgpu::CommandEncoder& encoder, Scene* scene);

    // ═══════════════════════════════════════════════════════════════════════
    // Initialization Functions
    // ═══════════════════════════════════════════════════════════════════════
    
    bool initWindowAndDevice();
    void terminateWindowAndDevice();
    
    bool initSwapChain();
    void terminateSwapChain();
    
    bool initDepthBuffer();
    void terminateDepthBuffer();
    
    bool initRenderPipeline();
    void terminateRenderPipeline();
    
    bool initSilhouettePipeline();
    void terminateSilhouettePipeline();
    
    bool initOutlineComposePipeline();
    void terminateOutlineComposePipeline();
    
    bool initBlitPipeline();
    void terminateBlitPipeline();
    
    bool initDebugPipeline();
    void terminateDebugPipeline();
    
    bool initTexture();
    void terminateTexture();
    
    bool initGeometry();
    void terminateGeometry();
    
    bool initUniforms();
    void terminateUniforms();
    
    bool initBindGroup();
    void terminateBindGroup();
    
    bool initGui();
    void terminateGui();
    
    void createRenderTarget(uint32_t width, uint32_t height);
    void createSilhouetteTarget(uint32_t width, uint32_t height);
    
    void onResize();

    // ═══════════════════════════════════════════════════════════════════════
    // Member Variables
    // ═══════════════════════════════════════════════════════════════════════
    
    // Window and Device
    GLFWwindow* m_window = nullptr;
    wgpu::Instance m_instance = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
    std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;
    
    // Swap Chain
    wgpu::SwapChain m_swapChain = nullptr;
    
    // Depth Buffer
    wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24PlusStencil8;
    wgpu::Texture m_depthTexture = nullptr;
    wgpu::TextureView m_depthTextureView = nullptr;
    
    // Main Render Pipeline
    wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
    wgpu::ShaderModule m_shaderModule = nullptr;
    wgpu::RenderPipeline m_pipeline = nullptr;
    
    // Silhouette Pipeline (for outlines)
    wgpu::RenderPipeline m_silhouettePipeline = nullptr;
    wgpu::ShaderModule m_silhouetteShader = nullptr;
    wgpu::Texture m_silhouetteTexture = nullptr;
    wgpu::TextureView m_silhouetteView = nullptr;
    
    // Outline Composition Pipeline
    wgpu::RenderPipeline m_outlineComposePipeline = nullptr;
    wgpu::ShaderModule m_outlineComposeShader = nullptr;
    wgpu::BindGroup m_outlineComposeBindGroup = nullptr;
    wgpu::BindGroupLayout m_outlineComposeBindGroupLayout = nullptr;
    
    // Blit Pipeline (render target to swapchain)
    wgpu::RenderPipeline m_blitPipeline = nullptr;
    wgpu::ShaderModule m_blitShaderModule = nullptr;
    wgpu::BindGroup m_blitBindGroup = nullptr;
    wgpu::BindGroupLayout m_blitBindGroupLayout = nullptr;
    wgpu::PipelineLayout m_blitPipelineLayout = nullptr;
    
    // Debug Pipeline
    wgpu::RenderPipeline m_debugPipeline = nullptr;
    wgpu::ShaderModule m_debugShader = nullptr;
    wgpu::Buffer m_debugVertexBuffer = nullptr;
    wgpu::Buffer m_debugUniformBuffer = nullptr;
    wgpu::BindGroup m_debugBindGroup = nullptr;
    wgpu::BindGroupLayout m_debugBindGroupLayout = nullptr;
    
    // Render Targets
    wgpu::Texture m_colorTexture = nullptr;
    wgpu::TextureView m_colorView = nullptr;
    
    // Textures
    wgpu::Sampler m_sampler = nullptr;
    wgpu::Texture m_texture = nullptr;
    wgpu::TextureView m_textureView = nullptr;
    
    // Meshes
    std::vector<Mesh> m_meshes;
    
    // Bind Groups
    wgpu::BindGroup m_bindGroup = nullptr;
    
    // ImGui
    ImGuiContext* m_imguiContext = nullptr;
    
    // Camera matrices (provided by game)
    glm::mat4 m_viewMatrix = glm::mat4(1.0f);
    glm::mat4 m_projectionMatrix = glm::mat4(1.0f);
    
    // Time
    float m_time = 0.0f;
    float m_deltaTime = 0.0f;
    float m_lastTime = 0.0f;
    
    // Window dimensions
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    
    // Zoom uniforms
    wgpu::Buffer m_zoomUniformBuffer = nullptr;
    ZoomUniforms m_zoomUniforms{1.0f, 0.5f, 0.5f, 0.0f};
};

} // namespace froggi
