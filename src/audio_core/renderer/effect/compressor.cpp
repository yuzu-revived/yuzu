// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "audio_core/renderer/effect/compressor.h"

namespace AudioCore::Renderer {

void CompressorInfo::Update(BehaviorInfo::ErrorInfo& error_info,
                            const InParameterVersion1& in_params, const PoolMapper& pool_mapper) {}

void CompressorInfo::Update(BehaviorInfo::ErrorInfo& error_info,
                            const InParameterVersion2& in_params, const PoolMapper& pool_mapper) {
    auto in_specific{reinterpret_cast<const ParameterVersion1*>(in_params.specific.data())};
    auto params{reinterpret_cast<ParameterVersion1*>(parameter.data())};

    std::memcpy(params, in_specific, sizeof(ParameterVersion1));
    mix_id = in_params.mix_id;
    process_order = in_params.process_order;
    enabled = in_params.enabled;

    error_info.error_code = ResultSuccess;
    error_info.address = CpuAddr(0);
}

void CompressorInfo::UpdateForCommandGeneration() {
    if (enabled) {
        usage_state = UsageState::Enabled;
    } else {
        usage_state = UsageState::Disabled;
    }

    auto params{reinterpret_cast<ParameterVersion1*>(parameter.data())};
    params->state = ParameterState::Updated;
}

void CompressorInfo::InitializeResultState(EffectResultState& result_state) {
    auto* params{reinterpret_cast<ParameterVersion2*>(parameter.data())};
    auto* stats{reinterpret_cast<Statistics*>(result_state.state.data())};
    *stats = {};
    stats->Reset(params->channel_count);
}

void CompressorInfo::UpdateResultState(EffectResultState& cpu_state,
                                       EffectResultState& dsp_state) {
    std::memcpy(cpu_state.state.data(), dsp_state.state.data(), sizeof(Statistics));
}

CpuAddr CompressorInfo::GetWorkbuffer(s32 index) {
    return GetSingleBuffer(index);
}

} // namespace AudioCore::Renderer
