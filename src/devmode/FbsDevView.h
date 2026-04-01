#pragma once
#ifdef _DEBUG

#include "data/ModData.h"
#include <string>

// -----------------------------------------------------------------------------
//  FbsDevView  --  Developer-mode fbsdata editor
//
//  Reads/writes original .bin files (FlatBuffers binary) instead of .tkmod.
//  Only compiled in Debug builds.
// -----------------------------------------------------------------------------

class FbsDevView
{
public:
    void Render();

private:
    void RenderToolbar();
    void RenderEditorArea();
    void RenderContentsList(float listWidth);

    // Per-type editors (show ALL fields including unknowns)
    void RenderCustomizeItemCommonEditor(ContentsBinData& bin);
    void RenderCharacterEditor(ContentsBinData& bin);

    ModData m_data;

    // Status feedback
    bool        m_showExportResult = false;
    bool        m_lastExportOk     = false;
    float       m_statusTimer      = 0.0f;
    std::string m_statusMessage;
};

#endif // _DEBUG
