// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <limits>

#include "audio_core/renderer/splitter/splitter_destinations_data.h"

namespace AudioCore::Renderer {

namespace {
/// Q14 fixed-point conversion of an f32 biquad coefficient. Saturates to s16 range.
constexpr s16 BiquadFloatToQ14(f32 coefficient) {
    const f32 scaled = std::round(coefficient * static_cast<f32>(1 << 14));
    const f32 clamped = std::clamp(scaled, static_cast<f32>(std::numeric_limits<s16>::min()),
                                   static_cast<f32>(std::numeric_limits<s16>::max()));
    return static_cast<s16>(clamped);
}

VoiceInfo::BiquadFilterParameter ConvertBiquadFloatToFixed(
    const SplitterDestinationData::BiquadFilterParameterFloat& src) {
    VoiceInfo::BiquadFilterParameter dst{};
    dst.enabled = src.enabled;
    dst.b[0] = BiquadFloatToQ14(src.b[0]);
    dst.b[1] = BiquadFloatToQ14(src.b[1]);
    dst.b[2] = BiquadFloatToQ14(src.b[2]);
    dst.a[0] = BiquadFloatToQ14(src.a[0]);
    dst.a[1] = BiquadFloatToQ14(src.a[1]);
    return dst;
}
} // namespace

SplitterDestinationData::SplitterDestinationData(const s32 id_) : id{id_} {}

void SplitterDestinationData::ClearMixVolume() {
    mix_volumes.fill(0.0f);
    prev_mix_volumes.fill(0.0f);
}

s32 SplitterDestinationData::GetId() const {
    return id;
}

bool SplitterDestinationData::IsConfigured() const {
    return in_use && destination_id != UnusedMixId;
}

s32 SplitterDestinationData::GetMixId() const {
    return destination_id;
}

f32 SplitterDestinationData::GetMixVolume(const u32 index) const {
    if (index >= mix_volumes.size()) {
        LOG_ERROR(Service_Audio, "SplitterDestinationData::GetMixVolume Invalid index {}", index);
        return 0.0f;
    }
    return mix_volumes[index];
}

std::span<f32> SplitterDestinationData::GetMixVolume() {
    return mix_volumes;
}

f32 SplitterDestinationData::GetMixVolumePrev(const u32 index) const {
    if (index >= prev_mix_volumes.size()) {
        LOG_ERROR(Service_Audio, "SplitterDestinationData::GetMixVolumePrev Invalid index {}",
                  index);
        return 0.0f;
    }
    return prev_mix_volumes[index];
}

std::span<f32> SplitterDestinationData::GetMixVolumePrev() {
    return prev_mix_volumes;
}

void SplitterDestinationData::Update(const InParameter& params,
                                     const bool is_prev_volume_reset_supported) {
    if (params.id != id || params.magic != GetSplitterSendDataMagic()) {
        return;
    }

    destination_id = params.mix_id;
    mix_volumes = params.mix_volumes;

    const bool reset_prev = is_prev_volume_reset_supported ? params.reset_prev_volume
                                                           : (!in_use && params.in_use);
    if (reset_prev) {
        prev_mix_volumes = mix_volumes;
        need_update = false;
    }

    in_use = params.in_use;
}

void SplitterDestinationData::Update(const InParameterVersion2& params,
                                     const bool is_prev_volume_reset_supported) {
    if (params.id != id || params.magic != GetSplitterSendDataMagic()) {
        return;
    }

    destination_id = params.mix_id;
    mix_volumes = params.mix_volumes;
    biquads = params.biquads;

    const bool reset_prev = is_prev_volume_reset_supported ? params.reset_prev_volume
                                                           : (!in_use && params.in_use);
    if (reset_prev) {
        prev_mix_volumes = mix_volumes;
        need_update = false;
    }

    in_use = params.in_use;
}

void SplitterDestinationData::Update(const InParameterVersion2b& params,
                                     const bool is_prev_volume_reset_supported) {
    if (params.id != id || params.magic != GetSplitterSendDataMagic()) {
        return;
    }

    destination_id = params.mix_id;
    mix_volumes = params.mix_volumes;
    for (size_t i = 0; i < biquads.size(); i++) {
        biquads[i] = ConvertBiquadFloatToFixed(params.biquads[i]);
    }

    const bool reset_prev = is_prev_volume_reset_supported ? params.reset_prev_volume
                                                           : (!in_use && params.in_use);
    if (reset_prev) {
        prev_mix_volumes = mix_volumes;
        need_update = false;
    }

    in_use = params.in_use;
}

const VoiceInfo::BiquadFilterParameter& SplitterDestinationData::GetBiquadFilterParameter(
    const u32 index) const {
    return biquads[index];
}

bool SplitterDestinationData::IsBiquadFilterEnabled() const {
    return biquads[0].enabled || biquads[1].enabled;
}

bool SplitterDestinationData::IsBiquadFilterEnabledPrev() const {
    return prev_biquad_enabled[0];
}

void SplitterDestinationData::UpdateBiquadFilterEnabledPrev(const u32 index) {
    prev_biquad_enabled[index] = biquads[index].enabled;
}

void SplitterDestinationData::MarkAsNeedToUpdateInternalState() {
    need_update = true;
}

void SplitterDestinationData::UpdateInternalState() {
    if (in_use && need_update) {
        prev_mix_volumes = mix_volumes;
    }
    need_update = false;
}

SplitterDestinationData* SplitterDestinationData::GetNext() const {
    return next;
}

void SplitterDestinationData::SetNext(SplitterDestinationData* next_) {
    next = next_;
}

} // namespace AudioCore::Renderer
