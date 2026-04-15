#pragma once
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <cstdint>

// Forward declarations — keeps d3d11.h and PanmParser.h out of the header chain.
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;
struct ParsedAnim;

// Rigid per-bone mesh parts for the 3D preview.
//
//  Usage:
//    Load(dev, meshFolder)    — reads skeleton.json + *.obj + Diffuse.png
//    Draw(ctx, cbuf, ...)     — called from PreviewRenderer::Render() each frame
//
//  Animation:
//    PANM stores LOCAL (parent-relative) transforms. Draw() traverses the bone
//    hierarchy (BoneNode::parentIdx) to accumulate world transforms each frame.
//
//  Texture:
//    One shared "Diffuse.png" is used for all mesh parts.
//    Alpha channel is used for clip-masking (clip at 0.5).
//    If Diffuse.png is missing, a 1x1 white fallback is used (no clip).
//
//  PreviewMesh owns its own D3D pipeline (VS + PS + InputLayout + Sampler).
//  It shares only the cbuf (b0) and rasterizer/depth state set by PreviewRenderer.
class PreviewMesh {
public:
    PreviewMesh();
    ~PreviewMesh();  // defined in .cpp (MeshPart / BoneNode complete types required)

    // Scan meshFolder for skeleton.json + *.obj + Diffuse.png.
    // Returns true if at least one part was loaded.
    bool Load(ID3D11Device* dev, const std::string& meshFolder);

    bool IsLoaded()        const { return m_loaded; }
    int  GetPartCount()    const { return (int)m_parts.size(); }
    bool IsTextureLoaded() const { return m_texLoaded; }  // false = using white fallback

    // Draw all loaded parts.
    //   cbuf     — shared 64-byte MVP constant buffer (updated per part)
    //   anim     — current animation; nullptr draws in bind pose
    //   frame    — frame index within the animation
    //   viewProj — row-major 4x4 float array (view * projection)
    void Draw(ID3D11DeviceContext* ctx, ID3D11Buffer* cbuf,
              const ParsedAnim* anim, uint32_t frame,
              const float viewProj[16]);

    // Per-bone world-space info cached from the last Draw() call.
    // Used by PreviewRenderer to draw skeleton lines.
    struct BonePoseInfo {
        float pos[3];       // world-space origin of this bone
        float fwd[3];       // local +Z axis in world space (non-zero for HARA_ROT1 only,
                            //   filled with ANIMATED world to drive the gizmo arrow)
        int   parentIdx;
    };
    // Fills 'out' with one entry per bone. Empty if Draw() has not yet been called.
    void GetBonePoses(std::vector<BonePoseInfo>& out) const;

    // Both structs are public so the file-local BuildSkeleton helper can reference them.
    struct MeshPart;

    // One entry per bone (ALL bones, including virtual ones with no mesh geometry).
    // Bones are stored in topological order (parent index always < child index).
    //
    // Method B: skeleton.json world_matrix is already in D3D11 Y-up row-major space.
    // Animation uses P * TRS_row(Y↔Z swapped) * P directly — no C_mat chain needed.
    struct BoneNode {
        std::string name;
        int         parentIdx   = -1;    // index in m_skeleton; -1 = root
        int         partIdx     = -1;    // index in m_parts (-1 if no mesh for this bone)
        float       bindWorld[16] = {};  // D3D11 row-major bind-pose world matrix
        float       bindLocal[16] = {};  // D3D11 row-major bind-pose local matrix
                                         // (= bindWorld for roots;
                                         //  = bindWorld * inv(parent.bindWorld) otherwise)

        // Position scale factor: 1.0f / scale_div.
        //   Trans / Top / Rot = 1.0f  (game units, no conversion)
        //   All other bones   = 0.01f (cm → m)
        float   scaleDivInv = 0.01f;
    };

private:
    bool InitPipeline(ID3D11Device* dev);

    std::vector<std::unique_ptr<MeshPart>> m_parts;
    std::vector<BoneNode>                  m_skeleton;  // all bones, topological order

    bool m_loaded     = false;
    bool m_texLoaded  = false;  // true = real Diffuse.png loaded; false = white fallback

    // World matrices from the last Draw() call — flat float[16] per bone, row-major.
    // Translation is at [12..14] (row 3 of each matrix).
    std::vector<std::array<float, 16>> m_lastAnimWorld;

    // Mesh-specific D3D pipeline
    ID3D11VertexShader*       m_vs          = nullptr;
    ID3D11PixelShader*        m_ps          = nullptr;  // diffuse + alpha clip
    ID3D11InputLayout*        m_layout      = nullptr;  // POSITION+COLOR+TEXCOORD (32b)
    ID3D11SamplerState*       m_sampler     = nullptr;
    ID3D11ShaderResourceView* m_diffuseSRV  = nullptr;  // shared Diffuse.png (or 1x1 white)
};
