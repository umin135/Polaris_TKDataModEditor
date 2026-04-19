// PreviewRenderer.cpp — D3D11 offscreen render target for Animation Manager 3D preview.
// Phase 3: axes + grid visualisation with orbit camera.
// Phase 5: part mesh loading and bind-pose / animated rendering.

#include "PreviewRenderer.h"
#include "PreviewMesh.h"
#include "SkeletonBoneFilter.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include <unordered_set>
#include <cstring>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// ─── HLSL ─────────────────────────────────────────────────────────────────────
//  Vertex shader transforms pos with row-major MVP (matches DirectXMath layout).
//  Pixel shader outputs per-vertex colour.

static const char g_shaderSrc[] = R"HLSL(
cbuffer PerFrame : register(b0)
{
    row_major float4x4 gMVP;
};

struct VSIn  { float3 pos : POSITION;  float3 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION;  float3 col : COLOR0; };

VSOut VS_main(VSIn v)
{
    VSOut o;
    o.pos = mul(float4(v.pos, 1.0f), gMVP);
    o.col = v.col;
    return o;
}

float4 PS_main(VSOut v) : SV_TARGET
{
    return float4(v.col, 1.0f);
}
)HLSL";

// ─── Geometry ─────────────────────────────────────────────────────────────────

struct Vtx { float x, y, z, r, g, b; };

// Builds axis lines (Y-up viewer space) and a ground grid.
// floorHeight: Y coordinate for the ground grid (default = game floor, 115 cm).
static std::vector<Vtx> BuildGeometry(float floorHeight)
{
    std::vector<Vtx> v;

    // Axis lines: 100-unit length, origin at floor level so they align with the grid.
    // X = right  (red)
    v.push_back({   0,         floorHeight,   0,   0.90f, 0.20f, 0.20f });
    v.push_back({ 100,         floorHeight,   0,   0.90f, 0.20f, 0.20f });
    // Y = up  (green)
    v.push_back({   0,         floorHeight,   0,   0.20f, 0.85f, 0.20f });
    v.push_back({   0, floorHeight + 100.f,   0,   0.20f, 0.85f, 0.20f });
    // Z = forward  (blue) — 캐릭터 정면(-Z) 방향
    v.push_back({   0,         floorHeight,    0,  0.20f, 0.50f, 0.90f });
    v.push_back({   0,         floorHeight, -100,  0.20f, 0.50f, 0.90f });

    // Ground grid at Y=floorHeight, 20-unit spacing, extends ±120 in X and Z.
    // ±200 범위는 eye(-Z≈213)에서 근거리 끝(z=-200)이 13유닛 → 극단적 원근 왜곡.
    // ±120으로 축소하면 근거리 끝(z=-120)이 eye에서 약 93유닛으로 적절.
    constexpr float gc = 0.22f;
    for (int i = -6; i <= 6; ++i) {
        float p = (float)i * 20.f;
        v.push_back({    p, floorHeight, -120.f,  gc, gc, gc });
        v.push_back({    p, floorHeight,  120.f,  gc, gc, gc });
        v.push_back({ -120.f, floorHeight,    p,  gc, gc, gc });
        v.push_back({  120.f, floorHeight,    p,  gc, gc, gc });
    }
    return v;
}

// ─── Destructor ───────────────────────────────────────────────────────────────

PreviewRenderer::~PreviewRenderer()
{
    delete m_mesh;  m_mesh = nullptr;
    ReleaseRT();
    if (m_geoVB)      { m_geoVB->Release();       m_geoVB       = nullptr; }
    if (m_cbuf)       { m_cbuf->Release();         m_cbuf        = nullptr; }
    if (m_layout)     { m_layout->Release();       m_layout      = nullptr; }
    if (m_ps)         { m_ps->Release();           m_ps          = nullptr; }
    if (m_vs)         { m_vs->Release();           m_vs          = nullptr; }
    if (m_rs)         { m_rs->Release();           m_rs          = nullptr; }
    if (m_dss)        { m_dss->Release();          m_dss         = nullptr; }
    if (m_dssNoDepth) { m_dssNoDepth->Release();   m_dssNoDepth  = nullptr; }
    if (m_bs)         { m_bs->Release();           m_bs          = nullptr; }
}

void PreviewRenderer::ReleaseRT()
{
    if (m_dsv)   { m_dsv->Release();   m_dsv   = nullptr; }
    if (m_dsTex) { m_dsTex->Release(); m_dsTex = nullptr; }
    if (m_srv)   { m_srv->Release();   m_srv   = nullptr; }
    if (m_rtv)   { m_rtv->Release();   m_rtv   = nullptr; }
    if (m_rtTex) { m_rtTex->Release(); m_rtTex = nullptr; }
}

// ─── Init ─────────────────────────────────────────────────────────────────────

bool PreviewRenderer::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    m_dev = dev;
    m_ctx = ctx;

    // ── Compile shaders ────────────────────────────────────────────
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* vsBlob  = nullptr;
    ID3DBlob* psBlob  = nullptr;
    ID3DBlob* errBlob = nullptr;

    if (FAILED(D3DCompile(g_shaderSrc, sizeof(g_shaderSrc) - 1,
                          "PreviewVS", nullptr, nullptr,
                          "VS_main", "vs_5_0", compileFlags, 0,
                          &vsBlob, &errBlob))) {
        if (errBlob) errBlob->Release();
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    if (FAILED(D3DCompile(g_shaderSrc, sizeof(g_shaderSrc) - 1,
                          "PreviewPS", nullptr, nullptr,
                          "PS_main", "ps_5_0", compileFlags, 0,
                          &psBlob, &errBlob))) {
        vsBlob->Release();
        if (errBlob) errBlob->Release();
        return false;
    }
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    dev->CreatePixelShader (psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);

    // ── Input layout: POSITION (float3, offset 0) + COLOR (float3, offset 12) ──
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    dev->CreateInputLayout(layoutDesc, 2,
                           vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                           &m_layout);

    vsBlob->Release();
    psBlob->Release();

    if (!m_vs || !m_ps || !m_layout) return false;

    // ── Constant buffer (64 bytes = one float4x4) ──────────────────
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = 64;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(dev->CreateBuffer(&bd, nullptr, &m_cbuf))) return false;
    }

    // ── Geometry vertex buffer ─────────────────────────────────────
    {
        std::vector<Vtx> verts = BuildGeometry(m_floorHeight);
        m_geoCount = (int)verts.size();

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(m_geoCount * sizeof(Vtx));
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = { verts.data() };
        if (FAILED(dev->CreateBuffer(&bd, &sd, &m_geoVB))) return false;
    }

    // ── Rasterizer state: no culling ───────────────────────────────
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode              = D3D11_FILL_SOLID;
        rd.CullMode              = D3D11_CULL_NONE;
        rd.DepthClipEnable       = TRUE;
        dev->CreateRasterizerState(&rd, &m_rs);
    }

    // ── Depth-stencil state: depth test + write ────────────────────
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable    = TRUE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dd.DepthFunc      = D3D11_COMPARISON_LESS;
        dev->CreateDepthStencilState(&dd, &m_dss);
    }

    // ── Depth-stencil state: no depth test (x-ray skeleton overlay) ──
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable    = FALSE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dd.DepthFunc      = D3D11_COMPARISON_ALWAYS;
        dev->CreateDepthStencilState(&dd, &m_dssNoDepth);
    }

    // ── Blend state: opaque ────────────────────────────────────────
    {
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable           = FALSE;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        dev->CreateBlendState(&bd, &m_bs);
    }

    m_shadersOk = true;
    return true;
}

// ─── Resize ───────────────────────────────────────────────────────────────────

bool PreviewRenderer::Resize(int width, int height)
{
    if (width <= 0 || height <= 0) return false;
    if (width == m_width && height == m_height && m_srv) return true;

    ReleaseRT();
    m_width  = width;
    m_height = height;
    return CreateRT(width, height);
}

bool PreviewRenderer::CreateRT(int width, int height)
{
    // Colour texture (RGBA8, bound as both RT and SRV for ImGui::Image)
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = (UINT)width;
        td.Height           = (UINT)height;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &m_rtTex)))   return false;
        if (FAILED(m_dev->CreateRenderTargetView(m_rtTex, nullptr, &m_rtv))) return false;
        if (FAILED(m_dev->CreateShaderResourceView(m_rtTex, nullptr, &m_srv))) return false;
    }

    // Depth-stencil texture (D24S8)
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = (UINT)width;
        td.Height           = (UINT)height;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &m_dsTex)))   return false;
        if (FAILED(m_dev->CreateDepthStencilView(m_dsTex, nullptr, &m_dsv))) return false;
    }
    return true;
}

// ─── Render ───────────────────────────────────────────────────────────────────

void PreviewRenderer::Render()
{
    if (!m_shadersOk || !m_srv) return;

    // Clear
    const float kClear[4] = { 0.07f, 0.07f, 0.07f, 1.f };
    m_ctx->ClearRenderTargetView(m_rtv, kClear);
    m_ctx->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH, 1.f, 0);

    // Output merger
    m_ctx->OMSetRenderTargets(1, &m_rtv, m_dsv);
    m_ctx->OMSetDepthStencilState(m_dss, 0);
    const float kBlend[4] = {};
    m_ctx->OMSetBlendState(m_bs, kBlend, 0xffffffff);

    // Rasterizer
    m_ctx->RSSetState(m_rs);
    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)m_width;
    vp.Height   = (float)m_height;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    m_ctx->RSSetViewports(1, &vp);

    // ── Camera ────────────────────────────────────────────────────
    // Character focus: orbit around Spine1 world position (cached from previous frame).
    // Otherwise orbit around the floor-level mid-character point.
    XMVECTOR target;
    if (m_charFocus && m_mesh && m_mesh->IsLoaded())
    {
        target = XMVectorSet(m_charFocusPos[0], m_charFocusPos[1], m_charFocusPos[2], 0.f);
    }
    else
    {
        target = XMVectorSet(0.f, m_floorHeight + 100.f, 0.f, 0.f);
    }
    XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    // Prevent gimbal lock near straight-up / straight-down
    if (fabsf(m_pitch) > 1.55f)
        up = XMVectorSet(0.f, 0.f, (m_pitch > 0.f ? -1.f : 1.f), 0.f);

    XMVECTOR eye = XMVectorAdd(target, XMVectorSet(
        m_dist * cosf(m_pitch) * sinf(m_yaw),
        m_dist * sinf(m_pitch),
        m_dist * cosf(m_pitch) * cosf(m_yaw),
        0.f));

    XMMATRIX view   = XMMatrixLookAtLH(eye, target, up);
    float    aspect = (float)m_width / (float)m_height;
    XMMATRIX proj   = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 1.f, 5000.f);
    XMMATRIX mvp    = view * proj;

    // Upload MVP
    D3D11_MAPPED_SUBRESOURCE ms = {};
    if (SUCCEEDED(m_ctx->Map(m_cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, &mvp, sizeof(XMMATRIX));
        m_ctx->Unmap(m_cbuf, 0);
    }

    // ── Draw geometry ─────────────────────────────────────────────
    m_ctx->IASetInputLayout(m_layout);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    UINT stride = sizeof(Vtx), vbOffset = 0;
    m_ctx->IASetVertexBuffers(0, 1, &m_geoVB, &stride, &vbOffset);
    m_ctx->VSSetShader(m_vs, nullptr, 0);
    m_ctx->VSSetConstantBuffers(0, 1, &m_cbuf);
    m_ctx->PSSetShader(m_ps, nullptr, 0);
    m_ctx->Draw((UINT)m_geoCount, 0);

    // ── Draw part meshes (Phase 5+) ───────────────────────────────
    float vpRaw[16];
    memcpy(vpRaw, &mvp, sizeof(vpRaw));   // view*proj (model=I for grid)
    if (m_mesh && m_mesh->IsLoaded()) {
        m_mesh->Draw(m_ctx, m_cbuf, m_anim, m_frame, vpRaw);
        // Restore topology for next frame's grid draw
        m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

        // Cache Spine1 world position for the next frame's character-focus camera.
        if (m_charFocus)
        {
            std::vector<PreviewMesh::BonePoseInfo> poses;
            m_mesh->GetBonePoses(poses);
            for (const auto& bp : poses)
            {
                if (bp.name == "Spine1")
                {
                    m_charFocusPos[0] = bp.pos[0];
                    m_charFocusPos[1] = bp.pos[1];
                    m_charFocusPos[2] = bp.pos[2];
                    break;
                }
            }
        }
    }

    // ── HARA_ROT1 rotation gizmo (always shown, depth-test off) ──────────────
    if (m_mesh && m_mesh->IsLoaded())
        DrawHaraRot1Gizmo(vpRaw);

    // ── Skeleton X-ray overlay ────────────────────────────────────────────────
    if (m_showSkeleton && m_mesh && m_mesh->IsLoaded())
        DrawSkeletonLines(vpRaw);

    // Unbind our RT so the main loop can re-establish the swapchain RT.
    ID3D11RenderTargetView* nullRTV = nullptr;
    m_ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
}

// ─── Camera controls ──────────────────────────────────────────────────────────

void PreviewRenderer::OrbitDrag(float dYaw, float dPitch)
{
    m_yaw   += dYaw;
    m_pitch += dPitch;
    if (m_pitch >  1.57f) m_pitch =  1.57f;
    if (m_pitch < -1.57f) m_pitch = -1.57f;
}

void PreviewRenderer::Zoom(float delta)
{
    m_dist += delta;
    if (m_dist <  10.f)   m_dist =  10.f;
    if (m_dist > 2000.f)  m_dist = 2000.f;
}

void PreviewRenderer::ResetCamera()
{
    m_yaw   = XM_PI - 0.5f;  // eye at -Z (캐릭터 정면 방향), 살짝 우측
    m_pitch = 0.25f;
    m_dist  = 250.f;
    // Reset character focus position to default so next character-focus frame starts clean.
    m_charFocusPos[0] = 0.f;
    m_charFocusPos[1] = m_floorHeight + 100.f;
    m_charFocusPos[2] = 0.f;
}

void PreviewRenderer::SetFloorHeight(float h)
{
    m_floorHeight = h;

    // Rebuild grid geometry VB with new Y offset.
    if (!m_dev) return;
    if (m_geoVB) { m_geoVB->Release(); m_geoVB = nullptr; }

    std::vector<Vtx> verts = BuildGeometry(m_floorHeight);
    m_geoCount = (int)verts.size();

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (UINT)(m_geoCount * sizeof(Vtx));
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { verts.data() };
    m_dev->CreateBuffer(&bd, &sd, &m_geoVB);
}

// ─── Mesh helpers ─────────────────────────────────────────────────────────────

void PreviewRenderer::LoadMeshes(const std::string& meshFolder)
{
    delete m_mesh;
    m_mesh = nullptr;
    if (!m_dev) return;
    PreviewMesh* mesh = new PreviewMesh();
    if (mesh->Load(m_dev, meshFolder))
        m_mesh = mesh;
    else
        delete mesh;
}

void PreviewRenderer::SetAnim(const ParsedAnim* anim, uint32_t frame)
{
    m_anim  = anim;
    m_frame = frame;
}

int PreviewRenderer::GetMeshPartCount() const
{
    return m_mesh ? m_mesh->GetPartCount() : 0;
}

bool PreviewRenderer::GetMeshTexLoaded() const
{
    return m_mesh && m_mesh->IsTextureLoaded();
}

bool PreviewRenderer::DumpBoneMatrices(const std::string& path, uint32_t frame) const
{
    return m_mesh && m_mesh->DumpBoneMatrices(path, frame);
}

// ─── DrawSkeletonLines ────────────────────────────────────────────────────────
// X-ray skeleton: bone lines (yellow) + joint crosses (cyan), depth-test off.

void PreviewRenderer::DrawSkeletonLines(const float viewProj[16])
{
    std::vector<PreviewMesh::BonePoseInfo> poses;
    m_mesh->GetBonePoses(poses);
    if (poses.empty()) return;

    // Build allowed-bone set from the active category filter.
    // If the filter list is empty (unimplemented categories) draw nothing.
    const std::vector<std::string>& filterList = SkeletonBoneFilter::GetList(m_animCat);
    if (filterList.empty()) return;

    std::unordered_set<std::string> allowed(filterList.begin(), filterList.end());

    struct SkelVtx { float x, y, z, r, g, b; };
    std::vector<SkelVtx> verts;
    verts.reserve(poses.size() * 6);

    // Bone lines: parent → child (yellow).
    // Draw only when the child bone is in the allowed set.
    for (auto& p : poses) {
        if (allowed.find(p.name) == allowed.end()) continue;
        if (p.parentIdx < 0 || p.parentIdx >= (int)poses.size()) continue;
        auto& par = poses[p.parentIdx];
        verts.push_back({ par.pos[0], par.pos[1], par.pos[2], 1.f, 0.80f, 0.10f });
        verts.push_back({ p.pos[0],   p.pos[1],   p.pos[2],   1.f, 0.80f, 0.10f });
    }
    // Joint markers: cross (cyan), 3-unit arm.
    // Draw only for bones in the allowed set.
    const float ks = 3.f;
    for (auto& p : poses) {
        if (allowed.find(p.name) == allowed.end()) continue;
        verts.push_back({ p.pos[0]-ks, p.pos[1],    p.pos[2],    0.2f, 1.f, 0.8f });
        verts.push_back({ p.pos[0]+ks, p.pos[1],    p.pos[2],    0.2f, 1.f, 0.8f });
        verts.push_back({ p.pos[0],    p.pos[1]-ks, p.pos[2],    0.2f, 1.f, 0.8f });
        verts.push_back({ p.pos[0],    p.pos[1]+ks, p.pos[2],    0.2f, 1.f, 0.8f });
    }

    if (verts.empty()) return;

    // Temporary immutable VB
    ID3D11Buffer* vb = nullptr;
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(verts.size() * sizeof(SkelVtx));
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = { verts.data() };
        if (FAILED(m_dev->CreateBuffer(&bd, &sd, &vb))) return;
    }

    // Upload VP (identity model matrix)
    D3D11_MAPPED_SUBRESOURCE ms = {};
    if (SUCCEEDED(m_ctx->Map(m_cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, viewProj, 64);
        m_ctx->Unmap(m_cbuf, 0);
    }

    // X-ray: no depth test → always drawn on top
    m_ctx->OMSetDepthStencilState(m_dssNoDepth, 0);
    m_ctx->IASetInputLayout(m_layout);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    UINT stride = sizeof(SkelVtx), offset = 0;
    m_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_ctx->VSSetShader(m_vs, nullptr, 0);
    m_ctx->VSSetConstantBuffers(0, 1, &m_cbuf);
    m_ctx->PSSetShader(m_ps, nullptr, 0);
    m_ctx->Draw((UINT)verts.size(), 0);

    // Restore normal depth state
    m_ctx->OMSetDepthStencilState(m_dss, 0);
    vb->Release();
}

// ─── DrawHaraRot1Gizmo ───────────────────────────────────────────────────────
// HARA_ROT1의 실제 애니메이션 회전 방향을 마젠타 화살표로 항상 표시.
// X-Ray 오버레이 토글 여부와 무관하게 매 프레임 Render()에서 호출됨.

void PreviewRenderer::DrawHaraRot1Gizmo(const float viewProj[16])
{
    std::vector<PreviewMesh::BonePoseInfo> poses;
    m_mesh->GetBonePoses(poses);
    if (poses.empty()) return;

    struct SkelVtx { float x, y, z, r, g, b; };
    std::vector<SkelVtx> verts;

    const float arrowLen = 40.f;
    const float ks       = 3.f;
    for (auto& p : poses) {
        if (p.fwd[0] == 0.f && p.fwd[1] == 0.f && p.fwd[2] == 0.f) continue;
        float ex = p.pos[0] + p.fwd[0] * arrowLen;
        float ey = p.pos[1] + p.fwd[1] * arrowLen;
        float ez = p.pos[2] + p.fwd[2] * arrowLen;
        // Arrow shaft (magenta)
        verts.push_back({ p.pos[0], p.pos[1], p.pos[2], 1.f, 0.1f, 1.f });
        verts.push_back({ ex,       ey,       ez,        1.f, 0.1f, 1.f });
        // Cross marker at arrow tip
        verts.push_back({ ex-ks, ey,    ez,    1.f, 0.1f, 1.f });
        verts.push_back({ ex+ks, ey,    ez,    1.f, 0.1f, 1.f });
        verts.push_back({ ex,    ey-ks, ez,    1.f, 0.1f, 1.f });
        verts.push_back({ ex,    ey+ks, ez,    1.f, 0.1f, 1.f });
    }
    if (verts.empty()) return;

    ID3D11Buffer* vb = nullptr;
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(verts.size() * sizeof(SkelVtx));
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = { verts.data() };
        if (FAILED(m_dev->CreateBuffer(&bd, &sd, &vb))) return;
    }

    D3D11_MAPPED_SUBRESOURCE ms = {};
    if (SUCCEEDED(m_ctx->Map(m_cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, viewProj, 64);
        m_ctx->Unmap(m_cbuf, 0);
    }

    // Depth-test off so the gizmo is always visible regardless of mesh overlap
    m_ctx->OMSetDepthStencilState(m_dssNoDepth, 0);
    m_ctx->IASetInputLayout(m_layout);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    UINT stride = sizeof(SkelVtx), offset = 0;
    m_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_ctx->VSSetShader(m_vs, nullptr, 0);
    m_ctx->VSSetConstantBuffers(0, 1, &m_cbuf);
    m_ctx->PSSetShader(m_ps, nullptr, 0);
    m_ctx->Draw((UINT)verts.size(), 0);

    m_ctx->OMSetDepthStencilState(m_dss, 0);
    vb->Release();
}
