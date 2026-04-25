// SPDX-FileCopyrightText: Copyright 2026 Yuzu Revived Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>

#include "audio_core/adsp/apps/audio_renderer/command_list_processor.h"
#include "audio_core/renderer/command/mix/multi_tap_biquad_filter_and_mix.h"
#include "audio_core/renderer/voice/voice_state.h"
#include "common/bit_cast.h"
#include "common/fixed_point.h"
#include "common/logging/log.h"

namespace AudioCore::Renderer {

namespace {

s32 ApplyDoubleBiquadFilterAndMix(std::span<s32> output, std::span<const s32> input,
                                  std::array<s16, 3>& b0_, std::array<s16, 2>& a0_,
                                  std::array<s16, 3>& b1_, std::array<s16, 2>& a1_,
                                  VoiceState::BiquadFilterState& state0,
                                  VoiceState::BiquadFilterState& state1, const u32 sample_count,
                                  const f32 volume_, const f32 ramp_) {
    constexpr f64 min{static_cast<f64>(std::numeric_limits<s32>::min())};
    constexpr f64 max{static_cast<f64>(std::numeric_limits<s32>::max())};
    const std::array<f64, 3> b0{Common::FixedPoint<50, 14>::from_base(b0_[0]).to_double(),
                                Common::FixedPoint<50, 14>::from_base(b0_[1]).to_double(),
                                Common::FixedPoint<50, 14>::from_base(b0_[2]).to_double()};
    const std::array<f64, 2> a0{Common::FixedPoint<50, 14>::from_base(a0_[0]).to_double(),
                                Common::FixedPoint<50, 14>::from_base(a0_[1]).to_double()};
    const std::array<f64, 3> b1{Common::FixedPoint<50, 14>::from_base(b1_[0]).to_double(),
                                Common::FixedPoint<50, 14>::from_base(b1_[1]).to_double(),
                                Common::FixedPoint<50, 14>::from_base(b1_[2]).to_double()};
    const std::array<f64, 2> a1{Common::FixedPoint<50, 14>::from_base(a1_[0]).to_double(),
                                Common::FixedPoint<50, 14>::from_base(a1_[1]).to_double()};
    std::array<f64, 4> s0{Common::BitCast<f64>(state0.s0), Common::BitCast<f64>(state0.s1),
                          Common::BitCast<f64>(state0.s2), Common::BitCast<f64>(state0.s3)};
    std::array<f64, 4> s1{Common::BitCast<f64>(state1.s0), Common::BitCast<f64>(state1.s1),
                          Common::BitCast<f64>(state1.s2), Common::BitCast<f64>(state1.s3)};

    f64 volume{volume_};
    const f64 ramp{ramp_};
    s32 last_mixed{0};

    for (u32 i = 0; i < sample_count; i++) {
        f64 in_sample{static_cast<f64>(input[i])};
        f64 sample{in_sample * b0[0] + s0[0] * b0[1] + s0[1] * b0[2] + s0[2] * a0[0] +
                   s0[3] * a0[1]};

        s0[1] = s0[0];
        s0[0] = in_sample;
        s0[3] = s0[2];
        s0[2] = sample;

        in_sample = sample;
        sample = in_sample * b1[0] + s1[0] * b1[1] + s1[1] * b1[2] + s1[2] * a1[0] + s1[3] * a1[1];

        s1[1] = s1[0];
        s1[0] = in_sample;
        s1[3] = s1[2];
        s1[2] = sample;

        const f64 mixed{sample * volume};
        const f64 clamped{std::clamp(mixed, min, max)};
        last_mixed = static_cast<s32>(clamped);
        output[i] = static_cast<s32>(
            std::clamp(static_cast<f64>(output[i]) + clamped, min, max));

        volume += ramp;
    }

    state0.s0 = Common::BitCast<s64>(s0[0]);
    state0.s1 = Common::BitCast<s64>(s0[1]);
    state0.s2 = Common::BitCast<s64>(s0[2]);
    state0.s3 = Common::BitCast<s64>(s0[3]);
    state1.s0 = Common::BitCast<s64>(s1[0]);
    state1.s1 = Common::BitCast<s64>(s1[1]);
    state1.s2 = Common::BitCast<s64>(s1[2]);
    state1.s3 = Common::BitCast<s64>(s1[3]);
    return last_mixed;
}

} // namespace

void MultiTapBiquadFilterAndMixCommand::Dump(
    [[maybe_unused]] const AudioRenderer::CommandListProcessor& processor, std::string& string) {
    string += fmt::format(
        "MultiTapBiquadFilterAndMixCommand\n\tinput {:02X} output {:02X} volume {:.6f} prev_volume "
        "{:.6f} needs_init0 {} needs_init1 {} has_volume_ramp {} is_first {}\n",
        input, output, volume, prev_volume, needs_init0, needs_init1, has_volume_ramp,
        is_first_mix_buffer);
}

void MultiTapBiquadFilterAndMixCommand::Process(
    const AudioRenderer::CommandListProcessor& processor) {
    auto* current0{reinterpret_cast<VoiceState::BiquadFilterState*>(state0)};
    auto* current1{reinterpret_cast<VoiceState::BiquadFilterState*>(state1)};
    auto* previous0{reinterpret_cast<VoiceState::BiquadFilterState*>(prev_state0)};
    auto* previous1{reinterpret_cast<VoiceState::BiquadFilterState*>(prev_state1)};
    auto* prev_sample_ptr{reinterpret_cast<s32*>(prev_sample)};

    auto sync = [&](VoiceState::BiquadFilterState& cur, VoiceState::BiquadFilterState& prev,
                    bool needs_init) {
        if (needs_init) {
            cur = {};
        } else if (is_first_mix_buffer) {
            prev = cur;
        } else {
            cur = prev;
        }
    };
    sync(*current0, *previous0, needs_init0);
    sync(*current1, *previous1, needs_init1);

    auto input_buffer{
        processor.mix_buffers.subspan(input * processor.sample_count, processor.sample_count)};
    auto output_buffer{
        processor.mix_buffers.subspan(output * processor.sample_count, processor.sample_count)};

    const f32 ramp = has_volume_ramp
                         ? (volume - prev_volume) / static_cast<f32>(processor.sample_count)
                         : 0.0f;
    const f32 start_volume = has_volume_ramp ? prev_volume : volume;

    const s32 last_mixed = ApplyDoubleBiquadFilterAndMix(
        output_buffer, input_buffer, biquad0.b, biquad0.a, biquad1.b, biquad1.a, *current0,
        *current1, processor.sample_count, start_volume, ramp);
    if (prev_sample_ptr != nullptr) {
        *prev_sample_ptr = last_mixed;
    }
}

bool MultiTapBiquadFilterAndMixCommand::Verify(
    [[maybe_unused]] const AudioRenderer::CommandListProcessor& processor) {
    return true;
}

} // namespace AudioCore::Renderer
