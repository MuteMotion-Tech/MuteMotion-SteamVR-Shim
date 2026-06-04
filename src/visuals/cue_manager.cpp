#include "cue_manager.hpp"
#include "cues_impl.hpp"

CueManager::CueManager() {
    // defaults when booting up
    m_activeCue = std::make_unique<GridCue>();
    m_activeCueType = CueType::GRID;
}

CueManager::~CueManager() = default;

void CueManager::SwitchCue(CueType type) {
    if (type == m_activeCueType) {
        return; // idempotency guard so we don't blow up the memory
    }

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
        case CueType::NYAN_CAT:
            m_activeCue = std::make_unique<NyanCatCue>();
            break;
        case CueType::DVD_BOUNCE:
            m_activeCue = std::make_unique<DVDCue>();
            break;
        default:
            break;
    }
    
    m_activeCueType = type;
}

void CueManager::UpdateActiveCue(MotionOffset current_offset, float dt, const CueParams& params) {
    if (m_activeCue) {
        m_activeCue->Update(current_offset, dt, params);
    }
}

void CueManager::DrawActiveCue(int width, int height) {
    if (m_activeCue) {
        m_activeCue->Draw(width, height);
    }
}
