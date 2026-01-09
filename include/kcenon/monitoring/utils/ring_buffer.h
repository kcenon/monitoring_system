#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file ring_buffer.h
 * @brief Public API for ring buffer metric storage
 *
 * This file exposes the ring buffer implementation as a public API
 * for efficient metric storage with configurable capacity and overflow behavior.
 */

#include "../../../../src/utils/ring_buffer.h"

// Re-export all ring_buffer types from internal implementation
// The internal implementation provides:
// - ring_buffer_config
// - ring_buffer_stats
// - ring_buffer<T>
// - make_ring_buffer<T>()
