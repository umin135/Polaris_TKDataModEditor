#pragma once
#include "fbsdata/data/ModData.h"
#include <string>
#include <cstdint>

struct ItemIdEditState {
    bool      open        = false;
    bool      pendingOpen = false;
    uint8_t   fixedA      = 2;      // 1=unique, 2=common
    int       XX          = 0;      // character id
    int       YY          = 0;      // item type id
    int       ZZZ         = 0;      // unique item id
    bool      xxManual    = false;
    bool      yyManual    = false;
    char      xxBuf[8]    = {};
    char      yyBuf[8]    = {};
    uint32_t* target      = nullptr;
};

// FbsData editor view -- loads/saves .tkmod files and renders per-bin editors.
// Layout: [Toolbar (Save/Load)] | [Editor area] | [Contents List]
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
    void RenderItemIdPopup();

    // Per-type editors
    void RenderCustomizeItemCommonEditor(ContentsBinData& bin);
    void RenderCharacterListEditor(ContentsBinData& bin);
    void RenderCustomizeItemExclusiveListEditor(ContentsBinData& bin);
    void RenderAreaListEditor(ContentsBinData& bin);
    void RenderBattleSubtitleInfoEditor(ContentsBinData& bin);
    void RenderFateDramaPlayerStartListEditor(ContentsBinData& bin);
    void RenderJukeboxListEditor(ContentsBinData& bin);
    void RenderSeriesListEditor(ContentsBinData& bin);
    void RenderTamMissionListEditor(ContentsBinData& bin);
    void RenderDramaPlayerStartListEditor(ContentsBinData& bin);
    void RenderStageListEditor(ContentsBinData& bin);
    void RenderBallPropertyListEditor(ContentsBinData& bin);
    void RenderBodyCylinderDataListEditor(ContentsBinData& bin);
    void RenderCustomizeItemUniqueListEditor(ContentsBinData& bin);
    void RenderCharacterSelectListEditor(ContentsBinData& bin);
    void RenderCustomizeItemProhibitDramaListEditor(ContentsBinData& bin);
    void RenderBattleMotionListEditor(ContentsBinData& bin);
    void RenderArcadeCpuListEditor(ContentsBinData& bin);
    void RenderBallRecommendListEditor(ContentsBinData& bin);
    void RenderBallSettingListEditor(ContentsBinData& bin);
    void RenderBattleCommonListEditor(ContentsBinData& bin);
    void RenderBattleCpuListEditor(ContentsBinData& bin);
    void RenderRankListEditor(ContentsBinData& bin);
    void RenderAssistInputListEditor(ContentsBinData& bin);

    ModData m_data;
    ItemIdEditState m_itemIdEdit;

public:
    ModData& GetModData() { return m_data; }

private:
    bool    m_showSaveResult = false;
    bool    m_lastSaveOk     = false;
    float   m_statusTimer    = 0.0f;
};
