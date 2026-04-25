// SPDX-FileCopyrightText: Copyright 2026 Yuzu Revived Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>

#include "audio_core/adsp/apps/audio_renderer/command_list_processor.h"
#include "audio_core/renderer/command/effect/biquad_filter.h"
#include "audio_core/renderer/command/mix/biquad_filter_and_mix.h"
#include "audio_core/renderer/voice/voice_state.h"
#include "common/bit_cast.h"
#include "common/fixed_point.h"
#include "common/logging/log.h"

namespace AudioCore::Renderer {

namespace {

/// Apply a biquad filter (float path) and mix the filtered result into output, optionally
/// applying a per-sample volume ramp. Returns the last filtered+volumed sample, used for
/// depopping.
s32 ApplyBiquadFilterAndMix(std::span<s32> output, std::span<const s32> input,
                            std::array<s16, 3>& b_, std::array<s16, 2>& a_,
                            VoiceState::BiquadFilterState& state, const u32 sample_count,
                            const f32 volume_, const f32 ramp_) {
    constexpr f64 min{static_cast<f64>(std::numeric_limits<s32>::min())};
    constexpr f64 max{static_cast<f64>(std::numeric_limits<s32>::max())};
    const std::array<f64, 3> b{Common::FixedPoint<50, 14>::from_base(b_[0]).to_double(),
                               Common::FixedPoint<50, 14>::from_base(b_[1]).to_double(),
                               Common::FixedPoint<50, 14>::from_base(b_[2]).to_double()};
    const std::array<f64, 2> a{Common::FixedPoint<50, 14>::from_base(a_[0]).to_double(),
                               Common::FixedPoint<50, 14>::from_base(a_[1]).to_double()};
    std::array<f64, 4> s{Common::BitCast<f64>(state.s0), Common::BitCast<f64>(state.s1),
                         Common::BitCast<f64>(state.s2), Common::BitCast<f64>(state.s3)};

    f64 volume{volume_};
    const f64 ramp{ramp_};
    s32 last_mixed{0};

    for (u32 i = 0; i < sample_count; i++) {
        const f64 in_sample{static_cast<f64>(input[i])};
        const f64 sample{in_sample * b[0] + s[0] * b[1] + s[1] * b[2] + s[2] * a[0] + s[3] * a[1]};

        s[1] = s[0];
        s[0] = in_sample;
        s[3] = s[2];
        s[2] = sample;

        const f64 mixed{sample * volume};
        const f64 clamped{std::clamp(mixed, min, max)};
        last_mixed = static_cast<s32>(clamped);
        output[i] = static_cast<s32>(
            std::clamp(static_cast<f64>(output[i]) + clamped, min, max));

        volume += ramp;
    }

    state.s0 = Common::BitCast<s64>(s[0]);
    state.s1 = Common::BitCast<s64>(s[1]);
    state.s2 = Common::BitCast<s64>(s[2]);
    state.s3 = Common::BitCast<s64>(s[3]);
    return last_mixed;
}

} // namespace

void BiquadFilterAndMixCommand::Dump(
    [[maybe_unused]] const AudioRenderer::CommandListProcessor& processor, std::string& string) {
    string += fmt::format(
        "BiquadFilterAndMixCommand\n\tinput {:02X} output {:02X} volume {:.6f} prev_volume {:.6f} "
        "needs_init {} has_volume_ramp {} is_first {}\n",
        input, output, volume, prev_volume, needs_init, has_volume_ramp, is_first_mix_buffer);
}

void BiquadFilterAndMixCommand::Process(const AudioRenderer::CommandListProcessor& processor) {
    auto* current{reinterpret_cast<VoiceState::BiquadFilterState*>(state)};
    auto* previous{reinterpret_cast<VoiceState::BiquadFilterState*>(prev_state)};
    auto* prev_sample_ptr{reinterpret_cast<s32*>(prev_sample)};

    if (needs_init) {
        *current = {};
    } else if (is_first_mix_buffer) {
        *previous = *current;
    } else {
        *current = *previous;
    }

    auto input_buffer{
        processor.mix_buffers.subspan(input * processor.sample_count, processor.sample_count)};
    auto output_buffer{
        processor.mix_buffers.subspan(output * processor.sample_count, processor.sample_count)};

    const f32 ramp = has_volume_ramp
                         ? (volume - prev_volume) / static_cast<f32>(processor.sample_count)
                         : 0.0f;
    const f32 start_volume = has_volume_ramp ? prev_volume : volume;

    const s32 last_mixed = ApplyBiquadFilterAndMix(output_buffer, input_buffer, biquad.b, biquad.a,
                                                   *current, processor.sample_count, start_volume,
                                                   ramp);
    if (prev_sample_ptr != nullptr) {
        *prev_sample_ptr = last_mixed;
    }
}

bool BiquadFilterAndMixCommand::Verify(
    [[maybe_unused]] const AudioRenderer::CommandListProcessor& processor) {
    return true;
}

} // namespace AudioCore::Renderer
