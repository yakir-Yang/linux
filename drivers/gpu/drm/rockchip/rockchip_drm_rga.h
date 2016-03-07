#ifndef __ROCKCHIP_DRM_RGA__
#define __ROCKCHIP_DRM_RGA__

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-attrs.h>
#include <linux/platform_device.h>

#include "rockchip_drm_drv.h"

struct fb_cmdlist {
	uint32_t	act_h;
	uint32_t	act_w;
	uint32_t	vir_w;
	uint32_t	vir_h;

	uint32_t	x_offset;
	uint32_t	y_offset;

	uint32_t	format;

	uint32_t	yrgb_addr;
	uint32_t	uv_addr;
	uint32_t	v_addr;
};

struct rockchip_rga_set_cmdlist {
	struct fb_cmdlist src;
	struct fb_cmdlist src1;
	struct fb_cmdlist dst;
	struct fb_cmdlist pat;

	uint32_t	rotate_mode;
	uint32_t	alpha_swp;
	uint32_t	fg_color;
	uint32_t	rop_code;
	uint32_t	rop_mode;
	uint32_t	rop_mask_addr;
	uint32_t	rop_mask_stride;
	uint32_t	alpha_rop_flag;
	uint32_t	scale_bicu_mode;
	uint32_t	src_trans_mode;

	uint32_t	alpha_mode_0;
	uint32_t	alpha_mode_1;

	uint32_t	fading_g_value;
	uint32_t	fading_r_value;
	uint32_t	fading_b_value;
	uint32_t	src_a_global_val;
	uint32_t	dst_a_global_val;

	uint32_t	dither_mode;
	uint32_t	yuv2rgb_mode;
	uint32_t	endian_mode;
	uint32_t	render_mode;
	uint32_t	bitblt_mode;
	uint32_t	color_fill_mode;
	uint32_t	alpha_zero_key;
	uint32_t	cmd_fin_int_enable;
};

#define RGA_CMDBUF_SIZE			14
#define RGA_CMDLIST_SIZE		0x20
#define RGA_CMDLIST_NUM			64


/* cmdlist data structure */
struct rga_cmdlist {
	u32		head;
	unsigned long	data[RGA_CMDLIST_SIZE*2];
	u32		last;	/* last data offset */
	void		*src_mmu_pages;
	void		*dst_mmu_pages;
	void		*src1_mmu_pages;
	struct dma_buf_attachment *src_attach;
	struct dma_buf_attachment *dst_attach;
};

struct rga_cmdlist_node {
	struct list_head	list;
	struct rga_cmdlist	cmdlist;
};

struct rga_runqueue_node {
	struct list_head	list;

	struct device		*dev;
	pid_t			pid;
	struct drm_file		*file;
	struct completion	complete;

	struct list_head	run_cmdlist;

	int			cmdlist_cnt;
	void			*cmdlist_pool_virt;
	dma_addr_t		cmdlist_pool;
	struct dma_attrs	cmdlist_dma_attrs;
};

struct rockchip_rga_version {
	__u32			major;
	__u32			minor;
};

struct rockchip_rga {
	struct drm_device	*drm_dev;
	struct device		*dev;
	struct regmap		*grf;
	void __iomem		*regs;
	struct clk		*sclk;
	struct clk		*aclk;
	struct clk		*hclk;

	bool				suspended;
	struct rockchip_rga_version	version;
	struct drm_rockchip_subdrv	subdrv;
	struct workqueue_struct		*rga_workq;
	struct work_struct		runqueue_work;

	/* rga command list pool */
	struct rga_cmdlist_node		cmdlist_node[RGA_CMDLIST_NUM];
	struct mutex			cmdlist_mutex;

	struct list_head		free_cmdlist;

	/* rga runqueue */
	struct rga_runqueue_node	*runqueue_node;
	struct list_head		runqueue_list;
	struct mutex			runqueue_mutex;
	struct kmem_cache		*runqueue_slab;
};

#ifdef CONFIG_ROCKCHIP_RGA
extern int rockchip_rga_get_ver_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);
extern int rockchip_rga_set_cmdlist_ioctl(struct drm_device *dev, void *data,
					  struct drm_file *file_priv);
extern int rockchip_rga_exec_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
#else
static inline int rockchip_rga_get_ver_ioctl(struct drm_device *dev, void *data,
					     struct drm_file *file_priv)
{
	return -ENODEV;
}

static inline int rockchip_rga_set_cmdlist_ioctl(struct drm_device *dev,
						 void *data,
						 struct drm_file *file_priv)
{
	return -ENODEV;
}

static inline int rockchip_rga_exec_ioctl(struct drm_device *dev, void *data,
					  struct drm_file *file_priv)
{
	return -ENODEV;
}
#endif

#endif /* __ROCKCHIP_DRM_RGA__ */
