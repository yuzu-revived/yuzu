// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include <zlib.h>

#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

const std::array<const char*, 16> LANGUAGE_NAMES{{
    "AmericanEnglish",
    "BritishEnglish",
    "Japanese",
    "French",
    "German",
    "LatinAmericanSpanish",
    "Spanish",
    "Italian",
    "Dutch",
    "CanadianFrench",
    "Portuguese",
    "Russian",
    "Korean",
    "TraditionalChinese",
    "SimplifiedChinese",
    "BrazilianPortuguese",
}};

std::string LanguageEntry::GetApplicationName() const {
    return Common::StringFromFixedZeroTerminatedBuffer(application_name.data(),
                                                       application_name.size());
}

std::string LanguageEntry::GetDeveloperName() const {
    return Common::StringFromFixedZeroTerminatedBuffer(developer_name.data(),
                                                       developer_name.size());
}

constexpr std::array<Language, 18> language_to_codes = {{
    Language::Japanese,
    Language::AmericanEnglish,
    Language::French,
    Language::German,
    Language::Italian,
    Language::Spanish,
    Language::SimplifiedChinese,
    Language::Korean,
    Language::Dutch,
    Language::Portuguese,
    Language::Russian,
    Language::TraditionalChinese,
    Language::BritishEnglish,
    Language::CanadianFrench,
    Language::LatinAmericanSpanish,
    Language::SimplifiedChinese,
    Language::TraditionalChinese,
    Language::BrazilianPortuguese,
}};

NACP::NACP() = default;

namespace {
// Firmware 21+ marks the first 0x3000 bytes of the NACP as a zlib-compressed title block.
// The flag is a u8 at offset 0x3215 (titles_data_format / NacpTitleCompression). When set, the
// block is {u16 compressed_size; u8 compressed_blob[0x2FFE]} and decompresses with raw deflate
// (wbits = -15) to a 32-entry LanguageEntry array (0x6000 bytes). See nxdumptool f3f19e8.
constexpr size_t TITLE_COMPRESSION_FLAG_OFFSET = 0x3215;
constexpr size_t COMPRESSED_TITLE_LANGUAGE_COUNT = 32;
constexpr int RAW_DEFLATE_WBITS = -15;

bool DecompressTitleBlock(const RawNACP& raw, std::vector<LanguageEntry>& out) {
    const auto* raw_bytes = reinterpret_cast<const u8*>(&raw);
    u16 compressed_size{};
    std::memcpy(&compressed_size, raw_bytes, sizeof(compressed_size));
    if (compressed_size == 0 || compressed_size > 0x2FFE) {
        LOG_WARNING(Loader, "NACP marked compressed but blob size {:#x} is out of range",
                    compressed_size);
        return false;
    }

    out.assign(COMPRESSED_TITLE_LANGUAGE_COUNT, LanguageEntry{});
    constexpr uLong decompressed_size =
        static_cast<uLong>(sizeof(LanguageEntry) * COMPRESSED_TITLE_LANGUAGE_COUNT);

    z_stream zstrm{};
    if (inflateInit2(&zstrm, RAW_DEFLATE_WBITS) != Z_OK) {
        out.clear();
        return false;
    }
    zstrm.next_in = const_cast<Bytef*>(raw_bytes + sizeof(compressed_size));
    zstrm.avail_in = compressed_size;
    zstrm.next_out = reinterpret_cast<Bytef*>(out.data());
    zstrm.avail_out = decompressed_size;

    const int rc = inflate(&zstrm, Z_FINISH);
    const uLong produced = zstrm.total_out;
    inflateEnd(&zstrm);

    if (rc != Z_STREAM_END || produced != decompressed_size) {
        LOG_WARNING(Loader, "NACP title block inflate failed: rc={} produced={}/{}", rc, produced,
                    decompressed_size);
        out.clear();
        return false;
    }
    return true;
}
} // namespace

NACP::NACP(VirtualFile file) {
    file->ReadObject(&raw);

    const auto* raw_bytes = reinterpret_cast<const u8*>(&raw);
    if (raw_bytes[TITLE_COMPRESSION_FLAG_OFFSET] != 0) {
        DecompressTitleBlock(raw, decompressed_language_entries);
    }
}

NACP::~NACP() = default;

const LanguageEntry& NACP::GetLanguageEntry() const {
    const bool use_decompressed = !decompressed_language_entries.empty();
    const auto* entries = use_decompressed ? decompressed_language_entries.data()
                                           : raw.language_entries.data();
    const size_t entry_count =
        use_decompressed ? decompressed_language_entries.size() : raw.language_entries.size();

    const Language language =
        language_to_codes[static_cast<s32>(Settings::values.language_index.GetValue())];
    const auto preferred_index = static_cast<size_t>(language);

    if (preferred_index < entry_count && !entries[preferred_index].GetApplicationName().empty()) {
        return entries[preferred_index];
    }

    for (size_t i = 0; i < entry_count; ++i) {
        if (!entries[i].GetApplicationName().empty()) {
            return entries[i];
        }
    }

    return entries[static_cast<size_t>(Language::AmericanEnglish)];
}

std::string NACP::GetApplicationName() const {
    return GetLanguageEntry().GetApplicationName();
}

std::string NACP::GetDeveloperName() const {
    return GetLanguageEntry().GetDeveloperName();
}

u64 NACP::GetTitleId() const {
    return raw.save_data_owner_id;
}

u64 NACP::GetDLCBaseTitleId() const {
    return raw.dlc_base_title_id;
}

std::string NACP::GetVersionString() const {
    return Common::StringFromFixedZeroTerminatedBuffer(raw.version_string.data(),
                                                       raw.version_string.size());
}

u64 NACP::GetDefaultNormalSaveSize() const {
    return raw.user_account_save_data_size;
}

u64 NACP::GetDefaultJournalSaveSize() const {
    return raw.user_account_save_data_journal_size;
}

bool NACP::GetUserAccountSwitchLock() const {
    return raw.user_account_switch_lock != 0;
}

u32 NACP::GetSupportedLanguages() const {
    return raw.supported_languages;
}

u64 NACP::GetDeviceSaveDataSize() const {
    return raw.device_save_data_size;
}

u32 NACP::GetParentalControlFlag() const {
    return raw.parental_control;
}

const std::array<u8, 0x20>& NACP::GetRatingAge() const {
    return raw.rating_age;
}

std::vector<u8> NACP::GetRawBytes() const {
    std::vector<u8> out(sizeof(RawNACP));
    std::memcpy(out.data(), &raw, sizeof(RawNACP));
    return out;
}
} // namespace FileSys
