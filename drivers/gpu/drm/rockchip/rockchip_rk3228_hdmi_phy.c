/*
 * Copyright (c) 2016 Fuzhou Rockchip Inc.
 * Author: Yakir Yang <ykk@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#define PHY_CONTROL				0
#define PHY_SOFT_RESET_SHIFT			6
#define PHY_SRC_SELECT_SHIFT			0
#define PHY_ANALOG_RESET			BIT(7)
#define PHY_DIGITAL_RESET			BIT(6)
#define PHY_SRC_SELECT_CRYSTAL_OSC		BIT(0)

#define PHY_TERM_CAL				0x03
#define PHY_TERM_CAL_EN_MASK			0x80
#define PHY_TERM_CAL_DIV_H_MASK			0x7f

#define PHY_TERM_CAL_DIV_L			0x04

#define PHY_PLL_PRE_DIVIDER			0xe2
#define PHY_PLL_FB_BIT8_MASK			0x80
#define PHY_PLL_PCLK_DIV5_EN_MASK		0x20
#define PHY_PLL_PRE_DIVIDER_MASK		0x1f

#define PHY_PLL_FB_DIVIDER			0xe3

#define PHY_PCLK_DIVIDER1			0xe4
#define PHY_PCLK_DIVIDERB_MASK			0x60
#define PHY_PCLK_DIVIDERA_MASK			0x1f

#define PHY_PCLK_DIVIDER2			0xe5
#define PHY_PCLK_DIVIDERC_MASK			0x60
#define PHY_PCLK_DIVIDERD_MASK			0x1f

#define PHY_TMDSCLK_DIVIDER			0xe6
#define PHY_TMDSCLK_DIVIDERC_MASK		0x30
#define PHY_TMDSCLK_DIVIDERA_MASK		0x0c
#define PHY_TMDSCLK_DIVIDERB_MASK		0x03

#define PHY_PLL_BW				0xe7

#define PHY_PPLL_PRE_DIVIDER			0xe9
#define PHY_PPLL_ENABLE_MASK			0xc0
#define PHY_PPLL_PRE_DIVIDER_MASK		0x1f

#define PHY_PPLL_FB_DIVIDER			0xea

#define PHY_PPLL_POST_DIVIDER			0xeb
#define PHY_PPLL_FB_DIVIDER_BIT8_MASK		0x80
#define PHY_PPLL_POST_DIVIDER_MASK		0x30
#define PHY_PPLL_LOCK_STATUS_MASK		0x01

#define PHY_PPLL_BW				0xec

#define PHY_SIGNAL_CTRL				0xee
#define PHY_TRANSITION_CLK_EN_MASK		0x80
#define PHY_TRANSITION_D0_EN_MASK		0x40
#define PHY_TRANSITION_D1_EN_MASK		0x20
#define PHY_TRANSITION_D2_EN_MASK		0x10
#define PHY_LEVEL_CLK_EN_MASK			0x08
#define PHY_LEVEL_D0_EN_MASK			0x04
#define PHY_LEVEL_D1_EN_MASK			0x02
#define PHY_LEVEL_D2_EN_MASK			0x01

#define PHY_SLOPEBOOST				0xef
#define PHY_SLOPEBOOST_CLK_MASK			0x03
#define PHY_SLOPEBOOST_D0_MASK			0x0c
#define PHY_SLOPEBOOST_D1_MASK			0x30
#define PHY_SLOPEBOOST_D2_MASK			0xc0

#define PHY_PREEMPHASIS				0xf0
#define PHY_PREEMPHASIS_D0_MASK			0x03
#define PHY_PREEMPHASIS_D1_MASK			0x0c
#define PHY_PREEMPHASIS_D2_MASK			0x30

#define PHY_LEVEL1				0xf1
#define PHY_LEVEL_CLK_MASK			0xf0
#define PHY_LEVEL_D2_MASK			0x0f

#define PHY_LEVEL2				0xf2
#define PHY_LEVEL_D1_MASK			0xf0
#define PHY_LEVEL_D0_MASK			0x0f

#define PHY_TERM_RESIS_AUTO			0xf4
#define PHY_AUTO_R50_OHMS			0
#define PHY_AUTO_R75_OHMS			(1 << 2)
#define PHY_AUTO_R100_OHMS			(2 << 2)
#define PHY_AUTO_ROPEN_CIRCUIT			(3 << 2)

#define PHY_TERM_RESIS_MANUAL_CLK		0xfb
#define PHY_TERM_RESIS_MANUAL_D2		0xfc
#define PHY_TERM_RESIS_MANUAL_D1		0xfd
#define PHY_TERM_RESIS_MANUAL_D0		0xfe

#define HDMI_PHY_NR_OUTPUT		2

struct rk_hdmi_phy {
	void __iomem *regs;
	struct device *dev;
	struct clk *tmds;
	struct clk *pclk;
	struct clk_onecell_data clk_data;
	struct clk_hw tmds_hw;
	struct clk_hw pclk_hw;
	unsigned long pll_rate;
	u8 drv_imp_clk;
	u8 drv_imp_d2;
	u8 drv_imp_d1;
	u8 drv_imp_d0;
	u32 ibias;
	u32 ibias_up;
};

#define PHY_PLL_RATE(_rate, _prediv, _fbdiv, _tmdsdiv_a, _tmdsdiv_b, \
		     _tmdsdiv_c, _tmdsdiv_d, _pclkdiv_a, _pclkdiv_b, \
		     _pclkdiv_c, _pclkdiv_d, _endiv_vco		\
{								\
	.rate	= _rate##U,					\
	.prediv = _prebdiv,					\
	.fbdiv = _fbdiv,					\
	.tmdsdiv_a = _tmdsdiv_a,				\
	.tmdsdiv_b = _tmdsdiv_b,				\
	.tmdsdiv_c = _tmdsdiv_c,				\
	.tmdsdiv_d = _tmdsdiv_d,				\
	.pclkdiv_a = _pclkdiv_a,				\
	.pclkdiv_b = _pclkdiv_b,				\
	.pclkdiv_c = _pclkdiv_c,				\
	.pclkdiv_d = _pclkdiv_d,				\
	.endiv_vco = _endiv_vco,				\
}

static struct rk_hdmi_phy_pll_rate_table hdmi_phy_pll_rates[] = {
	PHY_PLL_RATE(25200000, 1, 84, 3, 2, 2, 20, 0, 2, 2, 0), /* Fvco = 2.016 GHz */
	PHY_PLL_RATE(27000000, 1, 90, 3, 2, 2, 20, 0, 2, 2, 0), /* Fvco = 2.16 GHz */
	PHY_PLL_RATE(31500000, 1, 84, 1, 3, 3, 16, 0, 2, 2, 0), /* Fvco = 2.016 GHz */
	PHY_PLL_RATE(32000000, 1, 64, 2, 2, 2, 24, 0, 1, 1, 0), /* Fvco = 1.536 GHz */
	PHY_PLL_RATE(33750000, 1, 90, 1, 3, 3, 16, 0, 2, 2, 0), /* Fvco = 2.16 GHz */
	PHY_PLL_RATE(36000000, 1, 60, 3, 1, 1, 1,  3, 3, 4, 0), /* Fvco = 1.44 GHz */
	PHY_PLL_RATE(40000000, 1, 80, 2, 2, 2, 24, 0, 1, 1, 0), /* Fvco = 1.92 GHz */
	PHY_PLL_RATE(49500000, 1, 66, 1, 2, 2, 1,  2, 3, 4, 0), /* Fvco = 1.584 GHz */
	PHY_PLL_RATE(50000000, 1, 50, 2, 1, 1, 1,  1, 3, 4, 0), /* Fvco = 1.2 GHz */
	PHY_PLL_RATE(54000000, 1, 90, 3, 1, 1, 1,  3, 3, 4, 0), /* Fvco = 2.15 GHz */
	PHY_PLL_RATE(65000000, 1, 65, 2, 1, 1, 1,  1, 3, 4, 0), /* Fvco = 1.56 GHz */
	PHY_PLL_RATE(68250000, 1, 91, 1, 2, 2, 1,  2, 3, 4, 0), /* Fvco = 2.184 GHz */
	PHY_PLL_RATE(71000000, 1, 71, 2, 1, 1, 1,  1, 3, 4, 0), /* Fvco = 1.704 GHz */
	PHY_PLL_RATE(72000000, 1, 96, 1, 2, 2, 1,  2, 3, 4, 0), /* Fvco = 2.304 GHz */
	PHY_PLL_RATE(74250000, 1, 99, 1, 2, 2, 1,  2, 3, 4, 0), /* Fvco = 2.376 GHz */
	PHY_PLL_RATE(75000000, 1, 100, 1, 2, 2, 1, 2, 3, 4, 0), /* Fvco = 2.4 GHz */
	PHY_PLL_RATE(78750000, 1, 105, 0, 3, 3, 1, 2, 3, 4, 0), /* Fvco = 2.52 GHz */
	PHY_PLL_RATE(78800000, 3, 197, 3, 0, 0, 1, 3, 2, 2, 0), /* Fvco = 1.576 GHz */
	PHY_PLL_RATE(79500000, 1, 106, 1, 2, 2, 1, 2, 3, 4, 0), /* Fvco = 2.544 GHz */
	PHY_PLL_RATE(83500000, 2, 167, 2, 1, 1, 1, 1, 3, 4, 0), /* Fvco = 2.004 GHz */
	PHY_PLL_RATE(85500000, 1, 114, 0, 3, 3, 1, 2, 3, 4, 0), /* Fvco = 2.736 GHz */
	PHY_PLL_RATE(106500000, 1, 71, 1, 1, 1, 1, 2, 2, 2, 0), /* Fvco = 1.704 GHz */
	PHY_PLL_RATE(108000000, 1, 108, 2, 1, 1, 1, 1, 3, 4, 0), /* Fvco = 2.592 GHz */
	PHY_PLL_RATE(115500000, 1, 77, 1, 1, 1, 1, 2, 2, 2, 0), /* Fvco = 1.848 GHz */
	PHY_PLL_RATE(135000000, 1, 90, 1, 1, 1, 1, 2, 2, 2, 0), /* Fvco = 2.16 GHz */
	PHY_PLL_RATE(146250000, 2, 195, 1, 1, 1, 1, 2, 2, 2, 0), /* Fvco = 2.34 GHz */
	PHY_PLL_RATE(148500000, 1, 99, 1, 1, 1, 1, 2, 2, 2, 0), /* Fvco = 2.376 GHz */
	PHY_PLL_RATE(154000000, 1, 77, 2, 0, 0, 1, 1, 2, 2, 0), /* Fvco = 1.848 GHz */
	PHY_PLL_RATE(162000000, 1, 108, 1, 1, 1, 1, 2, 2, 2, 0), /* Fvco = 2.592 GHz */
	PHY_PLL_RATE(297000000, 1, 99, 1, 0, 0, 1, 2, 1, 1, 0), /* Fvco = 2.376 GHz */
	PHY_PLL_RATE(594000000, 1, 99, 0, 0, 0, 1, 0, 1, 1, 0), /* Fvco = 2.376 GHz */
};

static void rk_hdmi_phy_mask(struct rk_hdmi_phy *hdmi_phy, u32 offset,
			      u32 val, u32 mask)
{
	u32 tmp = readl(hdmi_phy->regs  + offset) & ~mask;

	tmp |= (val & mask);
	writel(tmp, hdmi_phy->regs + offset);
}

static inline struct rk_hdmi_phy *to_rk_hdmi_phy(struct clk_hw *hw)
{
	return container_of(hw, struct rk_hdmi_phy, pll_hw);
}

static int rk_hdmi_tmds_prepare(struct clk_hw *hw)
{
	struct rk_hdmi_phy *hdmi_phy = to_rk_hdmi_phy(hw);

	rk_hdmi_phy_mask(hdmi_phy, PHY_CONTROL, 3 << PHY_SOFT_RESET_SHIFT, 0);

	usleep_range(50, 100);

	rk_hdmi_phy_mask(hdmi_phy, PHY_CONTROL, 3 << PHY_SOFT_RESET_SHIFT,
			 PHY_ANALOG_RESET | PHY_DIGITAL_RESET);

	rk_hdmi_phy_mask(hdmi_phy, PHY_CONTROL, 1 << PHY_SRC_SELECT_SHIFT,
			 PHY_SRC_SELECT_CRYSTAL_OSC);
}

static void rk_hdmi_tmds_unprepare(struct clk_hw *hw)
{
}

static int rk_hdmi_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
}

static long rk_hdmi_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
}

static unsigned long rk_hdmi_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct rk_hdmi_phy *hdmi_phy = to_rk_hdmi_phy(hw);

	return hdmi_phy->pll_rate;
}

static const struct clk_ops rk_hdmi_tmds_ops = {
	.prepare = rk_hdmi_tmds_prepare,
	.unprepare = rk_hdmi_tmds_unprepare,
	.set_rate = rk_hdmi_tmds_set_rate,
	.round_rate = rk_hdmi_tmds_round_rate,
	.recalc_rate = rk_hdmi_tmds_recalc_rate,
};

static const struct clk_ops rk_hdmi_pclk_ops = {
	.set_rate = rk_hdmi_pclk_set_rate,
	.round_rate = rk_hdmi_pclk_round_rate,
	.recalc_rate = rk_hdmi_pclk_recalc_rate,
};

static void rk_hdmi_phy_enable_tmds(struct rk_hdmi_phy *hdmi_phy)
{
	rk_hdmi_phy_mask(hdmi_phy, HDMI_CON3, RG_HDMITX_SER_EN,
			  RG_HDMITX_SER_EN);
	rk_hdmi_phy_mask(hdmi_phy, HDMI_CON3, RG_HDMITX_PRD_EN,
			  RG_HDMITX_PRD_EN);
	rk_hdmi_phy_mask(hdmi_phy, HDMI_CON3, RG_HDMITX_DRV_EN,
			  RG_HDMITX_DRV_EN);
	usleep_range(100, 150);
}

static void rk_hdmi_phy_disable_tmds(struct rk_hdmi_phy *hdmi_phy)
{
	rk_hdmi_phy_mask(hdmi_phy, HDMI_CON3, 0, RG_HDMITX_DRV_EN);
	rk_hdmi_phy_mask(hdmi_phy, HDMI_CON3, 0, RG_HDMITX_PRD_EN);
	rk_hdmi_phy_mask(hdmi_phy, HDMI_CON3, 0, RG_HDMITX_SER_EN);
}

static int rk_hdmi_phy_power_on(struct phy *phy)
{
	struct rk_hdmi_phy *hdmi_phy = phy_get_drvdata(phy);
	int ret;

	ret = clk_prepare_enable(hdmi_phy->pll);
	if (ret < 0)
		return ret;

	rk_hdmi_phy_enable_tmds(hdmi_phy);

	return 0;
}

static int rk_hdmi_phy_power_off(struct phy *phy)
{
	struct rk_hdmi_phy *hdmi_phy = phy_get_drvdata(phy);

	rk_hdmi_phy_disable_tmds(hdmi_phy);
	clk_disable_unprepare(hdmi_phy->pll);

	return 0;
}

static struct phy_ops rk_hdmi_phy_ops = {
	.power_on = rk_hdmi_phy_power_on,
	.power_off = rk_hdmi_phy_power_off,
	.owner = THIS_MODULE,
};

static int rk_hdmi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rk_hdmi_phy *hdmi_phy;
	struct resource *mem;
	struct clk *ref_clk;
	const char *ref_clk_name;
	struct clk_init_data clk_init_tmds;
	struct clk_init_data clk_init_pclk;
	struct clk *clk_table[HDMI_PHY_NR_OUTPUT];
	struct phy *phy;
	struct phy_provider *phy_provider;
	int ret;

	hdmi_phy = devm_kzalloc(dev, sizeof(*hdmi_phy), GFP_KERNEL);
	if (!hdmi_phy)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdmi_phy->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(hdmi_phy->regs)) {
		ret = PTR_ERR(hdmi_phy->regs);
		dev_err(dev, "Failed to get memory resource: %d\n", ret);
		return ret;
	}

	ref_clk = devm_clk_get(dev, "pll_ref");
	if (IS_ERR(ref_clk)) {
		ret = PTR_ERR(ref_clk);
		dev_err(&pdev->dev, "Failed to get PLL reference clock: %d\n",
			ret);
		return ret;
	}
	ref_clk_name = __clk_get_name(ref_clk);

	/*
	 * Init and register hdmi-phy TMDS clock.
	 */
	clk_init_tmds.flags = CLK_SET_RATE_GATE;
	clk_init_tmds.parent_names = ref_clk_name;
	clk_init_tmds.num_parents = 1;
	clk_init_tmds.name = "hdmi-phy-tmds";
	clk_init_tmds.ops = &rk_hdmi_tmds_ops;

	/* optional override of the clockname */
	of_property_read_string_index(dev->of_node, "clock-output-names",
				      0, &clk_init_tmds.name);

	hdmi_phy->tmds_hw.init = &clk_init_tmds;
	clk_table[0] = devm_clk_register(dev, &hdmi_phy->tmds_hw);
	if (IS_ERR(hdmi_phy->tmds)) {
		ret = PTR_ERR(hdmi_phy->tmds);
		dev_err(dev, "Failed to register TMDS: %d\n", ret);
		return ret;
	}

	/*
	 * Init and register hdmi-phy PCLK clock.
	 */
	clk_init_pclk.flags = CLK_SET_RATE_GATE;
	clk_init_pclk.parent_names = clk_init_tmds.name;
	clk_init_pclk.num_parents = 1;
	clk_init_pclk.name = "hdmi-phy-pclk";
	clk_init_pclk.ops = &rk_hdmi_pclk_ops;

	/* optional override of the clockname */
	of_property_read_string_index(dev->of_node, "clock-output-names",
				      1, &clk_init_pclk.name);

	hdmi_phy->pclk_hw.init = &clk_init_pclk;
	clk_table[1] = devm_clk_register(dev, &hdmi_phy->pclk_hw);
	if (IS_ERR(hdmi_phy->pclk)) {
		ret = PTR_ERR(hdmi_phy->pclk);
		dev_err(dev, "Failed to register PCLK: %d\n", ret);
		return ret;
	}

	hdmi_phy->clk_data.clks = clk_table;
	hdmi_phy->clk_data.clk_num = HDMI_PHY_NR_OUTPUT;

	phy = devm_phy_create(dev, NULL, &rk_hdmi_phy_ops);
	if (IS_ERR(phy)) {
		dev_err(dev, "Failed to create HDMI PHY\n");
		return PTR_ERR(phy);
	}
	phy_set_drvdata(phy, hdmi_phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	hdmi_phy->dev = dev;

	return of_clk_add_provider(node, of_clk_src_onecell_get,
				   &hdmi_phy->clk_data);
}

static int rk_hdmi_phy_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id rk_hdmi_phy_match[] = {
	{ .compatible = "rockchip,rk3288-hdmi-phy", },
	{},
};

struct platform_driver rk_hdmi_phy_driver = {
	.probe = rk_hdmi_phy_probe,
	.remove = rk_hdmi_phy_remove,
	.driver = {
		.name = "rockchip-hdmi-phy",
		.of_match_table = rk_hdmi_phy_match,
	},
};

MODULE_AUTHOR("Yair Yang <ykk@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RK3288 HDMI PHY Driver");
MODULE_LICENSE("GPL v2");
