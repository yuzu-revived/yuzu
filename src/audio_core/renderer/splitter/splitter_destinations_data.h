// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>

#include "audio_core/common/common.h"
#include "audio_core/renderer/voice/voice_info.h"
#include "common/common_types.h"

namespace AudioCore::Renderer {
/**
 * Represents a mixing node, can be connected to a previous and next destination forming a chain
 * that a certain mix buffer will pass through to output.
 */
class SplitterDestinationData {
public:
    struct InParameter {
        /* 0x00 */ u32 magic; // 'SNDD'
        /* 0x04 */ s32 id;
        /* 0x08 */ std::array<f32, MaxMixBuffers> mix_volumes;
        /* 0x68 */ u32 mix_id;
        /* 0x6C */ bool in_use;
        /* 0x6D */ bool reset_prev_volume; // REV13+; previously part of trailing padding
        /* 0x6E */ std::array<u8, 2> reserved;
    };
    static_assert(sizeof(InParameter) == 0x70,
                  "SplitterDestinationData::InParameter has the wrong size!");

    /**
     * REV12 splitter destination input header. Adds two biquad filter parameters that are
     * applied to the destination's mix buffer before the splitter mixes it into the
     * destination mix.
     */
    struct InParameterVersion2 {
        /* 0x00 */ u32 magic; // 'SNDD'
        /* 0x04 */ s32 id;
        /* 0x08 */ std::array<f32, MaxMixBuffers> mix_volumes;
        /* 0x68 */ u32 mix_id;
        /* 0x6C */ std::array<VoiceInfo::BiquadFilterParameter, MaxBiquadFilters> biquads;
        /* 0x84 */ bool in_use;
        /* 0x85 */ bool reset_prev_volume; // REV13+; previously part of reserved padding
        /* 0x86 */ std::array<u8, 10> reserved;
    };
    static_assert(sizeof(InParameterVersion2) == 0x90,
                  "SplitterDestinationData::InParameterVersion2 has the wrong size!");

    SplitterDestinationData(s32 id);

    /**
     * Reset the mix volumes for this destination.
     */
    void ClearMixVolume();

    /**
     * Get the id of this destination.
     *
     * @return Id for this destination.
     */
    s32 GetId() const;

    /**
     * Check if this destination is correctly configured.
     *
     * @return True if configured, otherwise false.
     */
    bool IsConfigured() const;

    /**
     * Get the mix id for this destination.
     *
     * @return Mix id for this destination.
     */
    s32 GetMixId() const;

    /**
     * Get the current mix volume of a given index in this destination.
     *
     * @param index - Mix buffer index to get the volume for.
     * @return Current volume of the specified mix.
     */
    f32 GetMixVolume(u32 index) const;

    /**
     * Get the current mix volumes for all mix buffers in this destination.
     *
     * @return Span of current mix buffer volumes.
     */
    std::span<f32> GetMixVolume();

    /**
     * Get the previous mix volume of a given index in this destination.
     *
     * @param index - Mix buffer index to get the volume for.
     * @return Previous volume of the specified mix.
     */
    f32 GetMixVolumePrev(u32 index) const;

    /**
     * Get the previous mix volumes for all mix buffers in this destination.
     *
     * @return Span of previous mix buffer volumes.
     */
    std::span<f32> GetMixVolumePrev();

    /**
     * Update this destination.
     *
     * @param params                       - Input parameters to update the destination.
     * @param is_prev_volume_reset_supported - When true (REV13+), use
     *                                         params.reset_prev_volume to decide whether
     *                                         the previous mix volume is overwritten with
     *                                         the current. When false, fall back to the
     *                                         legacy implicit reset on first in-use.
     */
    void Update(const InParameter& params, bool is_prev_volume_reset_supported);

    /**
     * Update this destination from REV12 (with splitter biquad filter) parameters.
     *
     * @param params                       - Version 2 input parameters.
     * @param is_prev_volume_reset_supported - See Update(InParameter,...).
     */
    void Update(const InParameterVersion2& params, bool is_prev_volume_reset_supported);

    /**
     * Get the biquad filter parameter for the given index (0..MaxBiquadFilters-1).
     */
    const VoiceInfo::BiquadFilterParameter& GetBiquadFilterParameter(u32 index) const;

    /**
     * Returns true if any biquad filter on this destination is enabled.
     */
    bool IsBiquadFilterEnabled() const;

    /**
     * Returns true if any biquad filter was previously enabled (used to decide whether
     * the splitter biquad state needs initialisation).
     */
    bool IsBiquadFilterEnabledPrev() const;

    /**
     * Latch the current biquad-enabled state into prev_biquad_enabled[index] so the
     * "needs init" check works on subsequent invocations.
     */
    void UpdateBiquadFilterEnabledPrev(u32 index);

    /**
     * Mark this destination as needing its volumes updated.
     */
    void MarkAsNeedToUpdateInternalState();

    /**
     * Copy current volumes to previous if an update is required.
     */
    void UpdateInternalState();

    /**
     * Get the next destination in the mix chain.
     *
     * @return The next splitter destination, may be nullptr if this is the last in the chain.
     */
    SplitterDestinationData* GetNext() const;

    /**
     * Set the next destination in the mix chain.
     *
     * @param next - Destination this one is to be connected to.
     */
    void SetNext(SplitterDestinationData* next);

private:
    /// Id of this destination
    const s32 id;
    /// Mix id this destination represents
    s32 destination_id{UnusedMixId};
    /// Current mix volumes
    std::array<f32, MaxMixBuffers> mix_volumes{0.0f};
    /// Previous mix volumes
    std::array<f32, MaxMixBuffers> prev_mix_volumes{0.0f};
    /// Next destination in the mix chain
    SplitterDestinationData* next{};
    /// Is this destination in use?
    bool in_use{};
    /// Does this destination need its volumes updated?
    bool need_update{};
    /// REV12 splitter biquad filter parameters (entries with .enabled == false are inactive).
    std::array<VoiceInfo::BiquadFilterParameter, MaxBiquadFilters> biquads{};
    /// Latched .enabled state from the previous render iteration, used to detect whether the
    /// per-filter biquad state needs re-initialisation.
    std::array<bool, MaxBiquadFilters> prev_biquad_enabled{};
};

} // namespace AudioCore::Renderer
