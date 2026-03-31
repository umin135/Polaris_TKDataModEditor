#pragma once
#include "MovesetExtractor.h"
#include <string>
#include <functional>

// ─────────────────────────────────────────────────────────────
//  ExtractorView
//  Two-button panel: "Extract P1" / "Extract P2"
//  Each button auto-connects, refreshes slot info, and extracts.
// ─────────────────────────────────────────────────────────────
class ExtractorView {
public:
    explicit ExtractorView(const std::string& movesetRootDir);

    // Render only the Extract P1 / Extract P2 buttons.
    void RenderButtons();

    // Render the status log, slot info, and save-path hint.
    void RenderLog();

    // Update the destination folder (called when config changes).
    void SetDestFolder(const std::string& dir) { m_destFolder = dir; }

    // Called after a successful extraction so the caller can
    // trigger a moveset list refresh.
    void SetOnExtractSuccess(std::function<void()> cb) { m_onSuccess = std::move(cb); }

private:
    // Attempt connect + refresh + extract for a given slot in one step.
    void TryExtract(int slotIndex);

    MovesetExtractor      m_extractor;
    std::string           m_destFolder;
    std::string           m_lastMsg;
    bool                  m_lastOk = false;
    std::function<void()> m_onSuccess;
};
