#pragma once
#include <cstdint>
#include <string>

// Forward declarations — keeps d3d11.h / heavy headers out of the header chain.
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;
class  PreviewMesh;
struct ParsedAnim;

// Lightweight D3D11 offscreen renderer for the Animation Manager 3D preview.
//
//  Usage:
//    Init(dev, ctx)       — once after device is ready
//    Resize(w, h)         — whenever the panel size changes
//    Render()             — each frame before ImGui::Image(GetSRV(), ...)
//    OrbitDrag / Zoom     — driven by mouse input inside the preview child-window
class PreviewRenderer {
public:
    PreviewRenderer()  = default;
    ~PreviewRenderer();

    // Compile shaders, create geometry + pipeline state.
    // Must be called before any other method.
    bool Init(ID3D11Device* dev, ID3D11DeviceContext* ctx);

    // Create / recreate the offscreen render target.
    // Call on first use and whenever the available size changes.
    bool Resize(int width, int height);

    // Render one frame to the offscreen RT.
    void Render();

    // SRV to pass to ImGui::Image(). Null until Resize() succeeds.
    ID3D11ShaderResourceView* GetSRV() const { return m_srv; }

    // True once shaders are compiled AND the render target is allocated.
    bool IsReady() const { return m_shadersOk && m_srv != nullptr; }

    // Camera orbit (delta in radians / distance units).
    void OrbitDrag(float dYaw, float dPitch);
    void Zoom(float deltaDistance);
    void ResetCamera();

    // Load part meshes from meshFolder (expects skeleton.json + *.obj files).
    // Safe to call multiple times; reloads on each call.
    void LoadMeshes(const std::string& meshFolder);

    // Set the animation to display.  nullptr = draw in bind pose.
    void SetAnim(const ParsedAnim* anim, uint32_t frame);

    // Skeleton X-ray overlay toggle (drawn on top of mesh, depth-test off).
    void SetShowSkeleton(bool v) { m_showSkeleton = v; }
    bool GetShowSkeleton()  const { return m_showSkeleton; }

    // Diagnostics
    int  GetMeshPartCount()    const;
    bool GetMeshTexLoaded()    const;  // false = white fallback in use

private:
    void ReleaseRT();
    bool CreateRT(int width, int height);
    void DrawSkeletonLines(const float viewProj[16]);
    void DrawHaraRot1Gizmo(const float viewProj[16]);  // always-on rotation arrow

    // D3D device / context (non-owning, lifetime managed by App)
    ID3D11Device*        m_dev = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;

    // Offscreen render target
    ID3D11Texture2D*          m_rtTex = nullptr;
    ID3D11RenderTargetView*   m_rtv   = nullptr;
    ID3D11ShaderResourceView* m_srv   = nullptr;
    ID3D11Texture2D*          m_dsTex = nullptr;
    ID3D11DepthStencilView*   m_dsv   = nullptr;

    // Pipeline objects
    ID3D11VertexShader*      m_vs     = nullptr;
    ID3D11PixelShader*       m_ps     = nullptr;
    ID3D11InputLayout*       m_layout = nullptr;
    ID3D11Buffer*            m_cbuf   = nullptr;  // 64-byte MVP constant buffer
    ID3D11Buffer*            m_geoVB  = nullptr;  // axes + grid geometry
    ID3D11RasterizerState*   m_rs        = nullptr;
    ID3D11DepthStencilState* m_dss       = nullptr;
    ID3D11DepthStencilState* m_dssNoDepth= nullptr;  // x-ray: depth test disabled
    ID3D11BlendState*        m_bs        = nullptr;

    bool m_shadersOk    = false;
    bool m_showSkeleton = false;
    int  m_width     = 0;
    int  m_height    = 0;
    int  m_geoCount  = 0;

    // Orbit camera (spherical coordinates around look-target)
    float m_yaw   =  0.5f;    // radians, horizontal rotation
    float m_pitch =  0.25f;   // radians, elevation
    float m_dist  = 250.f;    // distance in scene units (cm)

    // Part meshes (Phase 5+)
    // Raw pointer: unique_ptr<PreviewMesh> triggers MSVC C2338 when
    // PreviewMesh is only forward-declared.  Lifetime managed manually.
    PreviewMesh*      m_mesh  = nullptr;
    const ParsedAnim* m_anim  = nullptr;
    uint32_t          m_frame = 0;
};
