#pragma once
#include <vector>
#include <string>

// Bones shown in x-ray skeleton overlay for each animation category.
// Fullbody is fully populated; other categories are reserved for future use.
//
// NOTE: This filter only affects the x-ray skeleton overlay (DrawSkeletonLines).
//       Animation playback and bone transforms are completely unaffected.

namespace SkeletonBoneFilter {

inline const std::vector<std::string>& GetList(int cat)
{
    static const std::vector<std::string> kFullbody = {
        "Trans", 
        "Top", 
        "Hip", 
        "HARA_ROT1",
        "Spine1", 
        "Spine2", 
        "Spine3",
        "Neck", 
        "Head",
        "L_Shoulder", 
        "L_UpperArm", 
        "L_LowerArm", 
        "L_Hand",
        "R_Shoulder", 
        "R_UpperArm", 
        "R_LowerArm", 
        "R_Hand",
        "L_UpperLeg", 
        "L_LowerLeg", 
        "L_Foot", 
        "L_Toe",
        "R_UpperLeg", 
        "R_LowerLeg", 
        "R_Foot", 
        "R_Toe",
    };
    static const std::vector<std::string> kHand   = {};  // TODO
    static const std::vector<std::string> kFacial = {};  // TODO
    static const std::vector<std::string> kSwing  = {};  // TODO
    static const std::vector<std::string> kCamera = {};  // TODO
    static const std::vector<std::string> kExtra  = {};  // TODO
    static const std::vector<std::string> kEmpty  = {};

    switch (cat) {
        case 0: return kFullbody;
        case 1: return kHand;
        case 2: return kFacial;
        case 3: return kSwing;
        case 4: return kCamera;
        case 5: return kExtra;
        default: return kEmpty;
    }
}

} // namespace SkeletonBoneFilter
