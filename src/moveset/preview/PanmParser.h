#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Per-frame transform for one bone.
// position unit: cm  (divide by 100 for metres)
// rotation: quaternion XYZW
struct BoneSample {
    float scale[3];     // sx, sy, sz
    float rotation[4];  // qx, qy, qz, qw
    float position[3];  // px, py, pz  (cm)
};

// All frames for one bone.
struct BoneTrack {
    std::string             name;
    std::vector<BoneSample> frames; // [0 .. bone_frames-1]
};

// Result of parsing one PANM file.
struct ParsedAnim {
    uint32_t                            totalFrames = 0;
    std::vector<BoneTrack>              bones;
    std::unordered_map<std::string,int> nameToIdx;

    const BoneTrack* FindBone(const std::string& name) const {
        auto it = nameToIdx.find(name);
        return (it != nameToIdx.end()) ? &bones[it->second] : nullptr;
    }

    // Returns a zeroed sample (identity pose) when bone/frame is out of range.
    BoneSample SampleAt(const std::string& name, uint32_t frame) const;
};

// Parse from a byte buffer already in memory.
bool ParsePanm(const std::vector<uint8_t>& data,
               ParsedAnim&                 out,
               std::string&                errorMsg);

// Open file from disk, then call ParsePanm.
bool ParsePanmFile(const std::string& path,
                   ParsedAnim&        out,
                   std::string&       errorMsg);
