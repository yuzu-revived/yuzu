// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "audio_core/renderer/effect/effect_context.h"
#include "common/logging/log.h"

namespace AudioCore::Renderer {

void EffectContext::Initialize(std::span<EffectInfoBase> effect_infos_, const u32 effect_count_,
                               std::span<EffectResultState> result_states_cpu_,
                               std::span<EffectResultState> result_states_dsp_,
                               const size_t dsp_state_count_) {
    effect_infos = effect_infos_;
    effect_count = effect_count_;
    result_states_cpu = result_states_cpu_;
    result_states_dsp = result_states_dsp_;
    dsp_state_count = dsp_state_count_;
}

EffectInfoBase& EffectContext::GetInfo(const u32 index) {
    return effect_infos[index];
}

EffectResultState& EffectContext::GetResultState(const u32 index) {
    return result_states_cpu[index];
}

EffectResultState& EffectContext::GetDspSharedResultState(const u32 index) {
    return result_states_dsp[index];
}

u32 EffectContext::GetCount() const {
    return effect_count;
}

void EffectContext::UpdateStateByDspShared() {
    // Bound the loop by the smallest of the three spans to avoid OOB virtual dispatch on a
    // past-the-end EffectInfoBase reference (corrupted vtable -> host crash). All three
    // should be the same size when V2 effect-info is enabled, but defending is cheap.
    const size_t bound = std::min({static_cast<size_t>(dsp_state_count), effect_infos.size(),
                                   result_states_cpu.size(), result_states_dsp.size()});
    if (bound != dsp_state_count) {
        LOG_ERROR(Service_Audio,
                  "EffectContext span size mismatch (dsp_state_count={}, effect_infos={}, "
                  "result_states_cpu={}, result_states_dsp={}); clamping to {}",
                  dsp_state_count, effect_infos.size(), result_states_cpu.size(),
                  result_states_dsp.size(), bound);
    }
    // De-virtualized dispatch. effect_infos entries are constructed in-place via
    // ResetEffect() on the IPC thread (effect_reset.h), which does *effect = {} followed
    // by std::construct_at<Derived>() — racing the renderer thread that reads through the
    // vtable here, with no synchronization. Only LightLimiter and Compressor actually
    // override UpdateResultState with real work; both simply memcpy the full 0x80-byte
    // result state. Avoid the vtable load entirely by dispatching on the (non-virtual)
    // type field. Unknown / Invalid types are skipped.
    for (size_t i = 0; i < bound; i++) {
        switch (effect_infos[i].GetType()) {
        case EffectInfoBase::Type::Compressor:
        case EffectInfoBase::Type::LightLimiter:
            std::memcpy(result_states_cpu[i].state.data(), result_states_dsp[i].state.data(),
                        result_states_cpu[i].state.size());
            break;
        default:
            break;
        }
    }
}

} // namespace AudioCore::Renderer
