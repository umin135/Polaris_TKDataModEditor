#pragma once
#include "fbsdata/data/ModData.h"
#include "fbsdata/editor/TkmodManagerView.h"
#include <string>
#include <cstdint>
#include <functional>
#include <vector>

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

    // Dispatches to the per-type editor for 'bin'. Used by TkmodManagerView for read-only preview.
    void RenderBinReadOnly(ContentsBinData& bin);

    // Source descriptor for the merged overview table.
    struct BinViewSource
    {
        const char*      filename;  // e.g. "Anna.tkmod"
        const char*      path;      // full path for Open callback
        ContentsBinData* bin;
        const ModData*   modData;   // parent mod (for cross-bin label lookups)
    };

    // Renders all entries from all sources in a single merged table with a "tkmod" column.
    void RenderBinMergedOverview(
        const std::vector<BinViewSource>& sources,
        const std::function<void(const std::string&)>& openCb);

private:
    void RenderToolbar();
    void RenderEditorArea();
    void RenderContentsList(float listWidth);
    void RenderAddPopup();
    void RenderInfoEditPopup();
    void RenderSaveConfirmPopup();
    void DoSave();
    void DoSaveAs();

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
    void RenderCustomizePanelListEditor(ContentsBinData& bin);
    void RenderCustomizeItemExceptionEditor(ContentsBinData& bin);

    ModData m_data;

public:
    ModData& GetModData() { return m_data; }

private:
    bool    m_modActive          = false; // true after New or successful Load
    bool    m_showSaveResult    = false;
    bool    m_lastSaveOk        = false;
    float   m_statusTimer       = 0.0f;
    bool        m_infoEditPending    = false;  // open info edit popup next frame
    bool        m_saveConfirmPending = false;  // open save-without-info confirm popup next frame
    bool        m_pendingDoSaveAs   = false;   // if true, DoSaveAs() on confirm; else DoSave()
    bool        m_renderReadOnly    = false;   // suppresses Add/Import/Delete buttons in editors
    std::string m_currentFilePath;             // empty = not yet saved to disk
    TkmodManagerView m_managerView;
};
