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
 * REV12 splitter command: apply a single biquad filter to the input mix buffer and mix the
 * filtered result into the output mix buffer with optional volume ramp.
 */
struct BiquadFilterAndMixCommand : ICommand {
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
    /// Biquad filter coefficients for this filter.
    VoiceInfo::BiquadFilterParameter biquad;
    /// Pointer to the active filter state.
    CpuAddr state;
    /// Pointer to the previous-iteration filter state, used for cross-buffer rewind.
    CpuAddr prev_state;
    /// Pointer to the slot that records the last filtered sample (for depopping).
    CpuAddr prev_sample;
    /// True if the filter state needs initialisation this call.
    bool needs_init;
    /// True if a non-zero volume ramp should be applied.
    bool has_volume_ramp;
    /// True if this is the first mix buffer in the destination's mix loop. Controls how the
    /// previous-state slot is updated to keep the chain in sync across mix buffers.
    bool is_first_mix_buffer;
};

} // namespace AudioCore::Renderer
