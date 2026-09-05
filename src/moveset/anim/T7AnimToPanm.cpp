#include "T7AnimToPanm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// Small utilities
// ─────────────────────────────────────────────────────────────────────────────

using Vec3 = std::array<double, 3>;
using Quat = std::array<double, 4>;   // XYZW

struct BoneFrame {
    Vec3 scale;
    Quat quat;
    Vec3 pos;
};

using BoneTrack  = std::vector<BoneFrame>;
using BoneTracks = std::unordered_map<std::string, BoneTrack>;

inline uint16_t RdU16(const uint8_t* d, size_t o) {
    uint16_t v; std::memcpy(&v, d + o, 2); return v;
}
inline uint32_t RdU32(const uint8_t* d, size_t o) {
    uint32_t v; std::memcpy(&v, d + o, 4); return v;
}
inline int16_t RdI16(const uint8_t* d, size_t o) {
    int16_t v; std::memcpy(&v, d + o, 2); return v;
}
inline double RdF32(const uint8_t* d, size_t o) {
    float v; std::memcpy(&v, d + o, 4); return static_cast<double>(v);
}

inline void WrU32(std::vector<uint8_t>& d, size_t o, uint32_t v) {
    std::memcpy(d.data() + o, &v, 4);
}
inline void PushU16(std::vector<uint8_t>& d, uint16_t v) {
    d.insert(d.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 2);
}
inline void PushU32(std::vector<uint8_t>& d, uint32_t v) {
    d.insert(d.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4);
}
inline void PushF32(std::vector<uint8_t>& d, double v) {
    float f = static_cast<float>(v);
    d.insert(d.end(), reinterpret_cast<uint8_t*>(&f), reinterpret_cast<uint8_t*>(&f) + 4);
}

// ctypes.c_int16 semantics: truncate to the low 16 bits, reinterpret as signed.
inline int16_t I16(int64_t v) {
    return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint64_t>(v) & 0xFFFFu));
}

// Python's round(): half-to-even, implemented exactly as CPython's float.__round__.
inline double RoundHalfEven(double x) {
    double r = std::round(x);
    if (std::fabs(x - r) == 0.5) r = 2.0 * std::round(x / 2.0);
    return r;
}

// Python floor division for int64.
inline int64_t FloorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

bool B64Decode(const char* s, std::vector<uint8_t>& out) {
    static int8_t tbl[256];
    static bool init = false;
    if (!init) {
        std::memset(tbl, -1, sizeof(tbl));
        const char* al = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) tbl[static_cast<uint8_t>(al[i])] = static_cast<int8_t>(i);
        init = true;
    }
    out.clear();
    uint32_t buf = 0; int bits = 0;
    for (const char* p = s; *p; ++p) {
        uint8_t c = static_cast<uint8_t>(*p);
        if (c == '=' ) break;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int8_t v = tbl[c];
        if (v < 0) return false;
        buf = (buf << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Quaternion math — formulas.py
// ─────────────────────────────────────────────────────────────────────────────

inline Quat QMul(const Quat& A, const Quat& B) {
    return { A[3]*B[0] + A[0]*B[3] + A[1]*B[2] - A[2]*B[1],
             A[3]*B[1] + A[1]*B[3] + A[2]*B[0] - A[0]*B[2],
             A[3]*B[2] + A[2]*B[3] + A[0]*B[1] - A[1]*B[0],
             A[3]*B[3] - A[0]*B[0] - A[1]*B[1] - A[2]*B[2] };
}

inline Quat QNorm(const Quat& q) {
    double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (n > 1e-10) return { q[0]/n, q[1]/n, q[2]/n, q[3]/n };
    return { 0.0, 0.0, 0.0, 1.0 };
}

inline Quat QInv(const Quat& q) { return { -q[0], -q[1], -q[2], q[3] }; }

// ZYX Euler (radians) → quaternion XYZW  (q = Rz·Ry·Rx)
inline Quat Etq(double ex, double ey, double ez) {
    double cx = std::cos(ex/2), sx = std::sin(ex/2);
    double cy = std::cos(ey/2), sy = std::sin(ey/2);
    double cz = std::cos(ez/2), sz = std::sin(ez/2);
    return QMul(QMul(Quat{0.0, 0.0, sz, cz}, Quat{0.0, sy, 0.0, cy}), Quat{sx, 0.0, 0.0, cx});
}
inline Quat Etq(const Vec3& e) { return Etq(e[0], e[1], e[2]); }

inline Quat Slerp(const Quat& qa, const Quat& qb_in, double t) {
    Quat qb = qb_in;
    double dot = qa[0]*qb[0] + qa[1]*qb[1] + qa[2]*qb[2] + qa[3]*qb[3];
    if (dot < 0) { qb = { -qb[0], -qb[1], -qb[2], -qb[3] }; dot = -dot; }
    dot = std::min(1.0, dot);
    if (dot > 0.9995) {
        return QNorm({ qa[0] + t*(qb[0]-qa[0]), qa[1] + t*(qb[1]-qa[1]),
                       qa[2] + t*(qb[2]-qa[2]), qa[3] + t*(qb[3]-qa[3]) });
    }
    double th0 = std::acos(dot);
    double th  = th0 * t;
    double s0  = std::cos(th) - dot * std::sin(th) / std::sin(th0);
    double s1  = std::sin(th) / std::sin(th0);
    return QNorm({ s0*qa[0] + s1*qb[0], s0*qa[1] + s1*qb[1],
                   s0*qa[2] + s1*qb[2], s0*qa[3] + s1*qb[3] });
}

// ─────────────────────────────────────────────────────────────────────────────
// TK7 parameter table (tk7_parser.py / tk7_kef_to_fbf.py)
// ─────────────────────────────────────────────────────────────────────────────

enum Tk7Param {
    P_GlobalPos = 0, P_LocalPos, P_DirY,
    P_FullBody, P_UpperBody, P_LowerBody, P_SpineFlex,
    P_Neck, P_Head,
    P_R_Shoulder, P_R_UpperArm, P_R_LowerArm, P_R_Hand,
    P_L_Shoulder, P_L_UpperArm, P_L_LowerArm, P_L_Hand,
    P_R_UpperLeg, P_R_LowerLeg, P_R_Foot,
    P_L_UpperLeg, P_L_LowerLeg, P_L_Foot,
    P_COUNT
};

using Tk7Frames = std::array<std::vector<Vec3>, P_COUNT>;

// Header type declarations of the standard 23 parameters (validation).
const uint32_t kStdTypes[P_COUNT] = {
    0xB, 0xB, 0x5, 0x7, 0x7, 0x7, 0xB, 0x7, 0x7, 0x7, 0x7, 0x6, 0x7,
    0x7, 0x7, 0x6, 0x7, 0x7, 0x6, 0x7, 0x7, 0x6, 0x7
};

// Standard 23-bone TK7 skeleton descriptor
// (11: positional, 6: Z-only, 5: Y-only, 7: full 3D)
const int kBoneDesc[P_COUNT] = {
    11, 11, 5, 7, 7, 7, 11, 7, 7, 7, 7, 6, 7, 7, 7, 6, 7, 7, 6, 7, 7, 6, 7
};

// ─────────────────────────────────────────────────────────────────────────────
// TK7 FBF parser — tk7_parser.py parse_tk7()
// ─────────────────────────────────────────────────────────────────────────────

bool ParseTk7Fbf(const uint8_t* data, size_t len, uint32_t& nFramesOut,
                 Tk7Frames& frames, std::string& err) {
    if (len < 8) { err = "TK7 FBF: file too small."; return false; }
    if (data[0] != 0xC8 || data[1] != 0x00) { err = "TK7 FBF: bad magic."; return false; }

    uint32_t nParams = RdU16(data, 0x02);
    if (nParams < P_COUNT) {
        err = "TK7 FBF: too few parameters (" + std::to_string(nParams) + ", need >= 23).";
        return false;
    }
    uint32_t nFrames = RdU32(data, 0x04);
    if (nFrames == 0) { err = "TK7 FBF: frame count is zero."; return false; }

    if (len < 0x08 + static_cast<size_t>(nParams) * 4) {
        err = "TK7 FBF: truncated parameter type table."; return false;
    }
    for (int i = 0; i < P_COUNT; ++i) {
        if (RdU32(data, 0x08 + static_cast<size_t>(i) * 4) != kStdTypes[i]) {
            err = "TK7 FBF: not the standard 23-parameter layout.";
            return false;
        }
    }

    size_t dataStart = 0x08 + static_cast<size_t>(nParams) * 4;
    size_t stride    = static_cast<size_t>(nParams) * 12;
    size_t minSize   = dataStart + static_cast<size_t>(nFrames) * stride;
    if (len < minSize) {
        err = "TK7 FBF: file too short (expected " + std::to_string(minSize) +
              " bytes, got " + std::to_string(len) + ").";
        return false;
    }

    for (int i = 0; i < P_COUNT; ++i) frames[i].resize(nFrames);
    for (uint32_t fr = 0; fr < nFrames; ++fr) {
        size_t base = dataStart + static_cast<size_t>(fr) * stride;
        for (int i = 0; i < P_COUNT; ++i) {
            size_t off = base + static_cast<size_t>(i) * 12;
            frames[i][fr] = { RdF32(data, off), RdF32(data, off + 4), RdF32(data, off + 8) };
        }
    }
    nFramesOut = nFrames;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// VLC KEF decoder — tk7_kef_to_fbf.py
// ─────────────────────────────────────────────────────────────────────────────

// int16 → radians. Exact binary32 constant from TK7 RVA 0x278f6b0 (bits 0x38c90fdb) = pi/32768.
const double MULT_CONST = M_PI / 32768.0;
const int64_t SENTINEL  = 0xF0000000LL;

inline bool IsBoneShort(int d) { return d >= 4 && d <= 7; }

// Bit reader — mirrors the game's reader (_BR in tk7_kef_to_fbf.py).
struct BitReader {
    const uint8_t* data = nullptr;
    size_t         size = 0;      // padded size
    size_t         ptr  = 0;
    uint64_t       rem  = 0;
    int            avail = 0;
    bool*          overrun = nullptr;

    BitReader(const uint8_t* d, size_t sz, size_t start, uint32_t init2, bool* ov)
        : data(d), size(sz), ptr(start), rem(static_cast<uint64_t>(init2 & 3u)),
          avail(2), overrun(ov) {}

    uint8_t Byte(size_t i) {
        if (i >= size) { *overrun = true; return 0; }
        return data[i];
    }

    int64_t Read(int n) {
        if (n == 0) return 0;
        if (avail < n) {
            int nc = ((n - avail - 1) >> 3) + 1;
            for (int i = 0; i < nc; ++i) {
                rem = static_cast<uint64_t>(Byte(ptr)) | (rem << 8);
                ++ptr;
            }
            avail += 8 * nc;
        }
        int v7 = avail - n;
        avail  = v7;
        int64_t r = static_cast<int64_t>(rem >> v7);
        rem &= ~(0xFFFFFFFFull << v7);
        if (n == 16 && (r & 0x8000)) r -= 0x10000;
        return r;
    }
};

// VLC prefix table: (nbits, base, next_x1)
struct VlcEntry { int nb; int base; int nx1; };
const VlcEntry kVlc[14] = {
    {0, 0, 0},      {2, 1, 2},      {2, -4, 2},      {4, 5, 4},
    {4, -20, 4},    {6, 21, 6},     {6, -84, 6},     {8, 85, 8},
    {8, -340, 8},   {10, 341, 10},  {10, -1364, 10}, {12, 1365, 12},
    {12, -5460, 12},{16, 0, 16}
};

struct VlcState { int64_t a2a = 0; int64_t x1 = 0; int64_t x2 = 0; };

int64_t VlcDecode(BitReader& br, VlcState& st) {
    if (st.a2a > 0) {
        int64_t na = st.a2a - 1;
        st.a2a = na;
        if (st.x1 == 0)  return 0;
        if (st.x1 == 99) return SENTINEL;
        return st.x2 + br.Read(static_cast<int>(st.x1));
    }
    int64_t pfx = br.Read(4);
    if (pfx < 14) {
        const VlcEntry& e = kVlc[pfx];
        if (e.nb == 0) { st.a2a = 0; st.x1 = 0; st.x2 = 0; return 0; }
        int64_t v = br.Read(e.nb) + e.base;
        st.a2a = 0; st.x1 = e.nx1; st.x2 = e.base;
        return v;
    }
    if (pfx == 14) {
        int64_t na = br.Read(4) - 1;
        st.a2a = na;
        if (st.x1 == 0)  return 0;
        if (st.x1 == 99) return SENTINEL;
        return st.x2 + br.Read(static_cast<int>(st.x1));
    }
    st.a2a = 0; st.x1 = 99; st.x2 = 0;
    return SENTINEL;
}

// parse_vlc_kef(): returns per-frame, per-bone [x,y,z] raw triples.
bool ParseVlcKef(const uint8_t* src, size_t srcLen, uint32_t& nFramesOut,
                 std::vector<std::vector<Vec3>>& framesOut, size_t& boneCountOut,
                 std::string& err) {
    // The game's bit reader can read 1-2 bytes past the end of the last stream;
    // it has slack memory after the animation buffer. Mimic with zero padding.
    std::vector<uint8_t> kef(src, src + srcLen);
    kef.resize(srcLen + 64, 0);
    const uint8_t* K = kef.data();
    const size_t   KS = kef.size();

    auto need = [&](size_t off, size_t n) { return off + n <= srcLen; };

    if (!need(0, 4)) { err = "KEF: header truncated."; return false; }
    size_t boneCount = RdU16(K, 0x02);
    if (boneCount == 0) { err = "KEF: bone count is zero."; return false; }
    if (!need(4, boneCount * 2)) { err = "KEF: bone descriptor table truncated."; return false; }

    std::vector<int> boneDesc(boneCount);
    for (size_t i = 0; i < boneCount; ++i) boneDesc[i] = RdU16(K, 4 + i * 2);

    size_t postBd = 4 + boneCount * 2;
    if (!need(postBd, 6)) { err = "KEF: header truncated at frame block."; return false; }

    uint32_t NF      = RdU16(K, postBd);
    uint32_t vv17    = K[postBd + 2];
    uint32_t fvShift = K[postBd + 3];
    size_t   floatN  = RdU16(K, postBd + 4);
    if (NF == 0) { err = "KEF: frame count is zero."; return false; }

    if (!need(postBd + 6, floatN * 4)) { err = "KEF: float offset table truncated."; return false; }
    std::vector<double> floatOffs(floatN);
    for (size_t i = 0; i < floatN; ++i) floatOffs[i] = RdF32(K, postBd + 6 + i * 4);

    uint32_t svShift = vv17 & 0x7F;
    if (svShift > 31 || fvShift > 31) { err = "KEF: invalid shift amount."; return false; }

    size_t bpRel  = 2 * ((4 * floatN + 6) / 2);
    size_t ptr    = postBd + bpRel;

    std::vector<Vec3>                 baseFloat(boneCount);
    std::vector<std::array<int16_t,3>> v81Init(boneCount);
    for (size_t bi = 0; bi < boneCount; ++bi) {
        if (IsBoneShort(boneDesc[bi])) {
            if (!need(ptr, 6)) { err = "KEF: base pose truncated."; return false; }
            for (int ax = 0; ax < 3; ++ax) {
                int16_t r = RdI16(K, ptr + ax * 2);
                baseFloat[bi][ax] = static_cast<double>(r) * MULT_CONST;
                v81Init[bi][ax]   = I16(static_cast<int32_t>(r) >> svShift);
            }
            ptr += 6;
        } else {
            if (!need(ptr, 12)) { err = "KEF: base pose truncated."; return false; }
            for (int ax = 0; ax < 3; ++ax) {
                baseFloat[bi][ax] = RdF32(K, ptr + ax * 4);
                v81Init[bi][ax]   = 0;
            }
            ptr += 12;
        }
    }

    size_t animPtr = ptr;
    // Frames needing a delta block are 2..NF (frame 1 = base pose).
    // Max block index = (NF-2)>>4 → block count = ceil((NF-1)/16).
    size_t kfCount = (static_cast<size_t>(NF) + 14) / 16;
    if (!need(animPtr, kfCount * 4)) { err = "KEF: keyframe offset table truncated."; return false; }
    std::vector<uint32_t> kfOffs(kfCount);
    for (size_t k = 0; k < kfCount; ++k) kfOffs[k] = RdU32(K, animPtr + k * 4);

    // Count of non-short (float) bones must be covered by the float offset table.
    {
        size_t fcnt = 0;
        for (size_t bi = 0; bi < boneCount; ++bi) if (!IsBoneShort(boneDesc[bi])) ++fcnt;
        if (fcnt > floatN) { err = "KEF: float offset table too small."; return false; }
    }

    bool overrun = false;

    // Decode one frame's int16-wrapped v81 state (identical to the game:
    // Animations_T_x64.cpp ParseAnimation0x64).
    auto decodeV81 = [&](uint32_t frame, std::vector<std::array<int16_t,3>>& v81) {
        v81 = v81Init;
        uint32_t bf = frame - 1;
        if (bf == 0) return;
        uint32_t subKf = bf & 0xF;
        size_t   kfIdx = bf >> 4;
        if (kfIdx >= kfCount) { kfIdx = kfCount - 1; subKf = 0xF; }   // tail-frame clamp
        size_t   cvp   = animPtr + kfOffs[kfIdx];
        VlcState st;
        for (size_t bi = 0; bi < boneCount; ++bi) {
            for (int ax = 0; ax < 3; ++ax) {
                if (cvp >= KS) { overrun = true; return; }
                uint32_t entry = K[cvp];
                uint32_t nvo   = entry >> 2;
                uint32_t ibits = entry & 3;
                BitReader br(K, KS, cvp + 1, ibits, &overrun);
                st.a2a = 0;
                int64_t delta = VlcDecode(br, st);
                if (delta != SENTINEL) v81[bi][ax] = I16(static_cast<int64_t>(v81[bi][ax]) + delta);
                if (subKf > 0) {
                    int64_t vel = 0;
                    for (uint32_t s = 0; s < subKf; ++s) {
                        int64_t d = VlcDecode(br, st);
                        if (d == SENTINEL) break;
                        vel += d;
                        v81[bi][ax] = I16(static_cast<int64_t>(v81[bi][ax]) + vel);
                    }
                }
                cvp += nvo;
            }
        }
    };

    // Decode every frame into int16-wrapped state (as the game does).
    std::vector<std::vector<std::array<int64_t,3>>> allV81(NF);
    {
        std::vector<std::array<int16_t,3>> tmp;
        for (uint32_t fr = 1; fr <= NF; ++fr) {
            decodeV81(fr, tmp);
            allV81[fr - 1].resize(boneCount);
            for (size_t bi = 0; bi < boneCount; ++bi)
                for (int ax = 0; ax < 3; ++ax)
                    allV81[fr - 1][bi][ax] = tmp[bi][ax];
        }
    }
    if (overrun) { err = "KEF: VLC bitstream ran past the end of the buffer."; return false; }

    // Phase-unwrap the positional (float) channels: express each frame's int16
    // value as the representation (±65536*k) closest to the previous frame, so
    // real displacements beyond int16 (walking backwards, crouching) stay
    // continuous while garbage-wrapped values stay wrapped.
    for (size_t bi = 0; bi < boneCount; ++bi) {
        if (IsBoneShort(boneDesc[bi])) continue;   // rotations are modular — no unwrap
        for (int ax = 0; ax < 3; ++ax) {
            int64_t prev = allV81[0][bi][ax];
            for (uint32_t f = 1; f < NF; ++f) {
                int64_t w = allV81[f][bi][ax];
                int64_t unwrapped = w + FloorDiv(prev - w + 32768, 65536) * 65536;
                allV81[f][bi][ax] = unwrapped;
                prev = unwrapped;
            }
        }
    }

    framesOut.assign(NF, std::vector<Vec3>(boneCount));
    for (uint32_t f = 0; f < NF; ++f) {
        size_t fbi = 0;
        for (size_t bi = 0; bi < boneCount; ++bi) {
            bool isS = IsBoneShort(boneDesc[bi]);
            for (int ax = 0; ax < 3; ++ax) {
                int64_t v = allV81[f][bi][ax];
                if (isS) {
                    uint32_t t = static_cast<uint32_t>(static_cast<int32_t>(v)) << svShift;
                    framesOut[f][bi][ax] =
                        static_cast<double>(static_cast<int16_t>(static_cast<uint16_t>(t))) * MULT_CONST;
                } else {
                    // v may exceed int16 after unwrapping; C++ `(float)(value << shift)`
                    // uses 32-bit shifts with no wrapping in the original, so widen.
                    framesOut[f][bi][ax] =
                        static_cast<double>(v << fvShift) * floatOffs[fbi] + baseFloat[bi][ax];
                }
            }
            if (!isS) ++fbi;
        }
    }

    nFramesOut   = NF;
    boneCountOut = boneCount;
    return true;
}

// KEF rotation triple (a, b, c) → unit quaternion (XYZW).
//   axis = (cos(a)·sin(b), -sin(a), cos(a)·cos(b))
//   q    = (axis · sin(c/2), cos(c/2))
inline Quat AxisAngleToQuat(double a, double b, double c) {
    double ca = std::cos(a), sa = std::sin(a);
    double s = std::sin(c / 2.0), w = std::cos(c / 2.0);
    return { ca * std::sin(b) * s, -sa * s, ca * std::cos(b) * s, w };
}

// tk7_kef_to_fbf._qnorm (1e-12 threshold, distinct from formulas.qnorm).
inline Quat QNormK(const Quat& q) {
    double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (n > 1e-12) return { q[0]/n, q[1]/n, q[2]/n, q[3]/n };
    return { 0.0, 0.0, 0.0, 1.0 };
}

// Quaternion XYZW → ZYX Euler (rx, ry, rz)
inline Vec3 QteK(const Quat& qin) {
    Quat q = QNormK(qin);
    double x = q[0], y = q[1], z = q[2], w = q[3];
    double s = 2 * (w*y - z*x);
    s = std::max(-1.0, std::min(1.0, s));
    return { std::atan2(2*(w*x + y*z), 1 - 2*(x*x + y*y)),
             std::asin(s),
             std::atan2(2*(w*z + x*y), 1 - 2*(y*y + z*z)) };
}

// _apply_container(): KEF triple → FBF Euler triple.
inline Vec3 ApplyContainer(const Vec3& kefVals, size_t bi) {
    int desc = (bi < P_COUNT) ? kBoneDesc[bi] : 7;
    if (desc == 11) return kefVals;   // positional (mm) — pass through
    // Every rotation track (including DirY, desc=5) goes through the same
    // spherical axis-angle sampler; the engine conjugates the quaternion
    // before writing the local matrix, so FBF euler = qte(conj(q)).
    Quat q = AxisAngleToQuat(kefVals[0], kefVals[1], kefVals[2]);
    return QteK({ -q[0], -q[1], -q[2], q[3] });
}

// ─────────────────────────────────────────────────────────────────────────────
// formulas.py — TK7 → TK8 bone conversion
// ─────────────────────────────────────────────────────────────────────────────

const double SPINE1_RATIO = 0.45;

inline Quat CHead()  { return Etq(M_PI/2, 0.0, M_PI/2); }
inline Quat CArm()   { return Etq(-M_PI/2, 0.0, 0.0); }
inline Quat CHand()  { return Etq(M_PI, 0.0, 0.0); }
inline Quat CLeg()   { return Etq(0.0, 0.0, M_PI); }
inline Quat HipQ()   { return { 0.5, 0.5, 0.5, 0.5 }; }
inline Quat BLower() { return Etq(-M_PI/2, 0.0, -M_PI/2); }

const Quat kIdentity  = { 0.0, 0.0, 0.0, 1.0 };
const Vec3 kZeroPos   = { 0.0, 0.0, 0.0 };
const Vec3 kUnitScale = { 1.0, 1.0, 1.0 };

// _temporal_fix() computes the dot product with Python's builtin sum(), which
// since CPython 3.12 uses Neumaier compensated summation. The distinction only
// shows up when the dot product cancels to near zero (consecutive quaternions
// ~90 degrees apart, where either hemisphere is equally valid), but reproducing
// it keeps output byte-identical to the reference converter.
inline double NeumaierDot(const Quat& a, const Quat& b) {
    double f = 0.0 + a[0]*b[0];
    double c = 0.0;
    for (int i = 1; i < 4; ++i) {
        double x = a[i]*b[i];
        double t = f + x;
        if (std::fabs(f) >= std::fabs(x)) c += (f - t) + x;
        else                              c += (x - t) + f;
        f = t;
    }
    return f + c;
}

// If dot(q_cur, q_prev) < 0, negate q_cur (temporal quaternion consistency).
void TemporalFix(BoneTrack& track) {
    for (size_t fr = 1; fr < track.size(); ++fr) {
        const Quat& p = track[fr - 1].quat;
        Quat& q = track[fr].quat;
        if (NeumaierDot(q, p) < 0) q = { -q[0], -q[1], -q[2], -q[3] };
    }
}

// detect_native_convention(): true = native TK7/TTT2 (LowerBody carries body
// orientation); false = TK8→TK7 converted (LowerBody pinned at [-pi/2, 0, -pi/2]).
bool DetectNativeConvention(const Tk7Frames& f) {
    const double halfPi = M_PI / 2;
    for (const Vec3& v : f[P_LowerBody]) {
        if (!(std::fabs(v[0] + halfPi) < 1e-3 && std::fabs(v[1]) < 1e-3 &&
              std::fabs(v[2] + halfPi) < 1e-3))
            return true;
    }
    return false;
}

BoneTracks ConvertTk7ToTk8Bones(const Tk7Frames& f, bool isTtt2, bool facingToHara) {
    const size_t n = f[P_FullBody].size();
    BoneTracks out;

    auto makeTrack = [&](const char* name) -> BoneTrack& {
        BoneTrack& t = out[name];
        t.resize(n);
        return t;
    };

    // ── Top: GlobalPos (mm → cm) ────────────────────────────────────────────
    {
        BoneTrack& t = makeTrack("Top");
        for (size_t fr = 0; fr < n; ++fr)
            t[fr] = { kUnitScale, kIdentity,
                      { f[P_GlobalPos][fr][0]/10.0, f[P_GlobalPos][fr][1]/10.0,
                        f[P_GlobalPos][fr][2]/10.0 } };
    }
    // ── Trans: LocalPos (mm → cm) ───────────────────────────────────────────
    {
        BoneTrack& t = makeTrack("Trans");
        for (size_t fr = 0; fr < n; ++fr)
            t[fr] = { kUnitScale, kIdentity,
                      { f[P_LocalPos][fr][0]/10.0, f[P_LocalPos][fr][1]/10.0,
                        f[P_LocalPos][fr][2]/10.0 } };
    }

    // ── Rot: facing × body orientation ──────────────────────────────────────
    // Ground truth (cmnyg_co_turnr vs ygco_turnr): Rot ~= FullBody × orient;
    // DirY lives on HARA_ROT1 and must NOT be multiplied into Rot. DirY-only
    // animations (FullBody ~= 0) fall back to DirY.
    bool fbActive = false;
    for (size_t fr = 0; fr < n && !fbActive; ++fr)
        if (std::fabs(Etq(f[P_FullBody][fr])[3]) < 0.9962) fbActive = true;   // > 10 degrees
    const int facingParam = fbActive ? P_FullBody : P_DirY;

    std::vector<Quat> orientQs(n);
    {
        BoneTrack& t = makeTrack("Rot");
        const Quat invB = QInv(BLower());
        for (size_t fr = 0; fr < n; ++fr) {
            Quat facingQ = Etq(f[facingParam][fr]);
            // Native LowerBody is composed as orient × B_LOWER; strip the basis
            // or the whole lower body tilts by ~105 degrees.
            Quat orient = isTtt2 ? QNorm(QMul(Etq(f[P_LowerBody][fr]), invB)) : kIdentity;
            orientQs[fr] = orient;
            t[fr] = { kUnitScale, QNorm(QMul(facingQ, orient)), kZeroPos };
        }
    }

    // ── HARA_ROT1: in-game facing (optional) ────────────────────────────────
    if (facingToHara) {
        BoneTrack& t = makeTrack("HARA_ROT1");
        for (size_t fr = 0; fr < n; ++fr)
            t[fr] = { kUnitScale, QNorm(Etq(f[facingParam][fr])), kZeroPos };
    }

    // ── Spine1 + Spine2: UpperBody split (SPINE1_RATIO fitted on tk8_move2) ──
    {
        BoneTrack& t1 = makeTrack("Spine1");
        BoneTrack& t2 = makeTrack("Spine2");
        const Quat invHip = QInv(HipQ());
        for (size_t fr = 0; fr < n; ++fr) {
            Quat ub = Etq(f[P_UpperBody][fr]);
            Quat u = isTtt2 ? QNorm(QMul(invHip, QMul(QInv(orientQs[fr]), ub)))
                            : QNorm(QMul(invHip, ub));
            Quat s1 = Slerp(kIdentity, u, SPINE1_RATIO);
            Quat s2 = QNorm(QMul(QInv(s1), u));
            t1[fr] = { kUnitScale, s1, kZeroPos };
            t2[fr] = { kUnitScale, s2, kZeroPos };
        }
    }

    // ── Neck ────────────────────────────────────────────────────────────────
    {
        BoneTrack& t = makeTrack("Neck");
        for (size_t fr = 0; fr < n; ++fr)
            t[fr] = { kUnitScale, Etq(f[P_Neck][fr]), kZeroPos };
    }

    // ── Head: remap (a,b,c,d)=(qy,qz,qx,qw) → original = (c,a,b,d) ──────────
    {
        BoneTrack& t = makeTrack("Head");
        const Quat invC = QInv(CHead());
        for (size_t fr = 0; fr < n; ++fr) {
            Quat r = QMul(invC, Etq(f[P_Head][fr]));
            t[fr] = { kUnitScale, QNorm({ r[2], r[0], r[1], r[3] }), kZeroPos };
        }
    }

    // ── R/L_Shoulder: remap (a,b,c,d)=(qx,-qz,qy,qw) → original = (a,c,-b,d) ─
    {
        const Quat invC = QInv(CArm());
        auto invShoulder = [&](const Vec3& e) {
            Quat r = QMul(invC, Etq(e));
            return QNorm({ r[0], r[2], -r[1], r[3] });
        };
        BoneTrack& tr = makeTrack("R_Shoulder");
        BoneTrack& tl = makeTrack("L_Shoulder");
        for (size_t fr = 0; fr < n; ++fr) {
            tr[fr] = { kUnitScale, invShoulder(f[P_R_Shoulder][fr]), kZeroPos };
            tl[fr] = { kUnitScale, invShoulder(f[P_L_Shoulder][fr]), kZeroPos };
        }
    }

    // ── R/L_UpperArm: remap (a,b,c,d)=(qx,-qy,-qz,qw) → original = (a,-b,-c,d)
    {
        const Quat invC = QInv(CArm());
        auto invUpperArm = [&](const Vec3& e) {
            Quat r = QMul(invC, Etq(e));
            return QNorm({ r[0], -r[1], -r[2], r[3] });
        };
        BoneTrack& tr = makeTrack("R_UpperArm");
        BoneTrack& tl = makeTrack("L_UpperArm");
        for (size_t fr = 0; fr < n; ++fr) {
            tr[fr] = { kUnitScale, invUpperArm(f[P_R_UpperArm][fr]), kZeroPos };
            tl[fr] = { kUnitScale, invUpperArm(f[P_L_UpperArm][fr]), kZeroPos };
        }
    }

    // ── R/L_LowerArm: Z-only with W-negate inverted ─────────────────────────
    {
        auto invLowerArm = [](double rz) -> Quat {
            return { 0.0, 0.0, -std::sin(rz/2), std::cos(rz/2) };
        };
        BoneTrack& tr = makeTrack("R_LowerArm");
        BoneTrack& tl = makeTrack("L_LowerArm");
        for (size_t fr = 0; fr < n; ++fr) {
            tr[fr] = { kUnitScale, invLowerArm(f[P_R_LowerArm][fr][2]), kZeroPos };
            tl[fr] = { kUnitScale, invLowerArm(f[P_L_LowerArm][fr][2]), kZeroPos };
        }
    }

    // ── R_Hand: inv(C_hand) × q ─────────────────────────────────────────────
    {
        BoneTrack& t = makeTrack("R_Hand");
        const Quat invC = QInv(CHand());
        for (size_t fr = 0; fr < n; ++fr)
            t[fr] = { kUnitScale, QNorm(QMul(invC, Etq(f[P_R_Hand][fr]))), kZeroPos };
    }

    // ── L_Hand: remap (a,b,c,d)=(qx,-qy,-qz,qw) → original = (a,-b,-c,d) ────
    {
        BoneTrack& t = makeTrack("L_Hand");
        for (size_t fr = 0; fr < n; ++fr) {
            Quat r = Etq(f[P_L_Hand][fr]);
            t[fr] = { kUnitScale, QNorm({ r[0], -r[1], -r[2], r[3] }), kZeroPos };
        }
    }

    // ── R/L_UpperLeg: inv(C_leg) × q ────────────────────────────────────────
    {
        const Quat invC = QInv(CLeg());
        BoneTrack& tr = makeTrack("R_UpperLeg");
        BoneTrack& tl = makeTrack("L_UpperLeg");
        for (size_t fr = 0; fr < n; ++fr) {
            tr[fr] = { kUnitScale, QNorm(QMul(invC, Etq(f[P_R_UpperLeg][fr]))), kZeroPos };
            tl[fr] = { kUnitScale, QNorm(QMul(invC, Etq(f[P_L_UpperLeg][fr]))), kZeroPos };
        }
    }

    // ── R/L_LowerLeg: Z-only (no W-negate) ──────────────────────────────────
    {
        auto invLowerLeg = [](double rz) -> Quat {
            return { 0.0, 0.0, std::sin(rz/2), std::cos(rz/2) };
        };
        BoneTrack& tr = makeTrack("R_LowerLeg");
        BoneTrack& tl = makeTrack("L_LowerLeg");
        for (size_t fr = 0; fr < n; ++fr) {
            tr[fr] = { kUnitScale, invLowerLeg(f[P_R_LowerLeg][fr][2]), kZeroPos };
            tl[fr] = { kUnitScale, invLowerLeg(f[P_L_LowerLeg][fr][2]), kZeroPos };
        }
    }

    // ── R/L_Foot: direct euler ──────────────────────────────────────────────
    {
        BoneTrack& tr = makeTrack("R_Foot");
        BoneTrack& tl = makeTrack("L_Foot");
        for (size_t fr = 0; fr < n; ++fr) {
            tr[fr] = { kUnitScale, Etq(f[P_R_Foot][fr]), kZeroPos };
            tl[fr] = { kUnitScale, Etq(f[P_L_Foot][fr]), kZeroPos };
        }
    }

    for (auto& kv : out) TemporalFix(kv.second);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// tk8_writer.py — enc=3 (compressed bitstream) rotation encoder
// ─────────────────────────────────────────────────────────────────────────────
//   header 16B : u16(4) u16(6) u16(base_frame_off=160) u16(bits_per_frame)
//                u32(bitstream_off=204) u32(frame_count)
//   track table: 9 tracks × 16B = (min f32, max f32, bits u32, idx u32=0)
//   base_frame : 44B (11 floats) — static channel values + qw sign reference
//   bitstream  : LSB-first bit order; per frame the active channels plus one
//                qw sign bit. qw = sign * sqrt(max(0, 1-qx²-qy²-qz²)).

const int ENC3_BITS       = 16;
const int ENC3_NUM_TRACKS = 9;

std::vector<uint8_t> EncodeEnc3Rotation(const BoneTrack& frames, size_t frameCount) {
    auto sample = [&](size_t f, int c) -> double {
        const BoneFrame& b = frames[f];
        if (c < 3)  return b.scale[c];
        if (c < 7)  return b.quat[c - 3];
        if (c < 10) return b.pos[c - 7];
        return 0.0;
    };

    const int ACTIVE_LO = 3, ACTIVE_HI = 5, QW = 6;
    double   trackMin[ENC3_NUM_TRACKS], trackMax[ENC3_NUM_TRACKS];
    uint32_t trackBits[ENC3_NUM_TRACKS];
    std::vector<int> active;

    for (int c = 0; c < ENC3_NUM_TRACKS; ++c) {
        double mn = sample(0, c), mx = mn;
        for (size_t f = 1; f < frameCount; ++f) {
            double v = sample(f, c);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        bool isRot = (c >= ACTIVE_LO && c <= ACTIVE_HI);
        if (c == QW) {                                   // reconstructed → static
            trackMin[c] = 0.0; trackMax[c] = 0.0; trackBits[c] = 16;
        } else if (isRot && (mx - mn) > 1e-7) {
            trackMin[c] = mn; trackMax[c] = mx; trackBits[c] = ENC3_BITS;
            active.push_back(c);
        } else {                                         // static (min == max)
            trackMin[c] = mn; trackMax[c] = mn; trackBits[c] = isRot ? 0u : 16u;
        }
    }

    const uint32_t bitsPerFrame = static_cast<uint32_t>(active.size()) * ENC3_BITS + 1;

    std::vector<uint8_t> bs((static_cast<uint64_t>(frameCount) * bitsPerFrame + 7) / 8, 0);
    size_t bitPos = 0;
    auto writeBits = [&](uint32_t value, int nbits) {
        for (int i = 0; i < nbits; ++i) {
            if ((value >> i) & 1u) bs[bitPos >> 3] |= static_cast<uint8_t>(1u << (bitPos & 7));
            ++bitPos;
        }
    };

    for (size_t f = 0; f < frameCount; ++f) {
        for (int c : active) {
            double mn = trackMin[c], mx = trackMax[c];
            uint32_t maxVal = (1u << ENC3_BITS) - 1u;
            double raw = RoundHalfEven((sample(f, c) - mn) / (mx - mn) * static_cast<double>(maxVal));
            int64_t iraw = static_cast<int64_t>(raw);
            if (iraw < 0) iraw = 0;
            if (iraw > static_cast<int64_t>(maxVal)) iraw = maxVal;
            writeBits(static_cast<uint32_t>(iraw), ENC3_BITS);
        }
        writeBits(sample(f, QW) < 0 ? 1u : 0u, 1);
    }

    const uint16_t baseFrameOffset = static_cast<uint16_t>(0x10 + ENC3_NUM_TRACKS * 16);  // 160
    const uint32_t bitstreamOffset = baseFrameOffset + 44u;                               // 204

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(bitstreamOffset) + bs.size());
    PushU16(out, static_cast<uint16_t>(4));
    PushU16(out, static_cast<uint16_t>(6));
    PushU16(out, baseFrameOffset);
    PushU16(out, static_cast<uint16_t>(bitsPerFrame));
    PushU32(out, bitstreamOffset);
    PushU32(out, static_cast<uint32_t>(frameCount));
    for (int c = 0; c < ENC3_NUM_TRACKS; ++c) {
        PushF32(out, trackMin[c]);
        PushF32(out, trackMax[c]);
        PushU32(out, trackBits[c]);
        PushU32(out, 0);
    }
    for (int c = 0; c < 11; ++c) PushF32(out, sample(0, c));   // base_frame
    out.insert(out.end(), bs.begin(), bs.end());
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// tk8_writer26.py — 26-bone facing-family PANM writer
// ─────────────────────────────────────────────────────────────────────────────

const size_t FBF_FRAME_BYTES  = 44;
const uint32_t ROOT_MOTION_CSIZE = 4;

const char* kHeaderB64 =
    "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";

struct StaticB64 { const char* name; const char* b64; };
const StaticB64 kStaticB64[] = {
    { "MUKI",       "AACAPwAAgD8AAIA/AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAAAA" },
    { "KOSI_NULL2", "AACAPwAAgD8AAIA/AAAAAAAAAAAAAAAAAACAPwAAyMMAAAAAAAAAAAAAAAAAAAAA" },
    { "Hip",        "AACAPwAAgD8AAIA/AAAAP/z//z4AAAA/BAAAPwAAAAAAAAAAAAAAAAAAAAAAAAAA" },
};

struct TmplBone {
    std::string name;
    uint32_t indicator = 0, enc = 0, aux = 0, cRel = 0, cSizeField = 0;
    size_t   absB = 0;
};

struct Tmpl {
    std::vector<uint8_t> header;
    std::vector<TmplBone> bones;          // template order
    std::vector<size_t>   sortedIdx;      // stable-sorted by cRel
    std::unordered_map<std::string, std::vector<uint8_t>> statics;
    std::vector<uint8_t>  facingRest;
    bool ok = false;
};

std::string ReadTmplName(const std::vector<uint8_t>& d, size_t absA) {
    uint32_t nlen = RdU32(d.data(), absA + 0x14);
    size_t   beg  = absA + 0x18;
    size_t   end  = std::min(d.size(), beg + nlen + 4);
    std::string raw(reinterpret_cast<const char*>(d.data()) + beg,
                    reinterpret_cast<const char*>(d.data()) + end);
    size_t nul = raw.find('\0');
    if (nul != std::string::npos) return raw.substr(0, nul);
    return raw.substr(0, std::min<size_t>(nlen, raw.size()));
}

const Tmpl& GetTemplate() {
    static Tmpl t = []() {
        Tmpl r;
        if (!B64Decode(kHeaderB64, r.header)) return r;
        if (r.header.size() < 0x98 + 4) return r;

        uint32_t boneCount = RdU32(r.header.data(), 0x94);
        if (r.header.size() < 0x98 + static_cast<size_t>(boneCount) * 4) return r;

        for (uint32_t i = 0; i < boneCount; ++i) {
            size_t arrPos = 0x98 + static_cast<size_t>(i) * 4;
            size_t absA   = arrPos + RdU32(r.header.data(), arrPos);
            size_t absB   = (absA + 0x10) + RdU32(r.header.data(), absA + 0x10) - 8;
            if (absA + 0x18 > r.header.size() || absB + 0x28 > r.header.size()) return Tmpl{};
            TmplBone b;
            b.name       = ReadTmplName(r.header, absA);
            b.indicator  = RdU32(r.header.data(), absB + 0x0C);
            b.enc        = RdU32(r.header.data(), absB + 0x14);
            b.aux        = RdU32(r.header.data(), absB + 0x1C);
            b.cRel       = (b.indicator == 0x24) ? RdU32(r.header.data(), absB + 0x20) : 0;
            b.cSizeField = RdU32(r.header.data(), absB + 0x24);
            b.absB       = absB;
            r.bones.push_back(std::move(b));
        }

        r.sortedIdx.resize(r.bones.size());
        for (size_t i = 0; i < r.sortedIdx.size(); ++i) r.sortedIdx[i] = i;
        std::stable_sort(r.sortedIdx.begin(), r.sortedIdx.end(),
                         [&](size_t a, size_t b) { return r.bones[a].cRel < r.bones[b].cRel; });

        for (const StaticB64& s : kStaticB64) {
            std::vector<uint8_t> v;
            if (!B64Decode(s.b64, v)) return Tmpl{};
            r.statics[s.name] = std::move(v);
        }

        // MUKI/MUNE_jnt/HARA rest pose: identity rotation, zero position.
        const double rest[11] = { 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0 };
        for (double v : rest) PushF32(r.facingRest, v);
        r.facingRest.insert(r.facingRest.end(), 4, 0);

        r.ok = true;
        return r;
    }();
    return t;
}

bool IsEnc1Bone(const std::string& n)   { return n == "Top" || n == "Trans"; }
bool IsRootMotion(const std::string& n) { return n == "Top"; }
bool IsStaticBone(const std::string& n) { return n == "Hip" || n == "KOSI_NULL2"; }
bool IsFacingBone(const std::string& n) {
    return n == "MUKI" || n == "MUNE_jnt" || n == "HARA_ROT1";
}

void PackFrame(std::vector<uint8_t>& d, const BoneFrame& f) {
    for (int i = 0; i < 3; ++i) PushF32(d, f.scale[i]);
    for (int i = 0; i < 4; ++i) PushF32(d, f.quat[i]);
    for (int i = 0; i < 3; ++i) PushF32(d, f.pos[i]);
    PushF32(d, 0.0);
}

bool BuildTk826(const BoneTracks& boneData, size_t frameCount,
                const std::vector<std::string>& facingBones,
                std::vector<uint8_t>& out, std::string& err) {
    const Tmpl& t = GetTemplate();
    if (!t.ok) { err = "PANM: embedded 26-bone template failed to decode."; return false; }
    if (frameCount == 0) { err = "PANM: frame count is zero."; return false; }

    auto isFacingEnabled = [&](const std::string& n) {
        return std::find(facingBones.begin(), facingBones.end(), n) != facingBones.end();
    };
    auto role = [&](const std::string& n) -> int {
        if (IsEnc1Bone(n))   return 1;
        if (IsStaticBone(n)) return 2;
        if (IsFacingBone(n)) return isFacingEnabled(n) ? 3 : 2;
        return 3;   // rotation bone
    };

    const BoneFrame identityFrame = { kUnitScale, kIdentity, kZeroPos };

    std::vector<uint8_t> blockC;
    std::unordered_map<std::string, uint32_t> cRels, cSizes;

    for (size_t idx : t.sortedIdx) {
        const TmplBone& b = t.bones[idx];
        cRels[b.name] = static_cast<uint32_t>(blockC.size());
        int r = role(b.name);

        if (r == 3) {
            auto it = boneData.find(b.name);
            BoneTrack fallback;
            const BoneTrack* frames;
            if (it != boneData.end() && it->second.size() >= frameCount) {
                frames = &it->second;
            } else {
                fallback.assign(frameCount, identityFrame);
                frames = &fallback;
            }
            std::vector<uint8_t> block = EncodeEnc3Rotation(*frames, frameCount);
            blockC.insert(blockC.end(), block.begin(), block.end());
            cSizes[b.name] = static_cast<uint32_t>(block.size());
            size_t pad = (4 - (block.size() % 4)) % 4;
            blockC.insert(blockC.end(), pad, 0);
        } else if (r == 1) {
            auto it = boneData.find(b.name);
            const BoneTrack* frames =
                (it != boneData.end() && it->second.size() >= frameCount) ? &it->second : nullptr;
            for (size_t fr = 0; fr < frameCount; ++fr)
                PackFrame(blockC, frames ? (*frames)[fr] : identityFrame);
            // Root motion (Top): the c_size field is a flag (4); the frame data
            // is written in full regardless.
            cSizes[b.name] = IsRootMotion(b.name)
                             ? ROOT_MOTION_CSIZE
                             : static_cast<uint32_t>(frameCount * FBF_FRAME_BYTES);
            blockC.insert(blockC.end(), 4, 0);
        } else {
            auto it = t.statics.find(b.name);
            const std::vector<uint8_t>& sd = (it != t.statics.end()) ? it->second : t.facingRest;
            blockC.insert(blockC.end(), sd.begin(), sd.end());
            cSizes[b.name] = 44;
        }
    }

    out = t.header;
    WrU32(out, 0x40, static_cast<uint32_t>(frameCount - 1));
    uint32_t newRaw = static_cast<uint32_t>(blockC.size());
    WrU32(out, 0x54, newRaw);
    WrU32(out, 0x58, newRaw);
    WrU32(out, 0x5C, newRaw);

    for (const TmplBone& b : t.bones) {
        int r = role(b.name);
        WrU32(out, b.absB + 0x14, static_cast<uint32_t>(r));
        WrU32(out, b.absB + 0x18, (r == 1 || r == 3) ? static_cast<uint32_t>(frameCount) : 1u);
        if (b.indicator == 0x24) {
            WrU32(out, b.absB + 0x20, cRels[b.name]);
        } else if (IsRootMotion(b.name)) {
            // Top (indicator=0x20): the real data size must live at +0x20 or the
            // game's root-motion sampler never reads the Top displacement.
            WrU32(out, b.absB + 0x20, static_cast<uint32_t>(frameCount * FBF_FRAME_BYTES));
        }
        WrU32(out, b.absB + 0x24, cSizes[b.name]);
    }

    out.insert(out.end(), blockC.begin(), blockC.end());
    return true;
}

}   // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API — converter.py convert_file(..., output_26bone=True)
// ─────────────────────────────────────────────────────────────────────────────

bool ConvertT7AnimToPanm(const uint8_t* src, size_t srcLen,
                         std::vector<uint8_t>& outPanm,
                         std::string& errorMsg) {
    outPanm.clear();
    errorMsg.clear();

    // Empty/stub file: container entries whose data offset is 0 live in the
    // external com.anmbin and dump as a handful of zero bytes.
    if (srcLen <= 8) {
        errorMsg = "Empty animation (" + std::to_string(srcLen) +
                   " bytes). The entry likely points into an external com.anmbin.";
        return false;
    }
    if (!src) { errorMsg = "Null input buffer."; return false; }

    bool anyNonZero = false;
    for (size_t i = 0; i < srcLen && !anyNonZero; ++i) if (src[i]) anyNonZero = true;
    if (!anyNonZero) { errorMsg = "Empty animation (all bytes zero)."; return false; }

    uint16_t magic = RdU16(src, 0);

    uint32_t nFrames = 0;
    Tk7Frames tk7Frames;

    if (magic == 0x00C8) {
        if (!ParseTk7Fbf(src, srcLen, nFrames, tk7Frames, errorMsg)) return false;
    } else if (magic == 0x0064) {
        std::vector<std::vector<Vec3>> rawFrames;
        size_t boneCount = 0;
        if (!ParseVlcKef(src, srcLen, nFrames, rawFrames, boneCount, errorMsg)) return false;
        size_t nb = std::min<size_t>(boneCount, P_COUNT);
        if (nb < P_COUNT) {
            errorMsg = "KEF: only " + std::to_string(nb) +
                       " bones present, the 23 standard parameters are required.";
            return false;
        }
        for (int i = 0; i < P_COUNT; ++i) tk7Frames[i].resize(nFrames);
        for (uint32_t fr = 0; fr < nFrames; ++fr)
            for (size_t bi = 0; bi < nb; ++bi)
                tk7Frames[bi][fr] = ApplyContainer(rawFrames[fr][bi], bi);
    } else {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "0x%04X", magic);
        errorMsg = std::string("Unsupported TK7 format (magic=") + buf +
                   "; supported: FBF=0x00C8, VLC=0x0064).";
        return false;
    }

    bool isTtt2 = DetectNativeConvention(tk7Frames);

    // 26-bone facing family (game standard, lossless target for TK7 data).
    // Our facing (Rot × inv(orient)) matches the official cmnyg HARA in
    // coordinate system and sign; write it to both HARA_ROT1 and MUKI.
    BoneTracks boneData = ConvertTk7ToTk8Bones(tk7Frames, isTtt2, /*facingToHara=*/true);
    boneData["MUKI"] = boneData["HARA_ROT1"];

    return BuildTk826(boneData, nFrames, { "MUKI", "HARA_ROT1" }, outPanm, errorMsg);
}

size_t EstimateT7AnimSize(const uint8_t* src, size_t srcLen) {
    if (!src || srcLen < 8) return 0;
    uint16_t magic = RdU16(src, 0);
    if (magic != 0x00C8) return 0;   // KEF size is not derivable from the header
    uint32_t nParams = RdU16(src, 0x02);
    uint32_t nFrames = RdU32(src, 0x04);
    if (nParams < P_COUNT || nFrames == 0) return 0;
    return 0x08 + static_cast<size_t>(nParams) * 4 +
           static_cast<size_t>(nFrames) * static_cast<size_t>(nParams) * 12;
}
