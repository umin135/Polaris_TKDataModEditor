// PreviewMesh.cpp
#define _CRT_SECURE_NO_WARNINGS
// Phase 5: per-bone OBJ meshes + skeleton.json + shared Diffuse.png texture.
//
// All mesh parts share one "Diffuse.png".
// The pixel shader samples the texture and clips on alpha < 0.5,
// which handles both opaque surfaces and alpha-masked parts (e.g. eyes) uniformly.

#include "PreviewMesh.h"
#include "PanmParser.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wincodec.h>

#include <windows.h>
#include "resource.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <array>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;

// ─── Vertex layout ────────────────────────────────────────────────────────────
// POSITION (float3) + COLOR (float3) + TEXCOORD (float2) = 32 bytes
struct MeshVtx { float x, y, z, r, g, b, u, v; };

// ─── HLSL ─────────────────────────────────────────────────────────────────────

static const char g_meshShader[] = R"HLSL(
cbuffer PerFrame : register(b0)
{
    row_major float4x4 gMVP;
};

Texture2D    gTex  : register(t0);
SamplerState gSamp : register(s0);

struct VSIn  { float3 pos : POSITION; float3 col : COLOR; float2 uv : TEXCOORD; };
struct VSOut { float4 pos : SV_POSITION; float3 col : COLOR0; float2 uv : TEXCOORD0; };

VSOut VS_main(VSIn v)
{
    VSOut o;
    o.pos = mul(float4(v.pos, 1.0f), gMVP);
    o.col = v.col;
    o.uv  = v.uv;
    return o;
}

// Diffuse texture with alpha clip.
// Alpha < 0.5 → pixel discarded (handles eye masks and any transparent region).
float4 PS_main(VSOut v) : SV_TARGET
{
    float4 c = gTex.Sample(gSamp, v.uv);
    clip(c.a - 0.5f);
    return float4(c.rgb * v.col, 1.0f);
}
)HLSL";

// ─── Internal part struct ─────────────────────────────────────────────────────

struct PreviewMesh::MeshPart {
    std::string   boneName;
    int           boneNodeIdx = -1;  // index into PreviewMesh::m_skeleton
    // Bind-pose world matrix (D3D11 Y-up) — used when no animation is playing
    // or when the bone has no matching track in the current PANM.
    XMMATRIX      bindWorld;
    ID3D11Buffer* vb      = nullptr;
    int           vcount  = 0;

    ~MeshPart() { if (vb) { vb->Release(); vb = nullptr; } }
};

// ─── Lifecycle ────────────────────────────────────────────────────────────────

PreviewMesh::PreviewMesh() = default;

PreviewMesh::~PreviewMesh()
{
    m_parts.clear();
    if (m_diffuseSRV) { m_diffuseSRV->Release(); m_diffuseSRV = nullptr; }
    if (m_sampler)    { m_sampler->Release();    m_sampler    = nullptr; }
    if (m_layout)     { m_layout->Release();     m_layout     = nullptr; }
    if (m_ps)         { m_ps->Release();         m_ps         = nullptr; }
    if (m_vs)         { m_vs->Release();         m_vs         = nullptr; }
}

// ─── Pipeline init ────────────────────────────────────────────────────────────

bool PreviewMesh::InitPipeline(ID3D11Device* dev)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* vsBlob  = nullptr;
    ID3DBlob* psBlob  = nullptr;
    ID3DBlob* errBlob = nullptr;

    auto compile = [&](const char* entry, const char* target, ID3DBlob** out) -> bool {
        if (errBlob) { errBlob->Release(); errBlob = nullptr; }
        return SUCCEEDED(D3DCompile(g_meshShader, sizeof(g_meshShader) - 1,
                                    entry, nullptr, nullptr,
                                    entry, target, flags, 0, out, &errBlob));
    };

    if (!compile("VS_main", "vs_5_0", &vsBlob)) goto fail;
    if (!compile("PS_main", "ps_5_0", &psBlob))  goto fail;

    dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    dev->CreatePixelShader (psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);

    {
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        dev->CreateInputLayout(layout, 3,
                               vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                               &m_layout);
    }

    vsBlob->Release();
    psBlob->Release();
    if (errBlob) { errBlob->Release(); errBlob = nullptr; }

    if (!m_vs || !m_ps || !m_layout) return false;

    // Sampler: bilinear wrap
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxLOD   = D3D11_FLOAT32_MAX;
        dev->CreateSamplerState(&sd, &m_sampler);
    }

    return m_sampler != nullptr;

fail:
    if (vsBlob)  vsBlob->Release();
    if (psBlob)  psBlob->Release();
    if (errBlob) errBlob->Release();
    return false;
}

// ─── WIC texture loader ───────────────────────────────────────────────────────
// Loads any WIC-supported image (PNG, etc.) from a memory buffer.
// Converts to DXGI_FORMAT_R8G8B8A8_UNORM.

static bool LoadTextureWIC(ID3D11Device* dev,
                            const void* data, size_t size,
                            ID3D11ShaderResourceView** outSRV)
{
    IWICImagingFactory*    wic       = nullptr;
    IWICStream*            stream    = nullptr;
    IWICBitmapDecoder*     decoder   = nullptr;
    IWICBitmapFrameDecode* frame     = nullptr;
    IWICFormatConverter*   converter = nullptr;
    bool ok = false;

    // CLSID_WICImagingFactory2 is MTA-safe (Win8+); fall back to the original if needed.
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,  nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) goto done;
    if (FAILED(wic->CreateStream(&stream))) goto done;
    if (FAILED(stream->InitializeFromMemory((BYTE*)data, (DWORD)size))) goto done;
    if (FAILED(wic->CreateDecoderFromStream(stream, nullptr,
               WICDecodeMetadataCacheOnLoad, &decoder))) goto done;
    if (FAILED(decoder->GetFrame(0, &frame))) goto done;
    if (FAILED(wic->CreateFormatConverter(&converter))) goto done;
    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
               WICBitmapDitherTypeNone, nullptr, 0.0,
               WICBitmapPaletteTypeCustom))) goto done;

    {
        UINT w = 0, h = 0;
        converter->GetSize(&w, &h);
        std::vector<BYTE> pixels(w * h * 4);
        converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w; td.Height = h;
        td.MipLevels = td.ArraySize = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_IMMUTABLE;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = { pixels.data(), w * 4, 0 };

        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &sd, &tex))) {
            if (SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, outSRV)))
                ok = true;
            tex->Release();
        }
    }

done:
    if (converter) converter->Release();
    if (frame)     frame->Release();
    if (decoder)   decoder->Release();
    if (stream)    stream->Release();
    if (wic)       wic->Release();
    return ok;
}

// ─── Solid colour texture helpers ────────────────────────────────────────────
// alpha=1 so the alpha-clip in the PS never fires.

static bool CreateSolidTexture(ID3D11Device* dev, ID3D11ShaderResourceView** outSRV, uint32_t rgba)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = { &rgba, 4, 0 };

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, outSRV);
    tex->Release();
    return SUCCEEDED(hr);
}

static bool CreateWhiteTexture(ID3D11Device* dev, ID3D11ShaderResourceView** outSRV)
{
    uint32_t white = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = { &white, 4, 0 };

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, outSRV);
    tex->Release();
    return SUCCEEDED(hr);
}

// ─── Skeleton JSON parser ─────────────────────────────────────────────────────

struct SkeletonBone {
    int         index      = -1;
    std::string name;
    std::string parentName;    // empty string = root (no parent)
    float       mat[16]    = {};
};

// Parses skeleton.json produced by export_skeleton.py.
// Supports both old format (no "parent") and new format (with "parent": name|null).
static std::vector<SkeletonBone> ParseSkeletonJsonText(const std::string& text)
{
    std::vector<SkeletonBone> result;
    const char* p = text.c_str();

    while (true) {
        p = strstr(p, "\"index\":");
        if (!p) break;
        p += 8;

        SkeletonBone b;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        b.index = (int)strtol(p, (char**)&p, 10);

        // Find the next three tags in this entry.
        const char* nameTag   = strstr(p, "\"name\":");
        const char* parentTag = strstr(p, "\"parent\":");
        const char* matTag    = strstr(p, "\"world_matrix\":");
        if (!nameTag || !matTag || nameTag > matTag) break;

        // ── name ──────────────────────────────────────────────────
        nameTag += 7;
        while (*nameTag && *nameTag != '"') ++nameTag;
        ++nameTag;
        const char* nameEnd = strchr(nameTag, '"');
        if (!nameEnd) break;
        b.name.assign(nameTag, nameEnd);

        // ── parent (optional; present only in new-format JSON) ────
        if (parentTag && parentTag < matTag) {
            parentTag += 9;  // skip past "parent":
            while (*parentTag == ' ' || *parentTag == '\t') ++parentTag;
            if (*parentTag == '"') {
                // Quoted string — e.g. "parent": "Trans"
                ++parentTag;
                const char* parentEnd = strchr(parentTag, '"');
                if (parentEnd) b.parentName.assign(parentTag, parentEnd);
            }
            // else: null — leave parentName empty (root)
        }

        // ── world_matrix (16 floats) ───────────────────────────────
        const char* q = matTag + 15;
        int floatCount = 0;
        while (floatCount < 16 && *q) {
            while (*q && *q != '-' && *q != '+' && !isdigit((unsigned char)*q)) ++q;
            if (!*q) break;
            char* end;
            b.mat[floatCount++] = strtof(q, &end);
            q = end;
        }

        if (floatCount == 16)
            result.push_back(b);
        p = nameEnd + 1;
    }
    return result;
}

// ─── OBJ loader ───────────────────────────────────────────────────────────────
// Parses v / vt / f lines from a string buffer.
// Face format: f v  /  f v/vt  /  f v/vt/vn  /  f v//vn
// Fan-triangulates polygons.

static bool ParseOBJ(ID3D11Device* dev, const std::string& text,
                     ID3D11Buffer** outVB, int* outCount)
{
    std::vector<std::array<float, 3>> pos;
    std::vector<std::array<float, 2>> uvs;
    std::vector<MeshVtx>              verts;
    pos.reserve(512);
    uvs.reserve(512);
    verts.reserve(1024);

    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.size() < 2) continue;

        if (line[0] == 'v' && line[1] == ' ') {
            float x = 0, y = 0, z = 0;
            if (sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3)
                pos.push_back({ x, y, z });
        }
        else if (line[0] == 'v' && line[1] == 't') {
            float u = 0, vt = 0;
            if (sscanf(line.c_str() + 2, "%f %f", &u, &vt) >= 1)
                uvs.push_back({ u, 1.0f - vt });  // flip V: OBJ top-left → DX top-left
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            struct FV { int vi, vti; };
            std::vector<FV> fverts;
            const char* p = line.c_str() + 2;
            while (*p) {
                while (*p == ' ' || *p == '\t') ++p;
                if (!*p || *p == '\n' || *p == '\r') break;
                if (!isdigit((unsigned char)*p)) { ++p; continue; }

                int vi  = (int)strtol(p, (char**)&p, 10) - 1;
                int vti = -1;
                if (*p == '/') {
                    ++p;
                    if (*p != '/' && isdigit((unsigned char)*p))
                        vti = (int)strtol(p, (char**)&p, 10) - 1;
                    if (*p == '/') { ++p; while (isdigit((unsigned char)*p)) ++p; }
                }
                fverts.push_back({ vi, vti });
            }

            for (int i = 1; i + 1 < (int)fverts.size(); ++i) {
                for (int k : { 0, i, i + 1 }) {
                    int vi  = fverts[k].vi;
                    int vti = fverts[k].vti;
                    if (vi < 0 || vi >= (int)pos.size()) continue;
                    float u = 0.f, v2 = 0.f;
                    if (vti >= 0 && vti < (int)uvs.size()) {
                        u  = uvs[vti][0];
                        v2 = uvs[vti][1];
                    }
                    // Vertex color = white; Diffuse.png provides all colour
                    verts.push_back({ pos[vi][0], pos[vi][1], pos[vi][2],
                                      1.f, 1.f, 1.f, u, v2 });
                }
            }
        }
    }

    if (verts.empty()) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (UINT)(verts.size() * sizeof(MeshVtx));
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { verts.data() };
    if (FAILED(dev->CreateBuffer(&bd, &sd, outVB))) return false;

    *outCount = (int)verts.size();
    return true;
}

// ─── Pack file reader ─────────────────────────────────────────────────────────

struct PackEntry {
    std::string name;
    std::string data;
};

static std::vector<PackEntry> ReadPack(const uint8_t* buf, size_t size)
{
    std::vector<PackEntry> entries;
    if (size < 12 || memcmp(buf, "PMSH", 4) != 0) return entries;

    uint32_t version = 0, count = 0;
    memcpy(&version, buf + 4, 4);
    memcpy(&count,   buf + 8, 4);
    if (version != 1) return entries;

    size_t pos = 12;
    for (uint32_t i = 0; i < count && pos < size; ++i) {
        if (pos + 4 > size) break;
        uint32_t nameLen = 0;
        memcpy(&nameLen, buf + pos, 4);  pos += 4;
        if (pos + nameLen > size) break;
        PackEntry e;
        e.name.assign((const char*)buf + pos, nameLen);  pos += nameLen;

        if (pos + 4 > size) break;
        uint32_t dataLen = 0;
        memcpy(&dataLen, buf + pos, 4);  pos += 4;
        if (pos + dataLen > size) break;
        e.data.assign((const char*)buf + pos, dataLen);  pos += dataLen;

        entries.push_back(std::move(e));
    }
    return entries;
}

// ─── Per-bone animation correction profiles ──────────────────────────────────
// Mirrors Blender addon's profiles_tk8.py FULLBODY_GROUPS.
// _process_frame() applies:
//   1. anim_quat = raw_quat * offset_quat
//   2. M_blender = C_mat @ TRS(anim_quat) @ C_inv
//   3. b_rot = decompose(M_blender)
//   4. b_rot = apose_quat * b_rot   (left-multiply, A-pose correction)
//   5. b_rot = b_rot * post_rot     (right-multiply)

struct BoneAnimProfile {
    float basisX, basisY, basisZ;    // XYZ Euler degrees → C_mat
    float offsetX, offsetY, offsetZ; // XYZ Euler degrees → offset_quat
    float postX,   postY,   postZ;   // XYZ Euler degrees → post_rot_quat
    float aposeX,  aposeY,  aposeZ;  // XYZ Euler degrees → apose_quat
};

static const BoneAnimProfile kDefaultProfile = {
    0.f, 0.f, 90.f,   // basis
    0.f, 0.f, 0.f,    // offset
    0.f, 0.f, 0.f,    // post_rot
    0.f, 0.f, 0.f,    // apose
};

static BoneAnimProfile GetBoneAnimProfile(const std::string& name)
{
    // A-pose offsets (left-multiply after C_mat decompose)
    // Source: APOSE_OFFSETS in profiles_tk8.py
    static const std::unordered_map<std::string, std::array<float,3>> kApose = {
        { "L_Shoulder",  { -10.f,       0.f,       0.f } },
        { "R_Shoulder",  {  10.f,       0.f,       0.f } },
        { "L_UpperArm",  { -35.f,       0.f,       0.f } },
        { "R_UpperArm",  {  35.f,       0.f,       0.f } },
        { "L_LowerArm",  {   0.f,       0.f, -15.f     } },
        { "R_LowerArm",  {   0.f,       0.f, -15.f     } },
        { "L_UpperLeg",  {  -5.99981f,  0.f,       0.f } },
        { "R_UpperLeg",  {   5.99981f,  0.f,       0.f } },
        { "L_Foot",      {   5.99909f,  0.f,       0.f } },
        { "R_Foot",      {  -5.99909f,  0.f,       0.f } },
    };

    BoneAnimProfile p = kDefaultProfile;

    // Non-default basis/offset/post_rot
    if      (name == "Hip")
        p = { 0.f,-90.f, 0.f,  0.f,-90.f,-90.f,  0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "R_Shoulder")
        p = { 0.f,-90.f,90.f,  0.f,-90.f,  0.f,  0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "L_Shoulder")
        p = { 0.f, 90.f,90.f,  0.f, 90.f,  0.f,  0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "R_Hand")
        p = { 90.f,0.f,90.f,  90.f,0.f,0.f,       0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "L_Hand")
        p = {-90.f,0.f,90.f, -90.f,0.f,0.f,       0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "R_UpperLeg")
        p = { 0.f,0.f,-90.f,  0.f,0.f,180.f,      0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "L_UpperLeg")
        p = { 0.f,0.f,-90.f,  0.f,0.f,180.f,      0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "R_Toe")
        p = { 90.f,0.f,0.f,   0.f,0.f, 90.f,      0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "L_Toe")
        p = { 90.f,0.f,0.f,   0.f,0.f, 90.f,      0.f,0.f,0.f,  0.f,0.f,0.f };
    else if (name == "HARA_ROT1")
        p = { 0.f,0.f,90.f,   0.f,0.f,  0.f,      0.f,0.f,180.f,0.f,0.f,0.f };

    // Apply A-pose correction
    auto it = kApose.find(name);
    if (it != kApose.end()) {
        p.aposeX = it->second[0];
        p.aposeY = it->second[1];
        p.aposeZ = it->second[2];
    }

    return p;
}

// XYZ Euler (degrees) → rotation matrix (D3D11 row-major = XMMATRIX default)
static XMMATRIX EulerXYZDegToMatrix(float x, float y, float z)
{
    return XMMatrixRotationX(XMConvertToRadians(x))
         * XMMatrixRotationY(XMConvertToRadians(y))
         * XMMatrixRotationZ(XMConvertToRadians(z));
}

// XYZ Euler (degrees) → quaternion (x,y,z,w stored as XMVECTOR)
static XMVECTOR EulerXYZDegToQuat(float x, float y, float z)
{
    return XMQuaternionRotationMatrix(EulerXYZDegToMatrix(x, y, z));
}

// scale_div 역수 조회: 모든 본 1.0 (PANM 단위 = 씬 단위)
static float GetScaleDivInv(const std::string& /*name*/)
{
    return 1.0f;
}

// ─── Skeleton + part build ────────────────────────────────────────────────────
// Builds m_skeleton (all bones, topological order) and m_parts (renderable meshes)
// from skeleton JSON + OBJ content map.
// Texture loading is handled separately in Load().

static bool BuildSkeleton(
    ID3D11Device* dev,
    const std::string& jsonText,
    const std::unordered_map<std::string, std::string>& objMap,
    std::vector<std::unique_ptr<PreviewMesh::MeshPart>>& outParts,
    std::vector<PreviewMesh::BoneNode>&                  outSkeleton)
{
    auto rawBones = ParseSkeletonJsonText(jsonText);
    if (rawBones.empty()) return false;

    // Sort by index so parent bones always come before their children.
    std::sort(rawBones.begin(), rawBones.end(),
              [](const SkeletonBone& a, const SkeletonBone& b){ return a.index < b.index; });

    // Build name → sorted position map for O(1) parent lookup.
    std::unordered_map<std::string, int> nameToSortedIdx;
    nameToSortedIdx.reserve(rawBones.size());
    for (int i = 0; i < (int)rawBones.size(); ++i)
        nameToSortedIdx[rawBones[i].name] = i;

    // ── Pass 1: read D3D11 bindWorld for each bone ───────────────────────────
    // skeleton.json (Method C) = Blender Armature rest pose, D3D11 Y-up row-major.
    // No correction needed.
    std::vector<XMMATRIX> bindWorldVec(rawBones.size());
    for (int i = 0; i < (int)rawBones.size(); ++i) {
        const float* m = rawBones[i].mat;
        bindWorldVec[i] = XMMatrixSet(
            m[0],  m[1],  m[2],  m[3],
            m[4],  m[5],  m[6],  m[7],
            m[8],  m[9],  m[10], m[11],
            m[12], m[13], m[14], m[15]
        );
    }

    // ── Pass 2: build BoneNode list with parentIdx and bindLocal ─────────────
    outSkeleton.resize(rawBones.size());
    for (int i = 0; i < (int)rawBones.size(); ++i) {
        PreviewMesh::BoneNode& bn = outSkeleton[i];
        bn.name     = rawBones[i].name;
        bn.partIdx  = -1;  // filled in pass 3

        // Resolve parent
        const std::string& pname = rawBones[i].parentName;
        if (!pname.empty()) {
            auto it = nameToSortedIdx.find(pname);
            bn.parentIdx = (it != nameToSortedIdx.end()) ? it->second : -1;
        } else {
            bn.parentIdx = -1;  // root
        }

        // Store bindWorld as plain floats
        memcpy(bn.bindWorld, &bindWorldVec[i], 64);

        // Compute bindLocal
        // D3D11 row-vector convention: world[i] = local[i] * world[parent[i]]
        // → local[i] = world[i] * inverse(world[parent[i]])
        XMMATRIX bindLocal;
        if (bn.parentIdx < 0) {
            bindLocal = bindWorldVec[i];  // root: local = world
        } else {
            XMMATRIX invParent = XMMatrixInverse(nullptr, bindWorldVec[bn.parentIdx]);
            bindLocal = XMMatrixMultiply(bindWorldVec[i], invParent);
        }
        memcpy(bn.bindLocal, &bindLocal, 64);

        bn.scaleDivInv = GetScaleDivInv(bn.name);

        // Per-bone animation correction (from FULLBODY profiles, mirrors profiles_tk8.py)
        BoneAnimProfile prof = GetBoneAnimProfile(bn.name);

        // Draw() uses Blender col-major sandwich: Ci * Meng * C
        // where cMat = EulerXYZDegToMatrix(basis) = C_bl^T numerically (D3D11 Rx*Ry*Rz)
        //       cInv = cMat^{-1} = cMat^T = C_bl numerically
        // No P-sandwich here — the pswap in Draw() handles the Blender→D3D11 conversion.
        XMMATRIX C  = EulerXYZDegToMatrix(prof.basisX, prof.basisY, prof.basisZ);
        XMMATRIX Ci = XMMatrixInverse(nullptr, C);
        memcpy(bn.cMat, &C,  64);
        memcpy(bn.cInv, &Ci, 64);

        // Blender quaternions: EulerXYZDegToQuat = bl_quat(x,y,z) in Python.
        // Applied in Blender quat space in Draw() before pswap converts to D3D11.
        XMVECTOR offQ  = EulerXYZDegToQuat(prof.offsetX, prof.offsetY, prof.offsetZ);
        XMVECTOR posQ  = EulerXYZDegToQuat(prof.postX,   prof.postY,   prof.postZ);
        XMVECTOR apoQ  = EulerXYZDegToQuat(prof.aposeX,  prof.aposeY,  prof.aposeZ);
        XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(bn.offsetQuat),  offQ);
        XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(bn.postRotQuat), posQ);
        XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(bn.aposeQuat),   apoQ);
    }

    // ── Pass 3: create MeshPart for each bone that has an OBJ ────────────────
    for (int i = 0; i < (int)rawBones.size(); ++i) {
        auto it = objMap.find(rawBones[i].name + ".obj");
        if (it == objMap.end()) continue;

        ID3D11Buffer* vb     = nullptr;
        int           vcount = 0;
        if (!ParseOBJ(dev, it->second, &vb, &vcount)) continue;

        int partIdx = (int)outParts.size();
        outSkeleton[i].partIdx = partIdx;

        auto part          = std::make_unique<PreviewMesh::MeshPart>();
        part->boneName     = rawBones[i].name;
        part->boneNodeIdx  = i;
        part->bindWorld    = bindWorldVec[i];
        part->vb           = vb;
        part->vcount       = vcount;
        outParts.push_back(std::move(part));
    }

    return !outParts.empty();
}

// ─── PreviewMesh::Load ────────────────────────────────────────────────────────

bool PreviewMesh::Load(ID3D11Device* dev, const std::string& meshFolder, bool isHand)
{
    m_parts.clear();
    m_skeleton.clear();
    m_loaded    = false;
    m_texLoaded = false;

    // COM required by WIC texture loader.
    // S_OK = we initialized; S_FALSE = already initialized same model; RPC_E_CHANGED_MODE = already MTA.
    // All cases: COM is available on this thread.
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (!InitPipeline(dev)) goto cleanup;

    // ── Read embedded resource pack once (used for both geometry and texture fallback) ──
    // Parsed upfront so both the disk-geometry path and the full-resource path can use it.
    {
        struct ResourcePack {
            std::string jsonText;
            std::unordered_map<std::string, std::string> objMap;
            std::string pngData;
            bool valid = false;
        } resPack;

        UINT    resId = isHand ? IDR_PREVIEW_MESHES_HAND : IDR_PREVIEW_MESHES;
        HRSRC   hRsrc = FindResource(nullptr, MAKEINTRESOURCE(resId), RT_RCDATA);
        HGLOBAL hGlob = hRsrc ? LoadResource(nullptr, hRsrc) : nullptr;
        const uint8_t* pData = hGlob ? static_cast<const uint8_t*>(LockResource(hGlob)) : nullptr;
        DWORD   dSize = hRsrc ? SizeofResource(nullptr, hRsrc) : 0;

        if (pData && dSize > 0) {
            for (auto& e : ReadPack(pData, (size_t)dSize)) {
                if (e.name == "skeleton.json") {
                    resPack.jsonText = e.data;
                    resPack.valid    = true;
                } else if (e.name == "Diffuse.png") {
                    resPack.pngData = e.data;
                } else if (e.name.size() > 4 &&
                           e.name.compare(e.name.size() - 4, 4, ".obj") == 0) {
                    resPack.objMap[e.name] = e.data;
                }
            }
        }

        // ── Helper: load Diffuse.png from disk → resource fallback ────
        // Hand meshes always use a flat grey (0.5, 0.5, 0.5) colour; no texture lookup.
        auto LoadDiffuse = [&](const std::string& diskFolder) {
            if (isHand) {
                // 0x80 per channel ≈ 0.5 linear (hand meshes, no texture)
                CreateSolidTexture(dev, &m_diffuseSRV, 0xFF808080u);
                return;
            }
            // Try disk first
            if (!diskFolder.empty()) {
                std::string p = diskFolder + "/Diffuse.png";
                std::ifstream f(p, std::ios::binary);
                if (f.is_open()) {
                    std::string data((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());
                    if (LoadTextureWIC(dev, data.data(), data.size(), &m_diffuseSRV)) {
                        m_texLoaded = true;
                        return;
                    }
                }
            }
            // Try embedded resource
            if (!resPack.pngData.empty())
                if (LoadTextureWIC(dev, resPack.pngData.data(),
                                   resPack.pngData.size(), &m_diffuseSRV))
                    m_texLoaded = true;
            // Last resort: 1x1 white (alpha=1, clip never fires)
            if (!m_diffuseSRV)
                CreateWhiteTexture(dev, &m_diffuseSRV);
        };

        // ── Try disk geometry first (developer workflow) ───────────
        {
            std::string jsonPath = meshFolder + "/skeleton.json";
            std::ifstream jf(jsonPath, std::ios::binary);
            if (jf.is_open()) {
                std::string jsonText((std::istreambuf_iterator<char>(jf)),
                                      std::istreambuf_iterator<char>());
                auto bones = ParseSkeletonJsonText(jsonText);
                std::unordered_map<std::string, std::string> objMap;
                for (auto& b : bones) {
                    std::string p = meshFolder + "/" + b.name + ".obj";
                    std::ifstream f(p, std::ios::binary);
                    if (f.is_open())
                        objMap[b.name + ".obj"] = std::string(
                            (std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
                }
                if (BuildSkeleton(dev, jsonText, objMap, m_parts, m_skeleton)) {
                    LoadDiffuse(meshFolder);
                    m_loaded = (m_diffuseSRV != nullptr);
                    goto cleanup;
                }
            }
        }

        // ── Fall back to embedded resource geometry ───────────────
        if (resPack.valid && BuildSkeleton(dev, resPack.jsonText, resPack.objMap, m_parts, m_skeleton)) {
            LoadDiffuse("");  // disk folder empty → goes straight to resource
            m_loaded = (m_diffuseSRV != nullptr);
        }
    }

cleanup:
    if (hrCo == S_OK) CoUninitialize();
    return m_loaded;
}

// ─── PreviewMesh::Draw ────────────────────────────────────────────────────────
//
// Animation hierarchy:
//   We traverse m_skeleton in topological order (parent always before child)
//   and accumulate world transforms:
//
//     local[i]     = R(d3d_quat[i]) * bindLocal[i]          (non-root-motion bones)
//     local[i]     = R(d3d_quat[i]) * bindLocal[i] + T_local (Trans/Top root motion)
//     animWorld[i] = local[i] * animWorld[parent[i]]         (root: just local)
//
//   Rotation pipeline (mirrors core_tk8.py _process_frame, Blender col-major convention):
//     animQ   = rawQ * offsetQ
//     Meng    = R(animQ)                                     (rotation only)
//     Mbl_T   = Ci * Meng * C                               (= M_bl^T where M_bl = C_col @ R_col @ Ci_col)
//     q_bl    = QFromMat(Mbl_T)                             (Blender quaternion)
//     q_bl    = apoQ * q_bl * postRotQ
//     d3d_quat = pswap(q_bl) = (-q_bl.x, -q_bl.z, -q_bl.y, q_bl.w)
//
//   Root motion translation (Trans / Top only):
//     PANM position = world-space D3D11 displacement.
//     local_disp = panm_pos * inv(parent_world_rot)          (parent-local frame)
//     local[3,:3] += local_disp
//
//   Bones with no PANM track fall back to their stored bindLocal matrix.

void PreviewMesh::Draw(ID3D11DeviceContext* ctx, ID3D11Buffer* cbuf,
                       const ParsedAnim* anim, uint32_t frame,
                       const float viewProj[16])
{
    if (m_parts.empty() || !m_vs || !m_diffuseSRV) return;

    XMMATRIX vp;
    memcpy(&vp, viewProj, sizeof(XMMATRIX));

    // ── Compute per-bone animated world matrices ──────────────────────────────
    // m_skeleton is in topological order (parent index < child index),
    // so a single forward pass is sufficient.
    const int boneCount = (int)m_skeleton.size();
    std::vector<XMMATRIX> animWorld(boneCount);

    for (int i = 0; i < boneCount; ++i) {
        const BoneNode& bn = m_skeleton[i];

        XMMATRIX local;
        if (anim) {
            const BoneTrack* track = anim->FindBone(bn.name);
            if (bn.name == "HARA_ROT1") {
                // HARA_ROT1: body tilt/twist 전용.
                // 회전은 bind pose로 고정 (자식 본 체인 보존).
                // 위치는 항상 bindLocal 사용 (root motion 본이 아님).
                // 실제 회전 방향은 gizmo arrow로 별도 표시.
                memcpy(&local, bn.bindLocal, 64);
            } else if (track && !track->frames.empty()) {
                uint32_t f = (frame < (uint32_t)track->frames.size())
                             ? frame : (uint32_t)track->frames.size() - 1;
                const BoneSample& s = track->frames[f];

                // Per-bone correction (see Draw() header comment for full pipeline).

                XMVECTOR rawQ  = XMVectorSet(s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3]);
                XMVECTOR offQ  = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(bn.offsetQuat));
                // NOTE: XMQuaternionMultiply(A,B) = B*A in Hamilton convention.
                // Python: animQ = qmul(rawQ, offQ) = rawQ*offQ → XMQMul(offQ, rawQ).
                XMVECTOR animQ = XMQuaternionMultiply(offQ, rawQ);

                XMMATRIX C    = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(bn.cMat));
                XMMATRIX Ci   = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(bn.cInv));

                bool isRootMotion = (bn.name == "Trans" || bn.name == "Top");

                // Rotation-only engine matrix; root motion translation handled separately.
                XMMATRIX Meng = XMMatrixRotationQuaternion(animQ);

                // Blender col-major sandwich: Ci * Meng * C
                // (cMat = Rx*Ry*Rz in D3D11 = C_bl^T numerically;
                //  cInv = C_bl stored row-major = its transpose)
                // Gives M_bl^T; XMQuaternionRotationMatrix extracts the Blender quat q_bl.
                XMMATRIX Mbl_T = XMMatrixMultiply(XMMatrixMultiply(Ci, Meng), C);

                XMVECTOR q_bl = XMQuaternionNormalize(XMQuaternionRotationMatrix(Mbl_T));

                // A-pose: left-multiply in Blender quat space → XMQMul(q_bl, apoQ) = apoQ*q_bl.
                // post_rot: right-multiply → XMQMul(posQ, q_bl) = q_bl*posQ.
                XMVECTOR apoQ  = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(bn.aposeQuat));
                q_bl = XMQuaternionMultiply(q_bl, apoQ);
                XMVECTOR posQ  = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(bn.postRotQuat));
                q_bl = XMQuaternionNormalize(XMQuaternionMultiply(posQ, q_bl));

                // pswap: Blender quat → D3D11 quat = (-x, -z, -y, w)
                // Full P-sandwich in quaternion space (Y↔Z swap + negate imaginary parts).
                XMFLOAT4 q_f; XMStoreFloat4(&q_f, q_bl);
                XMVECTOR d3d_quat = XMQuaternionNormalize(
                    XMVectorSet(-q_f.x, -q_f.z, -q_f.y, q_f.w));

                XMMATRIX bindL;
                memcpy(&bindL, bn.bindLocal, 64);

                if (isRootMotion) {
                    // PANM position is world-space D3D11 displacement (already Y-up).
                    // Convert to parent-local frame: panm_pos @ inv(parent_world_rot).
                    // PANM position is stored in TK8 right-handed Y-up space;
                    // negate Z to convert to D3D11 left-handed Y-up.
                    XMVECTOR panmPos = XMVectorSet(
                        s.position[0], s.position[1], -s.position[2], 0.f);
                    XMVECTOR localDisp;
                    if (bn.parentIdx >= 0) {
                        // Rotation is orthogonal → inverse = transpose (upper 3x3 only)
                        XMMATRIX parInvRot = XMMatrixTranspose(animWorld[bn.parentIdx]);
                        localDisp = XMVector3TransformNormal(panmPos, parInvRot);
                    } else {
                        localDisp = panmPos;
                    }
                    local = XMMatrixMultiply(XMMatrixRotationQuaternion(d3d_quat), bindL);
                    XMFLOAT4X4 lf; XMStoreFloat4x4(&lf, local);
                    XMFLOAT4 ld;   XMStoreFloat4(&ld, localDisp);
                    lf._41 += ld.x; lf._42 += ld.y; lf._43 += ld.z;
                    local = XMLoadFloat4x4(&lf);
                } else {
                    local = XMMatrixMultiply(XMMatrixRotationQuaternion(d3d_quat), bindL);
                }
            } else {
                // No PANM track → stay at bind-local pose
                memcpy(&local, bn.bindLocal, 64);
            }
        } else {
            // No animation → bind local
            memcpy(&local, bn.bindLocal, 64);
        }

        if (bn.parentIdx < 0) {
            animWorld[i] = local;
        } else {
            // D3D11 row-vector: world = local * parentWorld
            animWorld[i] = XMMatrixMultiply(local, animWorld[bn.parentIdx]);
        }
    }

    // ── Cache bone world matrices for skeleton visualization ──────────────────
    m_lastAnimWorld.resize(boneCount);
    for (int i = 0; i < boneCount; ++i)
        memcpy(m_lastAnimWorld[i].data(), &animWorld[i], 64);

    // ── HARA_ROT1 gizmo: overwrite m_lastAnimWorld entry with ANIMATED world ──
    // Draw() uses bindLocal for HARA_ROT1 in the hierarchy (children unaffected),
    // but GetBonePoses needs the actual animated orientation to drive the gizmo arrow.
    if (anim) {
        for (int i = 0; i < boneCount; ++i) {
            if (m_skeleton[i].name != "HARA_ROT1") continue;
            const BoneTrack* track = anim->FindBone("HARA_ROT1");
            if (!track || track->frames.empty()) break;
            uint32_t f = frame < (uint32_t)track->frames.size()
                       ? frame : (uint32_t)track->frames.size() - 1;
            const BoneSample& s    = track->frames[f];
            const BoneNode&   gbn  = m_skeleton[i];
            // HARA_ROT1은 root motion 본이 아님 → 위치는 항상 bindLocal (gizmo only)
            XMVECTOR gRawQ  = XMVectorSet(s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3]);
            XMVECTOR gOffQ  = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(gbn.offsetQuat));
            XMVECTOR gAnimQ = XMQuaternionMultiply(gOffQ, gRawQ);  // gRawQ*gOffQ in Hamilton
            XMMATRIX gC     = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(gbn.cMat));
            XMMATRIX gCi    = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(gbn.cInv));
            XMMATRIX gMeng  = XMMatrixRotationQuaternion(gAnimQ);
            // Same Ci * M * C pipeline + pswap as normal bones
            XMMATRIX gMbl_T = XMMatrixMultiply(XMMatrixMultiply(gCi, gMeng), gC);
            XMVECTOR g_bl   = XMQuaternionNormalize(XMQuaternionRotationMatrix(gMbl_T));
            XMVECTOR gPosQ  = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(gbn.postRotQuat));
            g_bl = XMQuaternionNormalize(XMQuaternionMultiply(gPosQ, g_bl));  // g_bl*gPosQ in Hamilton
            // pswap: Blender → D3D11
            XMFLOAT4 gqf; XMStoreFloat4(&gqf, g_bl);
            XMVECTOR g_rot = XMQuaternionNormalize(XMVectorSet(-gqf.x, -gqf.z, -gqf.y, gqf.w));
            XMMATRIX gBindL;
            memcpy(&gBindL, gbn.bindLocal, 64);
            XMMATRIX animLocal = XMMatrixMultiply(XMMatrixRotationQuaternion(g_rot), gBindL);
            XMMATRIX animWrld = (m_skeleton[i].parentIdx >= 0)
                ? XMMatrixMultiply(animLocal, animWorld[m_skeleton[i].parentIdx])
                : animLocal;
            memcpy(m_lastAnimWorld[i].data(), &animWrld, 64);
            break;
        }
    }

    // ── Set shared pipeline state ─────────────────────────────────────────────
    ctx->IASetInputLayout(m_layout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &cbuf);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &m_sampler);
    ctx->PSSetShaderResources(0, 1, &m_diffuseSRV);

    // ── Draw each mesh part ───────────────────────────────────────────────────
    for (auto& part : m_parts) {
        XMMATRIX model;
        if (anim && part->boneNodeIdx >= 0 && part->boneNodeIdx < boneCount) {
            model = animWorld[part->boneNodeIdx];
        } else {
            model = part->bindWorld;
        }

        XMMATRIX mvp = XMMatrixMultiply(model, vp);
        D3D11_MAPPED_SUBRESOURCE ms = {};
        if (SUCCEEDED(ctx->Map(cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, &mvp, sizeof(XMMATRIX));
            ctx->Unmap(cbuf, 0);
        }

        UINT stride = sizeof(MeshVtx), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &part->vb, &stride, &offset);
        ctx->Draw((UINT)part->vcount, 0);
    }

    // Unbind SRV to avoid RT/texture conflicts next frame
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
}

// ─── PreviewMesh::GetBonePoses ────────────────────────────────────────────────

void PreviewMesh::GetBonePoses(std::vector<BonePoseInfo>& out) const
{
    const int n = (int)m_skeleton.size();
    if ((int)m_lastAnimWorld.size() != n) { out.clear(); return; }
    out.resize(n);
    for (int i = 0; i < n; ++i) {
        // Row-major layout: translation is at indices [12], [13], [14] (row 3)
        out[i].pos[0]    = m_lastAnimWorld[i][12];
        out[i].pos[1]    = m_lastAnimWorld[i][13];
        out[i].pos[2]    = m_lastAnimWorld[i][14];
        out[i].parentIdx = m_skeleton[i].parentIdx;
        out[i].name      = m_skeleton[i].name;
        out[i].fwd[0]    = 0.f;
        out[i].fwd[1]    = 0.f;
        out[i].fwd[2]    = 0.f;

        if (m_skeleton[i].name == "HARA_ROT1") {
            // Row 2 = local Z-axis in world space (row-major D3D11).
            // m_lastAnimWorld for HARA_ROT1 was overwritten with the animated world
            // matrix in Draw(), so this reflects the current animation frame.
            out[i].fwd[0] = m_lastAnimWorld[i][8];
            out[i].fwd[1] = m_lastAnimWorld[i][9];
            out[i].fwd[2] = m_lastAnimWorld[i][10];
        }
    }
}

// ─── PreviewMesh::DumpBoneMatrices ────────────────────────────────────────────
// Debug helper: write m_lastAnimWorld (D3D11 Y-up row-major) to a JSON file
// that can be compared with blender_dump.py output using compare_dumps.py.

bool PreviewMesh::DumpBoneMatrices(const std::string& path, uint32_t frame) const
{
    const int n = (int)m_skeleton.size();
    if ((int)m_lastAnimWorld.size() != n || n == 0) return false;

    std::ofstream f(path);
    if (!f) return false;

    f << "{\n";
    f << "  \"frame\": " << frame << ",\n";
    f << "  \"bones\": [\n";
    for (int i = 0; i < n; ++i) {
        const auto& w = m_lastAnimWorld[i];
        f << "    {\n";
        f << "      \"name\": \"" << m_skeleton[i].name << "\",\n";
        // Row-major 4x4: rows 0..3, each 4 floats
        f << "      \"world_matrix\": [\n";
        for (int r = 0; r < 4; ++r) {
            f << "        [";
            for (int c = 0; c < 4; ++c) {
                f << w[r * 4 + c];
                if (c < 3) f << ", ";
            }
            f << "]";
            if (r < 3) f << ",";
            f << "\n";
        }
        f << "      ],\n";
        f << "      \"pos_d3d11\": [" << w[12] << ", " << w[13] << ", " << w[14] << "]\n";
        f << "    }";
        if (i < n - 1) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";
    return true;
}
