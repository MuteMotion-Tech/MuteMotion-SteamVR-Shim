#include "cue_manager.hpp"
#include "cues_impl.hpp"

CueManager::CueManager() {
    // Default boot shape
    m_activeCue = std::make_unique<GridCue>();
}

CueManager::~CueManager() = default;

void CueManager::SwitchCue(CueType type) {
    switch (type) {
        case CueType::SINGLE_DOT:
            m_activeCue = std::make_unique<SingleDotCue>();
            break;
        case CueType::GRID:
            m_activeCue = std::make_unique<GridCue>();
            break;
        case CueType::HORIZON:
            m_activeCue = std::make_unique<ArtificialHorizonCue>();
            break;
        case CueType::LIQUID:
            m_activeCue = std::make_unique<LiquidWaveCue>();
            break;
    }
}

void CueManager::UpdateActiveCue(MotionOffset current_offset, float dt) {
    if (m_activeCue) {
        m_activeCue->Update(current_offset, dt);
    }
}

void CueManager::DrawActiveCue(int width, int height) {
    if (m_activeCue) {
        m_activeCue->Draw(width, height);
    }
}
