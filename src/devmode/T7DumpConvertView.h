#pragma once
#ifdef _DEBUG

#include <string>

// -----------------------------------------------------------------------------
//  T7DumpConvertView  (Debug only)
//
//  Load a T7DUMP01 .bin (from res-wip/dump_t7_motbin.py) and convert it to
//  TK7_<name>/moveset.motbin under the configured Moveset root. No live T7.
// -----------------------------------------------------------------------------

class T7DumpConvertView
{
public:
    void Render();

private:
    void RunConvert();
    void ReloadAliases();

    char        m_dumpPath[1024] = {};
    std::string m_status;
    bool        m_statusOk = false;
    std::string m_aliasStatus;
    bool        m_aliasOk = false;
};

#endif // _DEBUG
