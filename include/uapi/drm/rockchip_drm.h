/*
 *
 * Copyright (c) Fuzhou Rockchip Electronics Co.Ltd
 * Authors:
 *       Mark Yao <yzq@rock-chips.com>
 *
 * base on exynos_drm.h
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _UAPI_ROCKCHIP_DRM_H
#define _UAPI_ROCKCHIP_DRM_H

#include <drm/drm.h>

/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *     - this handle will be set by gem module of kernel side.
 */
struct drm_rockchip_gem_create {
	uint64_t size;
	uint32_t flags;
	uint32_t handle;
};

/**
 * A structure for getting buffer offset.
 *
 * @handle: a pointer to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @offset: relatived offset value of the memory region allocated.
 *     - this value should be set by user.
 */
struct drm_rockchip_gem_map_off {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

/**
 * A structure for mapping buffer.
 *
 * @handle: a handle to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @size: memory size to be mapped.
 * @mapped: having user virtual address mmaped.
 *	- this variable would be filled by rockchip gem module
 *	of kernel side with user virtual address which is allocated
 *	by do_mmap().
 */
struct drm_rockchip_gem_mmap {
	unsigned int handle;
	unsigned int pad;
	uint64_t size;
	uint64_t mapped;
};

/**
 * A structure to gem information.
 *
 * @handle: a handle to gem object created.
 * @flags: flag value including memory type and cache attribute and
 *	this value would be set by driver.
 * @size: size to memory region allocated by gem and this size would
 *	be set by driver.
 */
struct drm_rockchip_gem_info {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

/* acquire type definitions. */
enum drm_rockchip_gem_cpu_acquire_type {
	DRM_ROCKCHIP_GEM_CPU_ACQUIRE_SHARED = 0x0,
	DRM_ROCKCHIP_GEM_CPU_ACQUIRE_EXCLUSIVE = 0x1,
};

/**
 * A structure for acquiring buffer for CPU access.
 *
 * @handle: a handle to gem object created.
 * @flags: acquire flag
 */
struct drm_rockchip_gem_cpu_acquire {
	uint32_t handle;
	uint32_t flags;
};

/*
 * A structure for releasing buffer for GPU access.
 *
 * @handle: a handle to gem object created.
 */
struct drm_rockchip_gem_cpu_release {
	uint32_t handle;
};

/* memory type definitions. */
enum e_drm_rockchip_gem_mem_type {
	/* Physically Continuous memory and used as default. */
	ROCKCHIP_BO_CONTIG	= 0 << 0,
	/* Physically Non-Continuous memory. */
	ROCKCHIP_BO_NONCONTIG	= 1 << 0,
	/* non-cachable mapping and used as default. */
	ROCKCHIP_BO_NONCACHABLE	= 0 << 1,
	/* cachable mapping. */
	ROCKCHIP_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	ROCKCHIP_BO_WC		= 1 << 2,
	ROCKCHIP_BO_MASK	= ROCKCHIP_BO_NONCONTIG | ROCKCHIP_BO_CACHABLE |
				  ROCKCHIP_BO_WC
};

struct drm_rockchip_rga_get_ver {
	__u32   major;
	__u32   minor;
};

struct drm_rockchip_rga_cmd {
	__u32   offset;
	__u32   data;
};

enum drm_rockchip_rga_buf_type {
	RGA_BUF_TYPE_USERPTR = 1 << 31,
	RGA_BUF_TYPE_GEMFD   = 1 << 30,
};

struct drm_rockchip_rga_set_cmdlist {
	__u64		cmd;
	__u64		cmd_buf;
	__u32		cmd_nr;
	__u32		cmd_buf_nr;
	__u64		user_data;
};

struct drm_rockchip_rga_exec {
	__u64		async;
};

/**
 * A structure for set/get VOP win color key
 *
 * @plane_id: the drm id of target plane
 * @enabled: set/get the color key status, 1 means enabled, 0 means disabled.
 * @colorkey: the key color value, for win0/1 the color key is RGB 10bit, and
 *	      for win2/3 the color key is RGB 8bit.
 */
struct drm_rockchip_plane_colorkey {
	__u32 plane_id;
	__u32 enabled;
	__u32 colorkey;
};

#define DRM_ROCKCHIP_GEM_CREATE		0x00
#define DRM_ROCKCHIP_GEM_MAP_OFFSET	0x01
#define DRM_ROCKCHIP_GEM_CPU_ACQUIRE	0x02
#define DRM_ROCKCHIP_GEM_CPU_RELEASE	0x03
#define DRM_ROCKCHIP_GEM_MMAP		0x04
#define DRM_ROCKCHIP_GEM_GET		0x05

#define DRM_ROCKCHIP_VOP_GET_PLANE_COLORKEY	0x10
#define DRM_ROCKCHIP_VOP_SET_PLANE_COLORKEY	0x11

#define DRM_ROCKCHIP_RGA_GET_VER		0x20
#define DRM_ROCKCHIP_RGA_SET_CMDLIST		0x21
#define DRM_ROCKCHIP_RGA_EXEC			0x22

#define DRM_IOCTL_ROCKCHIP_RGA_GET_VER		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_RGA_GET_VER, struct drm_rockchip_rga_get_ver)

#define DRM_IOCTL_ROCKCHIP_RGA_SET_CMDLIST	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_RGA_SET_CMDLIST, struct drm_rockchip_rga_set_cmdlist)

#define DRM_IOCTL_ROCKCHIP_RGA_EXEC		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_RGA_EXEC, struct drm_rockchip_rga_exec)

#define DRM_IOCTL_ROCKCHIP_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_CREATE, struct drm_rockchip_gem_create)

#define DRM_IOCTL_ROCKCHIP_GEM_MAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_MAP_OFFSET, struct drm_rockchip_gem_map_off)

#define DRM_IOCTL_ROCKCHIP_GEM_MMAP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_MMAP, struct drm_rockchip_gem_mmap)

#define DRM_IOCTL_ROCKCHIP_GEM_GET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_GET,	struct drm_rockchip_gem_info)

#define DRM_IOCTL_ROCKCHIP_GEM_CPU_ACQUIRE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_CPU_ACQUIRE, struct drm_rockchip_gem_cpu_acquire)

#define DRM_IOCTL_ROCKCHIP_GEM_CPU_RELEASE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_CPU_RELEASE, struct drm_rockchip_gem_cpu_release)

#define DRM_IOCTL_ROCKCHIP_VOP_GET_PLANE_COLORKEY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_VOP_GET_PLANE_COLORKEY, struct drm_rockchip_plane_colorkey)

#define DRM_IOCTL_ROCKCHIP_VOP_SET_PLANE_COLORKEY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_VOP_SET_PLANE_COLORKEY, struct drm_rockchip_plane_colorkey)

#endif /* _UAPI_ROCKCHIP_DRM_H */
