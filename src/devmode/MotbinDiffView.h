#pragma once
#ifdef _DEBUG

#include <string>

// -----------------------------------------------------------------------------
//  MotbinDiffView  (Debug only)
//
//  Loads two state-1 .motbin files (original vs extracted) and writes a
//  field-level diff report to <movesetRootDir>\report.txt.
//  Also supports live state-3 -> state-1 -> state-3 round-trip check.
// -----------------------------------------------------------------------------

class MotbinDiffView
{
public:
    void Render();

private:
    void RunReport();
    void RunLiveRoundTrip();

    char        m_pathA[1024] = {};   // original motbin
    char        m_pathB[1024] = {};   // extracted motbin
    std::string m_status;
    bool        m_statusOk    = false;
};

#endif // _DEBUG
