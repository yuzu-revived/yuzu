// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "audio_core/common/common.h"
#include "audio_core/renderer/effect/effect_info_base.h"
#include "common/common_types.h"

namespace AudioCore::Renderer {

class BiquadFilterInfo : public EffectInfoBase {
public:
    struct ParameterVersion1 {
        /* 0x00 */ std::array<s8, MaxChannels> inputs;
        /* 0x06 */ std::array<s8, MaxChannels> outputs;
        /* 0x0C */ std::array<s16, 3> b;
        /* 0x12 */ std::array<s16, 2> a;
        /* 0x16 */ s8 channel_count;
        /* 0x17 */ ParameterState state;
    };
    static_assert(sizeof(ParameterVersion1) <= sizeof(EffectInfoBase::InParameterVersion1),
                  "BiquadFilterInfo::ParameterVersion1 has the wrong size!");

    struct ParameterVersion2 {
        /* 0x00 */ std::array<s8, MaxChannels> inputs;
        /* 0x06 */ std::array<s8, MaxChannels> outputs;
        /* 0x0C */ std::array<s16, 3> b;
        /* 0x12 */ std::array<s16, 2> a;
        /* 0x16 */ s8 channel_count;
        /* 0x17 */ ParameterState state;
    };
    static_assert(sizeof(ParameterVersion2) <= sizeof(EffectInfoBase::InParameterVersion2),
                  "BiquadFilterInfo::ParameterVersion2 has the wrong size!");

    /**
     * REV15 biquad filter effect parameter (0x28 bytes). Same field set as ParameterVersion2,
     * but the b/a coefficients are now f32 instead of Q14 s16. The wire format includes a
     * 4-byte padding at 0x0C so the float coefficients are 4-byte aligned (matches Ryujinx
     * BiquadFilterEffectParameter2 and Citron-Neo). yuzu converts to ParameterVersion2 at
     * the parsing gateway (info_updater) by f32 -> Q14 cast.
     */
    struct ParameterVersion3 {
        /* 0x00 */ std::array<s8, MaxChannels> inputs;
        /* 0x06 */ std::array<s8, MaxChannels> outputs;
        /* 0x0C */ u32 padding;
        /* 0x10 */ std::array<f32, 3> b;
        /* 0x1C */ std::array<f32, 2> a;
        /* 0x24 */ s8 channel_count;
        /* 0x25 */ ParameterState state;
        /* 0x26 */ std::array<u8, 2> reserved;
    };
    static_assert(sizeof(ParameterVersion3) == 0x28,
                  "BiquadFilterInfo::ParameterVersion3 has the wrong size!");

    /**
     * Update the info with new parameters, version 1.
     *
     * @param error_info  - Used to write call result code.
     * @param in_params   - New parameters to update the info with.
     * @param pool_mapper - Pool for mapping buffers.
     */
    void Update(BehaviorInfo::ErrorInfo& error_info, const InParameterVersion1& in_params,
                const PoolMapper& pool_mapper) override;

    /**
     * Update the info with new parameters, version 2.
     *
     * @param error_info  - Used to write call result code.
     * @param in_params   - New parameters to update the info with.
     * @param pool_mapper - Pool for mapping buffers.
     */
    void Update(BehaviorInfo::ErrorInfo& error_info, const InParameterVersion2& in_params,
                const PoolMapper& pool_mapper) override;

    /**
     * Update the info after command generation. Usually only changes its state.
     */
    void UpdateForCommandGeneration() override;

    /**
     * Initialize a new result state. Version 2 only, unused.
     *
     * @param result_state - Result state to initialize.
     */
    void InitializeResultState(EffectResultState& result_state) override;

    /**
     * Update the host-side state with the ADSP-side state. Version 2 only, unused.
     *
     * @param cpu_state - Host-side result state to update.
     * @param dsp_state - AudioRenderer-side result state to update from.
     */
    void UpdateResultState(EffectResultState& cpu_state, EffectResultState& dsp_state) override;
};

} // namespace AudioCore::Renderer
