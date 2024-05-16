/*
 * Copyright 2015 Advanced Micro Devices, Inc.
 * Copyright 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "ac_descriptors.h"
#include "ac_formats.h"

#include "gfx10_format_table.h"
#include "sid.h"

#include "util/u_math.h"
#include "util/format/u_format.h"

unsigned
ac_map_swizzle(unsigned swizzle)
{
   switch (swizzle) {
   case PIPE_SWIZZLE_Y:
      return V_008F0C_SQ_SEL_Y;
   case PIPE_SWIZZLE_Z:
      return V_008F0C_SQ_SEL_Z;
   case PIPE_SWIZZLE_W:
      return V_008F0C_SQ_SEL_W;
   case PIPE_SWIZZLE_0:
      return V_008F0C_SQ_SEL_0;
   case PIPE_SWIZZLE_1:
      return V_008F0C_SQ_SEL_1;
   default: /* PIPE_SWIZZLE_X */
      return V_008F0C_SQ_SEL_X;
   }
}

void
ac_build_sampler_descriptor(const enum amd_gfx_level gfx_level, const struct ac_sampler_state *state, uint32_t desc[4])
{
   const unsigned perf_mip = state->max_aniso_ratio ? state->max_aniso_ratio + 6 : 0;
   const bool compat_mode = gfx_level == GFX8 || gfx_level == GFX9;

   desc[0] = S_008F30_CLAMP_X(state->address_mode_u) |
             S_008F30_CLAMP_Y(state->address_mode_v) |
             S_008F30_CLAMP_Z(state->address_mode_w) |
             S_008F30_MAX_ANISO_RATIO(state->max_aniso_ratio) |
             S_008F30_DEPTH_COMPARE_FUNC(state->depth_compare_func) |
             S_008F30_FORCE_UNNORMALIZED(state->unnormalized_coords) |
             S_008F30_ANISO_THRESHOLD(state->max_aniso_ratio >> 1) |
             S_008F30_ANISO_BIAS(state->max_aniso_ratio) |
             S_008F30_DISABLE_CUBE_WRAP(!state->cube_wrap) |
             S_008F30_COMPAT_MODE(compat_mode) |
             S_008F30_TRUNC_COORD(state->trunc_coord) |
             S_008F30_FILTER_MODE(state->filter_mode);
   desc[1] = 0;
   desc[2] = S_008F38_XY_MAG_FILTER(state->mag_filter) |
             S_008F38_XY_MIN_FILTER(state->min_filter) |
             S_008F38_MIP_FILTER(state->mip_filter);
   desc[3] = S_008F3C_BORDER_COLOR_TYPE(state->border_color_type);

   if (gfx_level >= GFX12) {
      desc[1] |= S_008F34_MIN_LOD_GFX12(util_unsigned_fixed(CLAMP(state->min_lod, 0, 17), 8)) |
                 S_008F34_MAX_LOD_GFX12(util_unsigned_fixed(CLAMP(state->max_lod, 0, 17), 8));
      desc[2] |= S_008F38_PERF_MIP_LO(perf_mip);
      desc[3] |= S_008F3C_PERF_MIP_HI(perf_mip >> 2);
   } else {
      desc[1] |= S_008F34_MIN_LOD_GFX6(util_unsigned_fixed(CLAMP(state->min_lod, 0, 15), 8)) |
                 S_008F34_MAX_LOD_GFX6(util_unsigned_fixed(CLAMP(state->max_lod, 0, 15), 8)) |
                 S_008F34_PERF_MIP(perf_mip);
   }

   if (gfx_level >= GFX10) {
      desc[2] |= S_008F38_LOD_BIAS(util_signed_fixed(CLAMP(state->lod_bias, -32, 31), 8)) |
                 S_008F38_ANISO_OVERRIDE_GFX10(!state->aniso_single_level);
   } else {
      desc[2] |= S_008F38_LOD_BIAS(util_signed_fixed(CLAMP(state->lod_bias, -16, 16), 8)) |
                 S_008F38_DISABLE_LSB_CEIL(gfx_level <= GFX8) |
                 S_008F38_FILTER_PREC_FIX(1) |
                 S_008F38_ANISO_OVERRIDE_GFX8(gfx_level >= GFX8 && !state->aniso_single_level);
   }

   if (gfx_level >= GFX11) {
      desc[3] |= S_008F3C_BORDER_COLOR_PTR_GFX11(state->border_color_ptr);
   } else {
      desc[3] |= S_008F3C_BORDER_COLOR_PTR_GFX6(state->border_color_ptr);
   }
}

void
ac_build_buffer_descriptor(const enum amd_gfx_level gfx_level, const struct ac_buffer_state *state, uint32_t desc[4])
{
   uint32_t rsrc_word1 = S_008F04_BASE_ADDRESS_HI(state->va >> 32) | S_008F04_STRIDE(state->stride);

   if (gfx_level >= GFX11) {
      rsrc_word1 |= S_008F04_SWIZZLE_ENABLE_GFX11(state->swizzle_enable);
   } else {
      rsrc_word1 |= S_008F04_SWIZZLE_ENABLE_GFX6(state->swizzle_enable);
   }

   uint32_t rsrc_word3 = S_008F0C_DST_SEL_X(ac_map_swizzle(state->swizzle[0])) |
                         S_008F0C_DST_SEL_Y(ac_map_swizzle(state->swizzle[1])) |
                         S_008F0C_DST_SEL_Z(ac_map_swizzle(state->swizzle[2])) |
                         S_008F0C_DST_SEL_W(ac_map_swizzle(state->swizzle[3])) |
                         S_008F0C_INDEX_STRIDE(state->index_stride) |
                         S_008F0C_ADD_TID_ENABLE(state->add_tid);

   if (gfx_level >= GFX10) {
      const struct gfx10_format *fmt = &ac_get_gfx10_format_table(gfx_level)[state->format];

      /* OOB_SELECT chooses the out-of-bounds check.
       *
       * GFX10:
       *  - 0: (index >= NUM_RECORDS) || (offset >= STRIDE)
       *  - 1: index >= NUM_RECORDS
       *  - 2: NUM_RECORDS == 0
       *  - 3: if SWIZZLE_ENABLE:
       *          swizzle_address >= NUM_RECORDS
       *       else:
       *          offset >= NUM_RECORDS
       *
       * GFX11+:
       *  - 0: (index >= NUM_RECORDS) || (offset+payload > STRIDE)
       *  - 1: index >= NUM_RECORDS
       *  - 2: NUM_RECORDS == 0
       *  - 3: if SWIZZLE_ENABLE && STRIDE:
       *          (index >= NUM_RECORDS) || ( offset+payload > STRIDE)
       *       else:
       *          offset+payload > NUM_RECORDS
       */
      rsrc_word3 |= gfx_level >= GFX12 ? S_008F0C_FORMAT_GFX12(fmt->img_format) :
                                         S_008F0C_FORMAT_GFX10(fmt->img_format) |
                    S_008F0C_OOB_SELECT(state->gfx10_oob_select) |
                    S_008F0C_RESOURCE_LEVEL(gfx_level < GFX11);
   } else {
      const struct util_format_description * desc =  util_format_description(state->format);
      const int first_non_void = util_format_get_first_non_void_channel(state->format);
      const uint32_t num_format = ac_translate_buffer_numformat(desc, first_non_void);

      /* DATA_FORMAT is STRIDE[14:17] for MUBUF with ADD_TID_ENABLE=1 */
      const uint32_t data_format =
         gfx_level >= GFX8 && state->add_tid ? 0 : ac_translate_buffer_dataformat(desc, first_non_void);

      assert(gfx_level <= GFX8 || !state->element_size);

      rsrc_word3 |= S_008F0C_NUM_FORMAT(num_format) |
                    S_008F0C_DATA_FORMAT(data_format) |
                    S_008F0C_ELEMENT_SIZE(state->element_size);
   }

   desc[0] = state->va;
   desc[1] = rsrc_word1;
   desc[2] = state->size;
   desc[3] = rsrc_word3;
}

void
ac_build_raw_buffer_descriptor(const enum amd_gfx_level gfx_level, uint64_t va, uint32_t size, uint32_t desc[4])
{
   const struct ac_buffer_state ac_state = {
      .va = va,
      .size = size,
      .format = PIPE_FORMAT_R32_FLOAT,
      .swizzle = {
         PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W,
      },
      .gfx10_oob_select = V_008F0C_OOB_SELECT_RAW,
   };

   ac_build_buffer_descriptor(gfx_level, &ac_state, desc);
}

void
ac_build_attr_ring_descriptor(const enum amd_gfx_level gfx_level, uint64_t va, uint32_t size, uint32_t desc[4])
{
   assert(gfx_level >= GFX11);

   const struct ac_buffer_state ac_state = {
      .va = va,
      .size = size,
      .format = PIPE_FORMAT_R32G32B32A32_FLOAT,
      .swizzle = {
         PIPE_SWIZZLE_X, PIPE_SWIZZLE_Y, PIPE_SWIZZLE_Z, PIPE_SWIZZLE_W,
      },
      .gfx10_oob_select = V_008F0C_OOB_SELECT_STRUCTURED_WITH_OFFSET,
      .swizzle_enable = 3, /* 16B */
      .index_stride = 2, /* 32 elements */
   };

   ac_build_buffer_descriptor(gfx_level, &ac_state, desc);
}
