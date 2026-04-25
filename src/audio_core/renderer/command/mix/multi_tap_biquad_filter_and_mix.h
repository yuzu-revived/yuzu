// SPDX-FileCopyrightText: Copyright 2026 Yuzu Revived Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "audio_core/renderer/command/icommand.h"
#include "audio_core/renderer/voice/voice_info.h"
#include "common/common_types.h"

namespace AudioCore::ADSP::AudioRenderer {
class CommandListProcessor;
}

namespace AudioCore::Renderer {

/**
 * REV12 splitter command: apply two cascaded biquad filters to the input mix buffer and mix
 * the filtered result into the output mix buffer with optional volume ramp.
 */
struct MultiTapBiquadFilterAndMixCommand : ICommand {
    void Dump(const AudioRenderer::CommandListProcessor& processor, std::string& string) override;
    void Process(const AudioRenderer::CommandListProcessor& processor) override;
    bool Verify(const AudioRenderer::CommandListProcessor& processor) override;

    /// Input mix buffer index.
    s16 input;
    /// Output mix buffer index.
    s16 output;
    /// Previous (ramp-start) volume.
    f32 prev_volume;
    /// Current (ramp-end) volume.
    f32 volume;
    /// First-stage biquad coefficients.
    VoiceInfo::BiquadFilterParameter biquad0;
    /// Second-stage biquad coefficients.
    VoiceInfo::BiquadFilterParameter biquad1;
    /// First-stage active state.
    CpuAddr state0;
    /// Second-stage active state.
    CpuAddr state1;
    /// First-stage previous state (for cross-mix-buffer rewind).
    CpuAddr prev_state0;
    /// Second-stage previous state.
    CpuAddr prev_state1;
    /// Pointer to the slot that records the last filtered sample.
    CpuAddr prev_sample;
    /// True if the first-stage state needs initialisation.
    bool needs_init0;
    /// True if the second-stage state needs initialisation.
    bool needs_init1;
    /// True if a non-zero volume ramp should be applied.
    bool has_volume_ramp;
    /// True if this is the first mix buffer in the destination's mix loop.
    bool is_first_mix_buffer;
};

} // namespace AudioCore::Renderer
