/* rockchip_drm.h
 *
 * Copyright (c) 2016 Fuzhou Rockchip Electronics Co., Ltd.
 * Authors:
 *	Yakir Yang <ykk@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _UAPI_ROCKCHIP_DRM_H_
#define _UAPI_ROCKCHIP_DRM_H_

#include <drm/drm.h>

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

struct drm_rockchip_rga_userptr {
	unsigned long userptr;
	unsigned long size;
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

#define DRM_ROCKCHIP_RGA_GET_VER		0x20
#define DRM_ROCKCHIP_RGA_SET_CMDLIST		0x21
#define DRM_ROCKCHIP_RGA_EXEC			0x22

#define DRM_IOCTL_ROCKCHIP_RGA_GET_VER		DRM_IOWR(DRM_COMMAND_BASE + \
	DRM_ROCKCHIP_RGA_GET_VER, struct drm_rockchip_rga_get_ver)

#define DRM_IOCTL_ROCKCHIP_RGA_SET_CMDLIST	DRM_IOWR(DRM_COMMAND_BASE + \
	DRM_ROCKCHIP_RGA_SET_CMDLIST, struct drm_rockchip_rga_set_cmdlist)

#define DRM_IOCTL_ROCKCHIP_RGA_EXEC		DRM_IOWR(DRM_COMMAND_BASE + \
	DRM_ROCKCHIP_RGA_EXEC, struct drm_rockchip_rga_exec)

#endif /* _UAPI_ROCKCHIP_DRM_H */
