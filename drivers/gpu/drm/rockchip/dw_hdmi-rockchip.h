#ifndef __DW_HDMI_ROCKCHIP__
#define __DW_HDMI_ROCKCHIP__

struct extphy_config_tab {
	u32 mpixelclock;
	int pre_emphasis;
	int slopeboost;
	int clk_level;
	int data0_level;
	int data1_level;
	int data2_level;
};

struct extphy_pll_config_tab {
	unsigned long mpixelclock;
	struct extphy_pll_config_param {
		u8	pll_nd;
		u16	pll_nf;
		u8	tmsd_divider_a;
		u8	tmsd_divider_b;
		u8	tmsd_divider_c;
		u8	pclk_divider_a;
		u8	pclk_divider_b;
		u8	pclk_divider_c;
		u8	pclk_divider_d;
		u8	vco_div_5;
		u8	ppll_nd;
		u8	ppll_nf;
		u8	ppll_no;
	} param[DW_HDMI_RES_MAX];
};

#define RK3288_GRF_SOC_CON6			0x025c
#define RK3288_HDMI_SEL_VOP_LIT			(1 << 4)

#define RK3229_GRF_SOC_CON6			0x0418
#define RK3229_IO_3V_DOMAIN			((7 << 4) | (7 << (4 + 16)))

#define RK3229_GRF_SOC_CON2			0x0408
#define RK3229_DDC_MASK_EN			((3 << 13) | (3 << (13 + 16)))

#define RK3229_PLL_POWER_DOWN			(BIT(12) | BIT(12 + 16))
#define RK3229_PLL_POWER_UP			BIT(12 + 16)
#define RK3229_PLL_PDATA_DEN			BIT(11 + 16)
#define RK3229_PLL_PDATA_EN			(BIT(11) | BIT(11 + 16))

#endif /* __DW_HDMI_ROCKCHIP__ */
