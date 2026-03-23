#pragma once
#include "data/ModData.h"
#include <string>

// FbsData editor view
// Layout: [Toolbar] | [Editor area] | [Contents List]
class FbsDataView
{
public:
    FbsDataView() = default;
    void Render();

    // Load a .tkmod file directly (used when launched via double-click).
    // Returns true on success.
    bool LoadFromPath(const std::string& path);

private:
    void RenderToolbar();
    void RenderEditorArea();
    void RenderContentsList(float listWidth);
    void RenderAddPopup();

    // Per-type editors
    void RenderCustomizeItemCommonEditor(ContentsBinData& bin);
    void RenderCharacterListEditor(ContentsBinData& bin);
    void RenderCustomizeItemExclusiveListEditor(ContentsBinData& bin);

    ModData m_data;
    bool    m_showSaveResult = false;
    bool    m_lastSaveOk     = false;
    float   m_statusTimer    = 0.0f;
};
