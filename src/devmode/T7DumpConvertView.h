#pragma once
#ifdef _DEBUG

#include <string>

// -----------------------------------------------------------------------------
//  T7DumpConvertView  (Debug only)
//
//  Load a T7DUMP01 .bin (+ optional anims.json / 0_body) and convert to
//  TK7_<name>/moveset.motbin (+ moveset.anmbin when body anims are present).
// -----------------------------------------------------------------------------

class T7DumpConvertView
{
public:
    void Render();

private:
    void RunConvert();
    void ReloadAliases();
    void RefreshAnimsDetect();

    char        m_dumpPath[1024] = {};
    std::string m_status;
    bool        m_statusOk = false;
    std::string m_aliasStatus;
    bool        m_aliasOk = false;

    bool        m_convertBodyAnims = true;
    std::string m_animsFolder;     // detected root (empty if none)
    int         m_bodyClipCount = 0;
    std::string m_lastDetectPath;  // avoid re-scanning every frame unless path changes
};

#endif // _DEBUG
