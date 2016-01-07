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

/* PHY Defined for RK322X */
#define EXT_PHY_CONTROL				0
#define EXT_PHY_ANALOG_RESET_MASK		0x80
#define EXT_PHY_DIGITAL_RESET_MASK		0x40
#define EXT_PHY_PCLK_INVERT_MASK		0x08
#define EXT_PHY_PREPCLK_INVERT_MASK		0x04
#define EXT_PHY_TMDSCLK_INVERT_MASK		0x02
#define EXT_PHY_SRC_SELECT_MASK			0x01

#define EXT_PHY_TERM_CAL			0x03
#define EXT_PHY_TERM_CAL_EN_MASK		0x80
#define EXT_PHY_TERM_CAL_DIV_H_MASK		0x7f

#define EXT_PHY_TERM_CAL_DIV_L			0x04

#define EXT_PHY_PLL_PRE_DIVIDER			0xe2
#define EXT_PHY_PLL_FB_BIT8_MASK		0x80
#define EXT_PHY_PLL_PCLK_DIV5_EN_MASK		0x20
#define EXT_PHY_PLL_PRE_DIVIDER_MASK		0x1f

#define EXT_PHY_PLL_FB_DIVIDER			0xe3

#define EXT_PHY_PCLK_DIVIDER1			0xe4
#define EXT_PHY_PCLK_DIVIDERB_MASK		0x60
#define EXT_PHY_PCLK_DIVIDERA_MASK		0x1f

#define EXT_PHY_PCLK_DIVIDER2			0xe5
#define EXT_PHY_PCLK_DIVIDERC_MASK		0x60
#define EXT_PHY_PCLK_DIVIDERD_MASK		0x1f

#define EXT_PHY_TMDSCLK_DIVIDER			0xe6
#define EXT_PHY_TMDSCLK_DIVIDERC_MASK		0x30
#define EXT_PHY_TMDSCLK_DIVIDERA_MASK		0x0c
#define EXT_PHY_TMDSCLK_DIVIDERB_MASK		0x03

#define EXT_PHY_PLL_BW				0xe7

#define EXT_PHY_PPLL_PRE_DIVIDER		0xe9
#define EXT_PHY_PPLL_ENABLE_MASK		0xc0
#define EXT_PHY_PPLL_PRE_DIVIDER_MASK		0x1f

#define EXT_PHY_PPLL_FB_DIVIDER			0xea

#define EXT_PHY_PPLL_POST_DIVIDER		0xeb
#define EXT_PHY_PPLL_FB_DIVIDER_BIT8_MASK	0x80
#define EXT_PHY_PPLL_POST_DIVIDER_MASK		0x30
#define EXT_PHY_PPLL_LOCK_STATUS_MASK		0x01

#define EXT_PHY_PPLL_BW				0xec

#define EXT_PHY_SIGNAL_CTRL			0xee
#define EXT_PHY_TRANSITION_CLK_EN_MASK		0x80
#define EXT_PHY_TRANSITION_D0_EN_MASK		0x40
#define EXT_PHY_TRANSITION_D1_EN_MASK		0x20
#define EXT_PHY_TRANSITION_D2_EN_MASK		0x10
#define EXT_PHY_LEVEL_CLK_EN_MASK		0x08
#define EXT_PHY_LEVEL_D0_EN_MASK		0x04
#define EXT_PHY_LEVEL_D1_EN_MASK		0x02
#define EXT_PHY_LEVEL_D2_EN_MASK		0x01

#define EXT_PHY_SLOPEBOOST			0xef
#define EXT_PHY_SLOPEBOOST_CLK_MASK		0x03
#define EXT_PHY_SLOPEBOOST_D0_MASK		0x0c
#define EXT_PHY_SLOPEBOOST_D1_MASK		0x30
#define EXT_PHY_SLOPEBOOST_D2_MASK		0xc0

#define EXT_PHY_PREEMPHASIS			0xf0
#define EXT_PHY_PREEMPHASIS_D0_MASK		0x03
#define EXT_PHY_PREEMPHASIS_D1_MASK		0x0c
#define EXT_PHY_PREEMPHASIS_D2_MASK		0x30

#define EXT_PHY_LEVEL1				0xf1
#define EXT_PHY_LEVEL_CLK_MASK			0xf0
#define EXT_PHY_LEVEL_D2_MASK			0x0f

#define EXT_PHY_LEVEL2				0xf2
#define EXT_PHY_LEVEL_D1_MASK			0xf0
#define EXT_PHY_LEVEL_D0_MASK			0x0f

#define EXT_PHY_TERM_RESIS_AUTO			0xf4
#define EXT_PHY_AUTO_R50_OHMS			0
#define EXT_PHY_AUTO_R75_OHMS			(1 << 2)
#define EXT_PHY_AUTO_R100_OHMS			(2 << 2)
#define EXT_PHY_AUTO_ROPEN_CIRCUIT		(3 << 2)

#define EXT_PHY_TERM_RESIS_MANUAL_CLK		0xfb
#define EXT_PHY_TERM_RESIS_MANUAL_D2		0xfc
#define EXT_PHY_TERM_RESIS_MANUAL_D1		0xfd
#define EXT_PHY_TERM_RESIS_MANUAL_D0		0xfe

#endif /* __DW_HDMI_ROCKCHIP__ */
