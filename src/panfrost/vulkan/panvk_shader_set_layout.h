/*
 * Copyright © 2024 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_SHADER_LAYOUT_SET_H
#define PANVK_SHADER_LAYOUT_SET_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include <vulkan/vulkan_core.h>
#include "vk_descriptor_set_layout.h"

#include "util/mesa-sha1.h"

#include "panvk_macros.h"

#define MAX_SETS 4

struct panvk_shader_set_info {
   unsigned sampler_offset;
   unsigned tex_offset;
   unsigned ubo_offset;
   unsigned dyn_ubo_offset;
   unsigned dyn_ssbo_offset;
   unsigned img_offset;
   unsigned dyn_desc_ubo_offset;

   unsigned num_ubos;
   unsigned num_dyn_ubos;
};

struct panvk_shader_set_layout {
   unsigned set_count;

   unsigned num_samplers;
   unsigned num_textures;
   unsigned num_ubos;
   unsigned num_dyn_ubos;
   unsigned num_dyn_ssbos;
   uint32_t num_imgs;

   struct {
      uint32_t size;
   } push_constants;

   struct panvk_shader_set_info sets[MAX_SETS];
};

void panvk_per_arch(shader_set_layout_fill)(
   struct panvk_shader_set_layout *layout, unsigned set_layout_count,
   struct vk_descriptor_set_layout *const *set_layouts,
   unsigned push_constant_range_count,
   const VkPushConstantRange *push_constant_ranges);

void panvk_per_arch(shader_set_layout_hash_state)(
   const struct panvk_shader_set_layout *layout,
   struct vk_descriptor_set_layout *const *set_layouts,
   struct mesa_sha1 *sha1_ctx);

unsigned panvk_per_arch(shader_set_layout_ubo_start)(
   const struct panvk_shader_set_layout *layout, unsigned set, bool is_dynamic);

unsigned panvk_per_arch(shader_set_layout_ubo_index)(
   const struct panvk_shader_set_layout *layout,
   struct vk_descriptor_set_layout *const *set_layouts, unsigned set,
   unsigned binding, unsigned array_index);

unsigned panvk_per_arch(shader_set_layout_dyn_desc_ubo_index)(
   const struct panvk_shader_set_layout *layout);

unsigned panvk_per_arch(shader_set_layout_dyn_ubos_offset)(
   const struct panvk_shader_set_layout *layout);

unsigned panvk_per_arch(shader_set_layout_total_ubo_count)(
   const struct panvk_shader_set_layout *layout);

#endif
