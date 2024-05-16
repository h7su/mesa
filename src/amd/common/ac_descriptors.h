/*
 * Copyright 2015 Advanced Micro Devices, Inc.
 * Copyright 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef AC_DESCRIPTORS_H
#define AC_DESCRIPTORS_H

#include "ac_gpu_info.h"

#include "util/format/u_format.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned
ac_map_swizzle(unsigned swizzle);

struct ac_sampler_state {
   unsigned address_mode_u : 3;
   unsigned address_mode_v : 3;
   unsigned address_mode_w : 3;
   unsigned max_aniso_ratio : 3;
   unsigned depth_compare_func : 3;
   unsigned unnormalized_coords : 1;
   unsigned cube_wrap : 1;
   unsigned trunc_coord : 1;
   unsigned filter_mode : 2;
   unsigned mag_filter : 2;
   unsigned min_filter : 2;
   unsigned mip_filter : 2;
   unsigned aniso_single_level : 1;
   unsigned border_color_type : 2;
   unsigned border_color_ptr : 12;
   float min_lod;
   float max_lod;
   float lod_bias;
};

void
ac_build_sampler_descriptor(const enum amd_gfx_level gfx_level,
                            const struct ac_sampler_state *state,
                            uint32_t desc[4]);

struct ac_buffer_state {
   uint64_t va;
   uint32_t size;
   enum pipe_format format;
   enum pipe_swizzle swizzle[4];
   uint32_t stride : 14;
   uint32_t swizzle_enable : 2;
   uint32_t element_size : 2;
   uint32_t index_stride : 2;
   uint32_t add_tid : 1;
   uint32_t gfx10_oob_select : 2;
};

void
ac_build_buffer_descriptor(const enum amd_gfx_level gfx_level,
                           const struct ac_buffer_state *state,
                           uint32_t desc[4]);

void
ac_build_raw_buffer_descriptor(const enum amd_gfx_level gfx_level,
                               uint64_t va,
                               uint32_t size,
                               uint32_t desc[4]);

void
ac_build_attr_ring_descriptor(const enum amd_gfx_level gfx_level,
                              uint64_t va,
                              uint32_t size,
                              uint32_t desc[4]);

#ifdef __cplusplus
}
#endif

#endif
