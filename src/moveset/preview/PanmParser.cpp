#include "PanmParser.h"
#include <cstring>
#include <cmath>
#include <fstream>
#include <algorithm>

// ----------------------------------------------------------------
//  Low-level readers
// ----------------------------------------------------------------

static inline uint16_t RdU16(const uint8_t* d, size_t o)
{ uint16_t v; memcpy(&v, d+o, 2); return v; }

static inline uint32_t RdU32(const uint8_t* d, size_t o)
{ uint32_t v; memcpy(&v, d+o, 4); return v; }

static inline float RdF32(const uint8_t* d, size_t o)
{ float v; memcpy(&v, d+o, 4); return v; }

// ----------------------------------------------------------------
//  BitReader  (LSB first, used by KEF)
// ----------------------------------------------------------------

struct BitReader {
    const uint8_t* data;
    size_t         dataSize;
    size_t         bitPos = 0;

    uint32_t Read(int n) {
        uint32_t value = 0;
        for (int i = 0; i < n; ++i) {
            size_t by = bitPos / 8;
            int    bi = (int)(bitPos % 8);
            if (by < dataSize)
                value |= (uint32_t)((data[by] >> bi) & 1) << i;
            ++bitPos;
        }
        return value;
    }
};

// ----------------------------------------------------------------
//  Read one 44-byte sample (shared layout for FBF / Static / KEF base)
// ----------------------------------------------------------------

static BoneSample ReadSample(const uint8_t* d, size_t off)
{
    BoneSample s;
    s.scale[0]    = RdF32(d, off +  0);  // Scale X
    s.scale[1]    = RdF32(d, off +  4);  // Scale Y
    s.scale[2]    = RdF32(d, off +  8);  // Scale Z
    s.rotation[0] = RdF32(d, off + 12);  // Quat X
    s.rotation[1] = RdF32(d, off + 16);  // Quat Y
    s.rotation[2] = RdF32(d, off + 20);  // Quat Z
    s.rotation[3] = RdF32(d, off + 24);  // Quat W
    s.position[0] = RdF32(d, off + 28);  // Pos X  (cm)
    s.position[1] = RdF32(d, off + 32);  // Pos Y  (cm)
    s.position[2] = RdF32(d, off + 36);  // Pos Z  (cm)
    // offset+40 = padding, ignored
    return s;
}

// ----------------------------------------------------------------
//  FBF  (Frame-By-Frame, anim_flag == 1)
// ----------------------------------------------------------------

static void ParseFBF(const uint8_t* d, size_t sz,
                     size_t c_start, uint32_t bone_frames,
                     BoneTrack& track)
{
    track.frames.reserve(bone_frames);
    for (uint32_t f = 0; f < bone_frames; ++f) {
        size_t off = c_start + (size_t)f * 44;
        if (off + 44 > sz) break;
        track.frames.push_back(ReadSample(d, off));
    }
}

// ----------------------------------------------------------------
//  Static  (anim_flag == 2)
// ----------------------------------------------------------------

static void ParseStatic(const uint8_t* d, size_t sz,
                        size_t c_start, uint32_t bone_frames,
                        BoneTrack& track)
{
    if (c_start + 44 > sz) return;
    track.frames.assign(bone_frames, ReadSample(d, c_start));
}

// ----------------------------------------------------------------
//  KEF  (Key-Framed Encoding, anim_flag == 3)
// ----------------------------------------------------------------

static void ParseKEF(const uint8_t* d, size_t sz,
                     size_t c_start, uint32_t bone_frames,
                     const std::string& bone_name,
                     BoneTrack& track)
{
    if (c_start + 16 > sz) return;

    // Section 1: Header (16 bytes at c_start)
    uint16_t magic          = RdU16(d, c_start + 0x00);  // expect 0x0004
    uint16_t mode           = RdU16(d, c_start + 0x02);  // 0x0006=normal, 0x000A=root motion
    uint16_t bits_per_frame = RdU16(d, c_start + 0x06);
    uint32_t bs_off_rel     = RdU32(d, c_start + 0x08);  // bitstream offset from c_start

    if (magic != 0x0004) return;
    if (bs_off_rel < 44) return;

    size_t base_frame_off = (size_t)bs_off_rel - 44;   // 44 bytes before bitstream
    int    num_tracks     = (base_frame_off > 0x10)
                          ? (int)((base_frame_off - 0x10) / 16)
                          : 0;
    if (num_tracks <= 0 || num_tracks > 11) return;

    // Section 2: Channel table (num_tracks * 16 bytes at c_start + 0x10)
    struct Channel { float min, max; uint32_t bits; };
    Channel channels[11] = {};
    for (int t = 0; t < num_tracks; ++t) {
        size_t off = c_start + 0x10 + (size_t)t * 16;
        if (off + 12 > sz) return;
        channels[t].min  = RdF32(d, off + 0);
        channels[t].max  = RdF32(d, off + 4);
        channels[t].bits = RdU32(d, off + 8);
        // Inactive channel: exact equality (NOT approximate — see format doc warning)
        if (channels[t].max == channels[t].min || channels[t].bits > 32)
            channels[t].bits = 0;
    }

    // Section 3: Base frame (44 bytes just before bitstream)
    size_t bf_abs = c_start + base_frame_off;
    if (bf_abs + 44 > sz) return;
    float base[11];
    for (int i = 0; i < 11; ++i)
        base[i] = RdF32(d, bf_abs + (size_t)i * 4);

    // mode == 0x000A: root motion bone (Top, Trans, …).
    // Channel table has 9 entries; the qw slot (idx 6) is absent and position
    // channels shift down by 1: idx 6→posX, 7→posY, 8→posZ (comp[7..9]).
    const bool is_root_motion = (mode == 0x000A);

    // has_qw: root motion bones never encode qw — always derive from qx/qy/qz.
    bool     has_qw = !is_root_motion && (num_tracks > 6 && channels[6].bits > 0);
    uint32_t sum_active_bits = 0;
    for (int t = 0; t < num_tracks; ++t)
        sum_active_bits += channels[t].bits;
    bool sign_bit_per_frame = (bits_per_frame == sum_active_bits + 1);

    // Section 4: Bitstream
    size_t bs_abs  = c_start + (size_t)bs_off_rel;
    size_t bs_size = (bs_abs < sz) ? (sz - bs_abs) : 0;
    BitReader reader{ d + bs_abs, bs_size, 0 };

    track.frames.reserve(bone_frames);
    for (uint32_t f = 0; f < bone_frames; ++f) {
        // Start from base frame each iteration
        float comp[11];
        memcpy(comp, base, sizeof(comp));

        // Read active channels from bitstream (frame-interleaved).
        // Root motion bone (mode=0x000A): channel table idx 6+ maps to comp[idx+1]
        // because the qw slot is absent and position channels are shifted up by 1.
        for (int t = 0; t < num_tracks; ++t) {
            if (channels[t].bits > 0) {
                uint32_t raw     = reader.Read((int)channels[t].bits);
                uint32_t max_val = (1u << channels[t].bits) - 1u;
                float decoded = channels[t].min
                              + ((float)raw / (float)max_val)
                              * (channels[t].max - channels[t].min);
                if (is_root_motion && t >= 6)
                    comp[t + 1] = decoded;   // posX/Y/Z: idx 6→comp[7], 7→comp[8], 8→comp[9]
                else
                    comp[t] = decoded;
            }
        }

        // Reconstruct Quat W
        float qx = comp[3], qy = comp[4], qz = comp[5], qw;
        if (has_qw) {
            qw = comp[6];
        } else {
            float dot  = qx*qx + qy*qy + qz*qz;
            float sign;
            if (sign_bit_per_frame)
                sign = reader.Read(1) ? -1.f : 1.f;
            else
                sign = (base[6] >= 0.f) ? 1.f : -1.f;
            float inner = 1.f - dot;
            qw = sign * sqrtf(inner > 0.f ? inner : 0.f);
        }

        BoneSample s;
        s.scale[0]    = comp[0];
        s.scale[1]    = comp[1];
        s.scale[2]    = comp[2];
        s.rotation[0] = qx;
        s.rotation[1] = qy;
        s.rotation[2] = qz;
        s.rotation[3] = qw;
        s.position[0] = comp[7];
        s.position[1] = comp[8];
        s.position[2] = comp[9];
        track.frames.push_back(s);
    }
}

// ----------------------------------------------------------------
//  Top-level parser
// ----------------------------------------------------------------

bool ParsePanm(const std::vector<uint8_t>& data,
               ParsedAnim&                 out,
               std::string&                errorMsg)
{
    const uint8_t* d  = data.data();
    const size_t   sz = data.size();

    if (sz < 0xA0) { errorMsg = "File too small"; return false; }

    // PANM identifier sits at bytes [4..7]
    if (memcmp(d + 4, "PANM", 4) != 0) {
        errorMsg = "Not a PANM file (missing identifier at 0x04)";
        return false;
    }

    out.totalFrames = RdU32(d, 0x40);
    uint32_t first_c_ptr = RdU32(d, 0x50);
    uint32_t bone_count  = RdU32(d, 0x94);

    if (bone_count == 0 || bone_count > 512) {
        errorMsg = "Implausible bone count";
        return false;
    }

    out.bones.clear();
    out.nameToIdx.clear();

    for (uint32_t i = 0; i < bone_count; ++i) {
        // --- Bone pointer array ---
        size_t entry_addr = 0x98 + (size_t)i * 4;
        if (entry_addr + 4 > sz) break;
        uint32_t rel_a = RdU32(d, entry_addr);
        size_t   a_off = entry_addr + rel_a;
        if (a_off + 0x20 > sz) continue;

        // --- A-Block: B-Block relative offset ---
        // entry_b_addr = a_off + 0x10 (where the relative offset to B-block is stored)
        // block_b_offset = entry_b_addr + rel_b - 8  (FlatBuffers self-relative pointer: -8 skips vtable header)
        size_t   entry_b_addr = a_off + 0x10;
        if (entry_b_addr + 4 > sz) continue;
        uint32_t rel_b  = RdU32(d, entry_b_addr);
        size_t   b_off  = entry_b_addr + (size_t)rel_b - 8;   // = block_b_offset (Blender addon formula)

        // --- A-Block: bone name ---
        if (a_off + 0x18 + 1 > sz) continue;
        uint32_t name_len = RdU32(d, a_off + 0x14);
        if (name_len == 0 || a_off + 0x18 + name_len > sz) continue;
        std::string bone_name(reinterpret_cast<const char*>(d + a_off + 0x18), name_len);
        while (!bone_name.empty() && bone_name.back() == '\0')
            bone_name.pop_back();

        // --- B-Block fields (offsets from block_b_offset, per Blender addon) ---
        //   +0x0C  indicator   (0x20 = use first_c_ptr; 0x24 = first_c_ptr + c_rel)
        //   +0x14  anim_flag   (1=FBF, 2=Static, 3=KEF)
        //   +0x18  bone_frames
        //   +0x20  c_rel       (only when indicator == 0x24)
        if (b_off + 0x20 > sz) continue;
        uint32_t indicator   = RdU32(d, b_off + 0x0C);
        uint32_t anim_flag   = RdU32(d, b_off + 0x14);
        uint32_t bone_frames = RdU32(d, b_off + 0x18);
        if (bone_frames == 0) bone_frames = 1;

        // --- C-Block absolute offset ---
        size_t c_start = 0;
        bool   valid   = false;
        if (indicator == 0x20) {
            c_start = first_c_ptr;
            valid   = true;
        } else if (indicator == 0x24) {
            uint32_t c_rel = RdU32(d, b_off + 0x20);
            c_start = (size_t)first_c_ptr + c_rel;
            valid   = true;
        }
        // indicator 0x34 / 0x38: root motion / auxiliary bones
        // C-Block location unknown — skip for now
        if (!valid) continue;

        // --- Decode ---
        BoneTrack track;
        track.name = bone_name;

        switch (anim_flag) {
        case 1: ParseFBF   (d, sz, c_start, bone_frames, track);            break;
        case 2: ParseStatic(d, sz, c_start, bone_frames, track);            break;
        case 3: ParseKEF   (d, sz, c_start, bone_frames, bone_name, track); break;
        default: continue;
        }

        if (track.frames.empty()) continue;

        out.nameToIdx[bone_name] = (int)out.bones.size();
        out.bones.push_back(std::move(track));
    }

    if (out.bones.empty()) {
        errorMsg = "No valid bones found";
        return false;
    }
    return true;
}

bool ParsePanmFile(const std::string& path,
                   ParsedAnim&        out,
                   std::string&       errorMsg)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { errorMsg = "Cannot open: " + path; return false; }
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>());
    return ParsePanm(data, out, errorMsg);
}

// ----------------------------------------------------------------
//  ParsedAnim helpers
// ----------------------------------------------------------------

static BoneSample IdentitySample()
{
    BoneSample s{};
    s.scale[0] = s.scale[1] = s.scale[2] = 1.f;
    s.rotation[3] = 1.f;  // qw = 1 (identity quaternion)
    return s;
}

BoneSample ParsedAnim::SampleAt(const std::string& name, uint32_t frame) const
{
    auto it = nameToIdx.find(name);
    if (it == nameToIdx.end()) return IdentitySample();
    const BoneTrack& t = bones[it->second];
    if (t.frames.empty()) return IdentitySample();
    uint32_t idx = frame < (uint32_t)t.frames.size()
                 ? frame
                 : (uint32_t)t.frames.size() - 1;
    return t.frames[idx];
}
