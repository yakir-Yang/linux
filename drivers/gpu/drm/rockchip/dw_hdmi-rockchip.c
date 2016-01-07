/*
 * Copyright (c) 2014, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <drm/drm_of.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_encoder_slave.h>
#include <drm/bridge/dw_hdmi.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_vop.h"

#include "dw_hdmi-rockchip.h"

struct rockchip_hdmi {
	struct device *dev;
	struct regmap *regmap;
	struct drm_encoder encoder;
	struct dw_hdmi_plat_data plat_data;

	void __iomem *extphy_regbase;
	struct clk *extphy_pclk;
};

#define to_rockchip_hdmi(x)	container_of(x, struct rockchip_hdmi, x)

static const struct extphy_config_tab rockchip_extphy_cfg[] = {
	{ .mpixelclock = 165000000,
	  .pre_emphasis = 0, .slopeboost = 0, .clk_level = 4,
	  .data0_level = 4, 4, 4,
	},

	{ .mpixelclock = 225000000,
	  .pre_emphasis = 0, .slopeboost = 0, .clk_level = 6,
	  .data0_level = 6, 6, 6,
	},

	{ .mpixelclock = 340000000,
	  .pre_emphasis = 1, .slopeboost = 0, .clk_level = 6,
	  .data0_level = 10, 10, 10,
	},

	{ .mpixelclock = 594000000,
	  .pre_emphasis = 1, .slopeboost = 0, .clk_level = 7,
	  .data0_level = 10, 10, 10,
	},

	{ .mpixelclock = ~0UL},
};

static const struct extphy_pll_config_tab rockchip_extphy_pll_cfg[] = {
	{
		.mpixelclock = 27000000, .param = {
			{ .pll_nd = 1, .pll_nf = 45,
			  .tmsd_divider_a = 3, 1, 1,
			  .pclk_divider_a = 1, 3, 3, 4,
			  .vco_div_5 = 0,
			  .ppll_nd = 1, .ppll_nf = 40, .ppll_no = 8,
			},
			{ .pll_nd = 1, .pll_nf = 45,
			  .tmsd_divider_a = 0, 3, 3,
			  .pclk_divider_a = 1, 3, 3, 4,
			  .vco_div_5 = 0,
			  .ppll_nd = 1, .ppll_nf = 40, .ppll_no = 8,
			},
		},
	}, {
		.mpixelclock = 59400000, .param = {
			{ .pll_nd = 2, .pll_nf = 99,
			  .tmsd_divider_a = 3, 1, 1,
			  .pclk_divider_a = 1, 3, 2, 2,
			  .vco_div_5 = 0,
			  .ppll_nd = 1, .ppll_nf = 40, .ppll_no = 8,
			},
			{ .pll_nd = 2, .pll_nf = 99,
			  .tmsd_divider_a = 1, 1, 1,
			  .pclk_divider_a = 1, 3, 2, 2,
			  .vco_div_5 = 0,
			  .ppll_nd = 1, .ppll_nf = 40, .ppll_no = 8,
			},
		},
	}, {
		.mpixelclock = 74250000, .param = {
			{ .pll_nd = 2, .pll_nf = 99,
			  .tmsd_divider_a = 1, 1, 1,
			  .pclk_divider_a = 1, 2, 2, 2,
			  .vco_div_5 = 0,
			  .ppll_nd = 1, .ppll_nf = 40, .ppll_no = 8,
			},
			{ .pll_nd = 4, .pll_nf = 495,
			  .tmsd_divider_a = 1, 2, 2,
			  .pclk_divider_a = 1, 3, 3, 4,
			  .vco_div_5 = 0,
			  .ppll_nd = 2, .ppll_nf = 40, .ppll_no = 4,
			},
		},
	}, {
		.mpixelclock = 148500000, .param = {
			{ .pll_nd = 2, .pll_nf = 99,
			  .tmsd_divider_a = 1, 0, 0,
			  .pclk_divider_a = 1, 2, 1, 1,
			  .vco_div_5 = 0,
			  .ppll_nd = 2, .ppll_nf = 40, .ppll_no = 4,
			},
			{ .pll_nd = 4, .pll_nf = 495,
			  .tmsd_divider_a = 0, 2, 2,
			  .pclk_divider_a = 1, 3, 2, 2,
			  .vco_div_5 = 0,
			  .ppll_nd = 4, .ppll_nf = 40, .ppll_no = 2,
			},
		},
	}, {
		.mpixelclock = 297000000, .param = {
			{ .pll_nd = 2, .pll_nf = 99,
			  .tmsd_divider_a = 0, 0, 0,
			  .pclk_divider_a = 1, 0, 1, 1,
			  .vco_div_5 = 0,
			  .ppll_nd = 4, .ppll_nf = 40, .ppll_no = 2,
			},
			{ .pll_nd = 4, .pll_nf = 495,
			  .tmsd_divider_a = 1, 2, 0,
			  .pclk_divider_a = 1, 3, 1, 1,
			  .vco_div_5 = 0,
			  .ppll_nd = 8, .ppll_nf = 40, .ppll_no = 1,
			},
		},
	}, {
		.mpixelclock = 594000000, .param = {
			{ .pll_nd = 1, .pll_nf = 99,
			  .tmsd_divider_a = 0, 2, 0,
			  .pclk_divider_a = 1, 0, 1, 1,
			  .vco_div_5 = 0,
			  .ppll_nd = 8, .ppll_nf = 40, .ppll_no = 1,
			},
		}
	}, {
		.mpixelclock = ~0UL,
	}
};

static const struct dw_hdmi_mpll_config rockchip_mpll_cfg[] = {
	{
		27000000, {
			{ 0x00b3, 0x0000},
			{ 0x2153, 0x0000},
			{ 0x40f3, 0x0000}
		},
	}, {
		36000000, {
			{ 0x00b3, 0x0000},
			{ 0x2153, 0x0000},
			{ 0x40f3, 0x0000}
		},
	}, {
		40000000, {
			{ 0x00b3, 0x0000},
			{ 0x2153, 0x0000},
			{ 0x40f3, 0x0000}
		},
	}, {
		54000000, {
			{ 0x0072, 0x0001},
			{ 0x2142, 0x0001},
			{ 0x40a2, 0x0001},
		},
	}, {
		65000000, {
			{ 0x0072, 0x0001},
			{ 0x2142, 0x0001},
			{ 0x40a2, 0x0001},
		},
	}, {
		66000000, {
			{ 0x013e, 0x0003},
			{ 0x217e, 0x0002},
			{ 0x4061, 0x0002}
		},
	}, {
		74250000, {
			{ 0x0072, 0x0001},
			{ 0x2145, 0x0002},
			{ 0x4061, 0x0002}
		},
	}, {
		83500000, {
			{ 0x0072, 0x0001},
		},
	}, {
		108000000, {
			{ 0x0051, 0x0002},
			{ 0x2145, 0x0002},
			{ 0x4061, 0x0002}
		},
	}, {
		106500000, {
			{ 0x0051, 0x0002},
			{ 0x2145, 0x0002},
			{ 0x4061, 0x0002}
		},
	}, {
		146250000, {
			{ 0x0051, 0x0002},
			{ 0x2145, 0x0002},
			{ 0x4061, 0x0002}
		},
	}, {
		148500000, {
			{ 0x0051, 0x0003},
			{ 0x214c, 0x0003},
			{ 0x4064, 0x0003}
		},
	}, {
		~0UL, {
			{ 0x00a0, 0x000a },
			{ 0x2001, 0x000f },
			{ 0x4002, 0x000f },
		},
	}
};

static const struct dw_hdmi_curr_ctrl rockchip_cur_ctr[] = {
	/*      pixelclk    bpp8    bpp10   bpp12 */
	{
		40000000,  { 0x0018, 0x0018, 0x0018 },
	}, {
		65000000,  { 0x0028, 0x0028, 0x0028 },
	}, {
		66000000,  { 0x0038, 0x0038, 0x0038 },
	}, {
		74250000,  { 0x0028, 0x0038, 0x0038 },
	}, {
		83500000,  { 0x0028, 0x0038, 0x0038 },
	}, {
		146250000, { 0x0038, 0x0038, 0x0038 },
	}, {
		148500000, { 0x0000, 0x0038, 0x0038 },
	}, {
		~0UL,      { 0x0000, 0x0000, 0x0000},
	}
};

static const struct dw_hdmi_phy_config rockchip_phy_config[] = {
	/*pixelclk   symbol   term   vlev*/
	{ 74250000,  0x8009, 0x0004, 0x0272},
	{ 148500000, 0x802b, 0x0004, 0x028d},
	{ 297000000, 0x8039, 0x0005, 0x028d},
	{ ~0UL,	     0x0000, 0x0000, 0x0000}
};

static inline void hdmi_extphy_write(struct rockchip_hdmi *hdmi,
				     unsigned short data, unsigned char addr)
{
	writel_relaxed(data, hdmi->extphy_regbase + (addr) * 0x04);
}

static inline unsigned int hdmi_extphy_read(struct rockchip_hdmi *hdmi,
					    unsigned char addr)
{
	return readl_relaxed(hdmi->extphy_regbase + (addr) * 0x04);
}

static int rockchip_extphy_config(const struct dw_hdmi_plat_data *plat_data,
				  int res, int pixelclock)
{
	struct rockchip_hdmi *hdmi = to_rockchip_hdmi(plat_data);
	const struct extphy_pll_config_tab *mpll = rockchip_extphy_pll_cfg;
	const struct extphy_config_tab *ctrl = rockchip_extphy_cfg;
	const struct extphy_pll_config_param *param;
	unsigned long timeout;
	int i, stat;

	if (res >= DW_HDMI_RES_MAX) {
		dev_err(hdmi->dev, "Extphy can't support res %d\n", res);
		return -EINVAL;
	}

	/* Find out the extphy MPLL configure parameters */
	for (i = 0; mpll[i].mpixelclock != ~0UL; i++)
		if (pixelclock == mpll[i].mpixelclock)
			break;
	if (mpll[i].mpixelclock == ~0UL) {
		dev_err(hdmi->dev, "Extphy can't support %dHz\n", pixelclock);
		return -EINVAL;
	}
	param = &mpll[i].param[res];

	regmap_write(hdmi->regmap, RK3229_GRF_SOC_CON2,
		     RK3229_PLL_POWER_DOWN | RK3229_PLL_PDATA_DEN);

	/*
	 * Configure external HDMI PHY PLL registers.
	 */
	stat = ((param->pll_nf >> 1) & EXT_PHY_PLL_FB_BIT8_MASK) |
	       ((param->vco_div_5 & 1) << 5) |
	       (param->pll_nd & EXT_PHY_PLL_PRE_DIVIDER_MASK);
	hdmi_extphy_write(hdmi, stat, EXT_PHY_PLL_PRE_DIVIDER);

	hdmi_extphy_write(hdmi, param->pll_nf, EXT_PHY_PLL_FB_DIVIDER);

	stat = (param->pclk_divider_a & EXT_PHY_PCLK_DIVIDERA_MASK) |
	       ((param->pclk_divider_b & 3) << 5);
	hdmi_extphy_write(hdmi, stat, EXT_PHY_PCLK_DIVIDER1);

	stat = (param->pclk_divider_d & EXT_PHY_PCLK_DIVIDERD_MASK) |
	       ((param->pclk_divider_c & 3) << 5);
	hdmi_extphy_write(hdmi, stat, EXT_PHY_PCLK_DIVIDER2);

	stat = ((param->tmsd_divider_c & 3) << 4) |
	       ((param->tmsd_divider_a & 3) << 2) |
	       (param->tmsd_divider_b & 3);
	hdmi_extphy_write(hdmi, stat, EXT_PHY_TMDSCLK_DIVIDER);

	hdmi_extphy_write(hdmi, param->ppll_nf, EXT_PHY_PPLL_FB_DIVIDER);

	if (param->ppll_no == 1) {
		hdmi_extphy_write(hdmi, 0, EXT_PHY_PPLL_POST_DIVIDER);

		stat = 0x20 | param->ppll_nd;
		hdmi_extphy_write(hdmi, stat, EXT_PHY_PPLL_PRE_DIVIDER);
	} else {
		stat = ((param->ppll_no / 2) - 1) << 4;
		hdmi_extphy_write(hdmi, stat, EXT_PHY_PPLL_POST_DIVIDER);

		stat = 0xe0 | param->ppll_nd;
		hdmi_extphy_write(hdmi, stat, EXT_PHY_PPLL_PRE_DIVIDER);
	}


	/* Find out the external HDMI PHY driver configure parameters */
	for (i = 0; ctrl[i].mpixelclock != ~0UL; i++)
		if (pixelclock <= ctrl[i].mpixelclock)
			break;
	if (ctrl[i].mpixelclock == ~0UL) {
		dev_err(hdmi->dev, "Extphy can't support %dHz\n", pixelclock);
		return -EINVAL;
	}

	/*
	 * Configure the external HDMI PHY driver registers.
	 */
	if (ctrl[i].slopeboost) {
		hdmi_extphy_write(hdmi, 0xff, EXT_PHY_SIGNAL_CTRL);

		stat = (ctrl[i].slopeboost - 1) & 3;
		stat = (stat << 6) | (stat << 4) | (stat << 2) | stat;
		hdmi_extphy_write(hdmi, stat, EXT_PHY_SLOPEBOOST);
	} else
		hdmi_extphy_write(hdmi, 0x0f, EXT_PHY_SIGNAL_CTRL);

	stat = ctrl[i].pre_emphasis & 3;
	stat = (stat << 4) | (stat << 2) | stat;
	hdmi_extphy_write(hdmi, stat, EXT_PHY_PREEMPHASIS);

	stat = ((ctrl[i].clk_level & 0xf) << 4) | (ctrl[i].data2_level & 0xf);
	hdmi_extphy_write(hdmi, stat, EXT_PHY_LEVEL1);

	stat = ((ctrl[i].data1_level & 0xf) << 4) | (ctrl[i].data0_level & 0xf);
	hdmi_extphy_write(hdmi, stat, EXT_PHY_LEVEL2);

	hdmi_extphy_write(hdmi, 0x22, 0xf3);

	stat = clk_get_rate(hdmi->extphy_pclk) / 100000;
	hdmi_extphy_write(hdmi, ((stat >> 8) & 0xff) | 0x80, EXT_PHY_TERM_CAL);
	hdmi_extphy_write(hdmi,  stat & 0xff, EXT_PHY_TERM_CAL_DIV_L);

	if (pixelclock > 340000000)
		stat = EXT_PHY_AUTO_R100_OHMS;
	else if (pixelclock > 200000000)
		stat = EXT_PHY_AUTO_R50_OHMS;
	else
		stat = EXT_PHY_AUTO_ROPEN_CIRCUIT;
	hdmi_extphy_write(hdmi, stat | 0x20, EXT_PHY_TERM_RESIS_AUTO);
	hdmi_extphy_write(hdmi, (stat >> 8) & 0xff, EXT_PHY_TERM_CAL);

	stat = (pixelclock > 200000000) ? 0 : 0x11;
	hdmi_extphy_write(hdmi, stat, EXT_PHY_PLL_BW);
	hdmi_extphy_write(hdmi, 0x27, EXT_PHY_PPLL_BW);

	regmap_write(hdmi->regmap, RK3229_GRF_SOC_CON2, RK3229_PLL_POWER_UP);

	/* Detect whether PLL is lock or not */
	timeout = jiffies + msecs_to_jiffies(100);
	while (!time_after(jiffies, timeout)) {
		usleep_range(1000, 2000);
		stat = hdmi_extphy_read(hdmi, EXT_PHY_PPLL_POST_DIVIDER);
		if (stat & EXT_PHY_PPLL_LOCK_STATUS_MASK)
			break;
	}

	regmap_write(hdmi->regmap, RK3229_GRF_SOC_CON2, RK3229_PLL_PDATA_EN);

	if ((stat & EXT_PHY_PPLL_LOCK_STATUS_MASK) == 0) {
		dev_err(hdmi->dev, "EXT PHY PLL not locked\n");
		return -EBUSY;
	}

	return 0;
}

static int rockchip_hdmi_parse_dt(struct rockchip_hdmi *hdmi)
{
	struct device_node *np = hdmi->dev->of_node;
	struct platform_device *pdev;
	struct resource *iores;
	int ret;

	pdev = container_of(hdmi->dev, struct platform_device, dev);

	hdmi->regmap = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(hdmi->regmap)) {
		dev_err(hdmi->dev, "Unable to get rockchip,grf\n");
		return PTR_ERR(hdmi->regmap);
	}

	if (hdmi->plat_data.dev_type == RK3229_HDMI) {
		iores = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!iores)
			return -ENXIO;

		hdmi->extphy_regbase = devm_ioremap_resource(hdmi->dev, iores);
		if (IS_ERR(hdmi->extphy_regbase)) {
			dev_err(hdmi->dev, "failed to map extphy regbase\n");
			return PTR_ERR(hdmi->extphy_regbase);
		}

		hdmi->extphy_pclk = devm_clk_get(hdmi->dev, "extphy");
		if (IS_ERR(hdmi->extphy_pclk)) {
			dev_err(hdmi->dev, "failed to get extphy clock\n");
			return PTR_ERR(hdmi->extphy_pclk);
		}

		ret = clk_prepare_enable(hdmi->extphy_pclk);
		if (ret) {
			dev_err(hdmi->dev, "failed to enable extphy clk: %d\n",
				ret);
			return ret;
		}

		regmap_write(hdmi->regmap, RK3229_GRF_SOC_CON6,
			     RK3229_IO_3V_DOMAIN);

		regmap_write(hdmi->regmap, RK3229_GRF_SOC_CON2,
			     RK3229_DDC_MASK_EN);
	}

	return 0;
}

static enum drm_mode_status
dw_hdmi_rockchip_mode_valid(const struct dw_hdmi_plat_data *plat_data,
			    struct drm_display_mode *mode)
{
	int pclk = mode->clock * 1000;
	bool valid = false;
	int i;

	if (plat_data->dev_type == RK3288_HDMI)
		for (i = 0; rockchip_mpll_cfg[i].mpixelclock != ~0UL; i++)
			if (pclk == rockchip_mpll_cfg[i].mpixelclock) {
				valid = true;
				break;
			}

	if (plat_data->dev_type == RK3229_HDMI)
		for (i = 0; rockchip_extphy_pll_cfg[i].mpixelclock != ~0UL; i++)
			if (pclk == rockchip_extphy_pll_cfg[i].mpixelclock) {
				valid = true;
				break;
			}

	return (valid) ? MODE_OK : MODE_BAD;
}

static const struct drm_encoder_funcs dw_hdmi_rockchip_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static void dw_hdmi_rockchip_encoder_disable(struct drm_encoder *encoder)
{
}

static bool
dw_hdmi_rockchip_encoder_mode_fixup(struct drm_encoder *encoder,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adj_mode)
{
	return true;
}

static void dw_hdmi_rockchip_encoder_mode_set(struct drm_encoder *encoder,
					      struct drm_display_mode *mode,
					      struct drm_display_mode *adj_mode)
{
}

static void dw_hdmi_rockchip_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_hdmi *hdmi = to_rockchip_hdmi(encoder);
	int out_mode = ROCKCHIP_OUT_MODE_AAAA;
	u32 val;
	int mux;

	if (hdmi->plat_data.dev_type == RK3229_HDMI)
		out_mode = ROCKCHIP_OUT_MODE_P888;

	rockchip_drm_crtc_mode_config(encoder->crtc, DRM_MODE_CONNECTOR_HDMIA,
				      out_mode);

	if (hdmi->plat_data.dev_type == RK3288_HDMI) {
		mux = rockchip_drm_encoder_get_mux_id(hdmi->dev->of_node,
						      encoder);
		if (mux)
			val = RK3288_HDMI_SEL_VOP_LIT |
			      (RK3288_HDMI_SEL_VOP_LIT << 16);
		else
			val = RK3288_HDMI_SEL_VOP_LIT << 16;

		regmap_write(hdmi->regmap, RK3288_GRF_SOC_CON6, val);

		dev_dbg(hdmi->dev, "vop %s output to hdmi\n",
			(mux) ? "LIT" : "BIG");
	}
}

static const struct drm_encoder_helper_funcs dw_hdmi_rockchip_encoder_helper_funcs = {
	.mode_fixup = dw_hdmi_rockchip_encoder_mode_fixup,
	.mode_set   = dw_hdmi_rockchip_encoder_mode_set,
	.enable     = dw_hdmi_rockchip_encoder_enable,
	.disable    = dw_hdmi_rockchip_encoder_disable,
};

static const struct dw_hdmi_plat_data rk3288_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg   = rockchip_mpll_cfg,
	.cur_ctr    = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.dev_type   = RK3288_HDMI,
};

static const struct dw_hdmi_plat_data rk3229_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.extphy_config = rockchip_extphy_config,
	.dev_type   = RK3229_HDMI,
};

static const struct of_device_id dw_hdmi_rockchip_dt_ids[] = {
	{ .compatible = "rockchip,rk3288-dw-hdmi",
	  .data = &rk3288_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3229-dw-hdmi",
	  .data = &rk3229_hdmi_drv_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, dw_hdmi_rockchip_dt_ids);

static int dw_hdmi_rockchip_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	const struct of_device_id *match;
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct rockchip_hdmi *hdmi;
	struct resource *iores;
	int irq;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	hdmi = devm_kzalloc(&pdev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	match = of_match_node(dw_hdmi_rockchip_dt_ids, pdev->dev.of_node);
	hdmi->plat_data = *(const struct dw_hdmi_plat_data *)match->data;
	hdmi->dev = &pdev->dev;
	encoder = &hdmi->encoder;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores)
		return -ENXIO;

	platform_set_drvdata(pdev, hdmi);

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	/*
	 * If we failed to find the CRTC(s) which this encoder is
	 * supposed to be connected to, it's because the CRTC has
	 * not been registered yet.  Defer probing, and hope that
	 * the required CRTC is added later.
	 */
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	ret = rockchip_hdmi_parse_dt(hdmi);
	if (ret) {
		dev_err(hdmi->dev, "Unable to parse OF data\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &dw_hdmi_rockchip_encoder_helper_funcs);
	drm_encoder_init(drm, encoder, &dw_hdmi_rockchip_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	return dw_hdmi_bind(dev, master, data, encoder, iores, irq,
			    &hdmi->plat_data);
}

static void dw_hdmi_rockchip_unbind(struct device *dev, struct device *master,
				    void *data)
{
	return dw_hdmi_unbind(dev, master, data);
}

static const struct component_ops dw_hdmi_rockchip_ops = {
	.bind	= dw_hdmi_rockchip_bind,
	.unbind	= dw_hdmi_rockchip_unbind,
};

static int dw_hdmi_rockchip_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &dw_hdmi_rockchip_ops);
}

static int dw_hdmi_rockchip_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dw_hdmi_rockchip_ops);

	return 0;
}

static struct platform_driver dw_hdmi_rockchip_pltfm_driver = {
	.probe  = dw_hdmi_rockchip_probe,
	.remove = dw_hdmi_rockchip_remove,
	.driver = {
		.name = "dwhdmi-rockchip",
		.of_match_table = dw_hdmi_rockchip_dt_ids,
	},
};

module_platform_driver(dw_hdmi_rockchip_pltfm_driver);

MODULE_AUTHOR("Andy Yan <andy.yan@rock-chips.com>");
MODULE_AUTHOR("Yakir Yang <ykk@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip Specific DW-HDMI Driver Extension");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:dwhdmi-rockchip");
