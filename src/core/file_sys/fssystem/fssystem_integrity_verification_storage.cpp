// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/alignment.h"
#include "common/logging/log.h"
#include "core/file_sys/fssystem/fssystem_integrity_verification_storage.h"

namespace FileSys {

constexpr inline u32 ILog2(u32 val) {
    ASSERT(val > 0);
    return static_cast<u32>((sizeof(u32) * 8) - 1 - std::countl_zero<u32>(val));
}

void IntegrityVerificationStorage::Initialize(VirtualFile hs, VirtualFile ds, s64 verif_block_size,
                                              s64 upper_layer_verif_block_size, bool is_real_data) {
    // Validate preconditions.
    ASSERT(verif_block_size >= HashSize);

    // Defensive null guard: this is called during NSP/NCA parsing, and a malformed or
    // unsupported layered-hash layout can produce nullptr storages here. Crashing the
    // host is worse than failing this NCA gracefully — leave the storages unset so the
    // outer NCA load fails cleanly via subsequent IsNotNull() / size checks.
    if (hs == nullptr || ds == nullptr) {
        LOG_ERROR(Service_FS,
                  "IntegrityVerificationStorage::Initialize received null storage "
                  "(hs={}, ds={}); skipping",
                  static_cast<bool>(hs), static_cast<bool>(ds));
        return;
    }

    // Set storages.
    m_hash_storage = hs;
    m_data_storage = ds;

    // Set verification block sizes.
    m_verification_block_size = verif_block_size;
    m_verification_block_order = ILog2(static_cast<u32>(verif_block_size));
    ASSERT(m_verification_block_size == 1ll << m_verification_block_order);

    // Set upper layer block sizes.
    upper_layer_verif_block_size = std::max(upper_layer_verif_block_size, HashSize);
    m_upper_layer_verification_block_size = upper_layer_verif_block_size;
    m_upper_layer_verification_block_order = ILog2(static_cast<u32>(upper_layer_verif_block_size));
    ASSERT(m_upper_layer_verification_block_size == 1ll << m_upper_layer_verification_block_order);

    // Validate sizes.
    {
        s64 hash_size = m_hash_storage->GetSize();
        s64 data_size = m_data_storage->GetSize();
        ASSERT(((hash_size / HashSize) * m_verification_block_size) >= data_size);
    }

    // Set data.
    m_is_real_data = is_real_data;
}

void IntegrityVerificationStorage::Finalize() {
    m_hash_storage = VirtualFile();
    m_data_storage = VirtualFile();
}

size_t IntegrityVerificationStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Succeed if zero size.
    if (size == 0) {
        return size;
    }

    // Validate arguments.
    ASSERT(buffer != nullptr);

    // Defensive: Initialize() bails on null storages without setting m_data_storage, so
    // Read on a never-initialized object would dereference null. Treat as a 0-byte read
    // so the outer NCA loader fails its size/header checks gracefully instead of taking
    // down the host.
    if (m_data_storage == nullptr) {
        return 0;
    }

    // Validate the offset.
    s64 data_size = m_data_storage->GetSize();
    ASSERT(offset <= static_cast<size_t>(data_size));

    // Validate the access range.
    ASSERT(R_SUCCEEDED(IStorage::CheckAccessRange(
        offset, size, Common::AlignUp(data_size, static_cast<size_t>(m_verification_block_size)))));

    // Determine the read extents.
    size_t read_size = size;
    if (static_cast<s64>(offset + read_size) > data_size) {
        // Determine the padding sizes.
        s64 padding_offset = data_size - offset;
        size_t padding_size = static_cast<size_t>(
            m_verification_block_size - (padding_offset & (m_verification_block_size - 1)));
        ASSERT(static_cast<s64>(padding_size) < m_verification_block_size);

        // Clear the padding.
        std::memset(static_cast<u8*>(buffer) + padding_offset, 0, padding_size);

        // Set the new in-bounds size.
        read_size = static_cast<size_t>(data_size - offset);
    }

    // Perform the read.
    return m_data_storage->Read(buffer, read_size, offset);
}

size_t IntegrityVerificationStorage::GetSize() const {
    if (m_data_storage == nullptr) {
        return 0;
    }
    return m_data_storage->GetSize();
}

} // namespace FileSys
