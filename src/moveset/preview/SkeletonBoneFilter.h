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
    static const std::vector<std::string> kHand = {
        // Wrists
        "L_Hand", "R_Hand",
        // Thumbs
        "L_Thumb0", "L_Thumb1", "L_Thumb2",
        "R_Thumb0", "R_Thumb1", "R_Thumb2",
        // Index
        "L_Index0", "L_Index1", "L_Index2", "L_Index3",
        "R_Index0", "R_Index1", "R_Index2", "R_Index3",
        // Middle
        "L_Middle0", "L_Middle1", "L_Middle2", "L_Middle3",
        "R_Middle0", "R_Middle1", "R_Middle2", "R_Middle3",
        // Ring
        "L_Ring0", "L_Ring1", "L_Ring2", "L_Ring3",
        "R_Ring0", "R_Ring1", "R_Ring2", "R_Ring3",
        // Pinky
        "L_Pinky0", "L_Pinky1", "L_Pinky2", "L_Pinky3",
        "R_Pinky0", "R_Pinky1", "R_Pinky2", "R_Pinky3",
    };
    static const std::vector<std::string> kFacial = {
        "Head",
        "R_UpEyelid", "R_LowEyelid", "R_Eye",
        "L_UpEyelid", "L_LowEyelid", "L_Eye",
        "R_LipCorner", "L_LipCorner",
        "C_LowLip", "C_UpLip",
        "C_Jaw_Root", "C_LowJaw",
        "R_Cheek1", "L_Cheek1",
    };
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
