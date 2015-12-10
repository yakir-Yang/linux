
#include <asm-generic/io.h>
#include <asm/cacheflush.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "rockchip_drm_rga.h"

#include <linux/vt_buffer.h>

struct rockchip_rga {
	struct drm_device	*drm_dev;
	struct device		*dev;
	struct regmap		*grf;
	void __iomem		*regs;
	void __iomem		*reg_buffer;

	struct clk		*sclk;
	struct clk		*aclk;
	struct clk		*hclk;
};

#define USE_RGA_MASTER	1

static void rga_write_buffer(struct rockchip_rga *rga, u32 reg, u32 value)
{
#ifdef USE_RGA_MASTER
	writel(value, rga->reg_buffer + (reg - 0x100));
#else
	writel(value, rga->regs + reg);
#endif
}

static void rga_write(struct rockchip_rga *rga, u32 reg, u32 value)
{
	writel(value, rga->regs + reg);
}

static u32 rga_read(struct rockchip_rga *rga, u32 reg)
{
	return readl(rga->regs + reg);
}

static void rga_mod(struct rockchip_rga *rga, u32 reg, u32 value, u32 mask)
{
	u32 temp = rga_read(rga, reg) & ~(mask);

	temp |= value & mask;
	rga_write(rga, reg, temp);
}

static void rockchip_rga_init(struct rockchip_rga *rga)
{
	/*
	 * Enable auto-reset after one frame finish,
	 * soft reset the core clock and aclk domain,
	 * enable auto clock gating,
	 * RGA work in slave command mode.
	 */
	rga_write(rga, RGA_SYS_CTRL, 0x6c);

	/*
	 * Enable  all command finished interrupt,
	 * and MMU interrupt, and error interrupt.
	 */
	rga_write(rga, RGA_INT, 0x700);

	/*
	 * Disbale command channel MMU.
	 */
	rga_write(rga, RGA_MMU_CTRL0, 0x00);
	rga_write(rga, RGA_MMU_CTRL1, 0x00);
}

/* RGA Bitflip Test Function */
static int rockchip_rga_bitflit_show(struct seq_file *s, void *data)
{
	struct rockchip_rga *rga = (struct rockchip_rga *)s->private;

	return 0;
}
static int rockchip_rga_bitflit_open(struct inode *inode, struct file *file)
{
	return single_open(file, rockchip_rga_bitflit_show, inode->i_private);
}

static ssize_t rockchip_rga_bitflit_write(struct file *file, const char __user *buf,
					   size_t count, loff_t *ppos)
{
	struct rockchip_rga *rga = ((struct seq_file *)file->private_data)->private;
	unsigned int width, height, src_mode, dst_mode;
	u16 *source, *dest;
	char kbuf[256];
	int ret, i;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	ret = sscanf(kbuf, "%d%d%d%d", &width, &height, &src_mode, &dst_mode);
	if (ret != 4) {
		dev_err(rga->dev, "invalid input format [%d]\n", ret);
		return -EINVAL;
	}

	dev_info(rga->dev, "size: %dx%d, mode: %d -> %d\n", width, height,
		 src_mode, dst_mode);

	if (width > 128 && height > 64) {
		dev_err(rga->dev, "Failed to alloc framebuffer\n");
		return -ENOMEM;
	}

	rockchip_rga_init(rga);

	/* Config bitflit mode, enable current frame finished interrupt */
	rga_write_buffer(rga, RGA_MODE_CTRL, 0x00);

	/* Alloc source framebuffer, and init them with [R3 G3 B3] */
	source = (u16 *)kmalloc(width * height * 2, GFP_KERNEL);
	scr_memsetw(source, 0x08e3, width * height * 2);
	dmac_flush_range(source, source + width * height * 2);
	outer_flush_range(virt_to_phys(source), virt_to_phys(source + width * height * 2));
	dev_info(rga->dev, "source framebuffer address = 0x%x\n", virt_to_phys(source));

	rga_write_buffer(rga, RGA_SRC_BASE0, virt_to_phys(source));
	rga_write_buffer(rga, RGA_SRC_BASE1, virt_to_phys(source) + width * height);
	rga_write_buffer(rga, RGA_SRC_BASE2, virt_to_phys(source) + width * height + width * height / 4);

	for (i = 0; i < width * height; i++) {
		if (i && (i % 16 == 0))
			printk("\t [%4x] \n", i);
		printk("0x%4x, ", source[i]);
	}

	/*
	 * Config source framebuffer information
	 *   width / height / format RGB565
	 */
	rga_write_buffer(rga, RGA_SRC_INFO, 0x02000014);
	rga_write_buffer(rga, RGA_SRC_VIR_INFO, (width - 1) & 0xf0);
	rga_write_buffer(rga, RGA_SRC_ACT_INFO, (height - 1) << 16 | (width - 1));
	rga_write_buffer(rga, RGA_SRC_X_FACTOR, 0);
	rga_write_buffer(rga, RGA_SRC_Y_FACTOR, 0);

	/* Alloc dest framebuffer */
	dest = kmalloc(width * height * 2, GFP_KERNEL);
	scr_memsetw(dest, 0x0000, width * height * 2);
	dmac_flush_range(dest, dest + width * height * 2);
	outer_flush_range(virt_to_phys(dest), virt_to_phys(dest + width * height * 2));

	rga_write_buffer(rga, RGA_DST_BASE0, virt_to_phys(dest));
	rga_write_buffer(rga, RGA_DST_BASE1, virt_to_phys(dest) + width * height);
	rga_write_buffer(rga, RGA_DST_BASE2, virt_to_phys(dest) + width * height + width * height / 4);

	dev_info(rga->dev, "dest framebuffer address = 0x%x\n", virt_to_phys(dest));

	/*
	 * Config dest framebuffer information
	 *   width / height / format RGB888
	 */
	rga_write_buffer(rga, RGA_DST_INFO, 0x04);
	rga_write_buffer(rga, RGA_DST_VIR_INFO, (width - 1) & 0xf0);
	rga_write_buffer(rga, RGA_DST_ACT_INFO, (height - 1) << 16 | (width - 1));

#ifdef USE_RGA_MASTER
	rga_write(rga, RGA_SYS_CTRL, 0x0);
	rga_write(rga, RGA_CMD_BASE, virt_to_phys(rga->reg_buffer));

	dmac_flush_range(rga->reg_buffer, rga->reg_buffer + 128);
	outer_flush_range(virt_to_phys(rga->reg_buffer), virt_to_phys(rga->reg_buffer + 128));

	rga_write(rga, RGA_SYS_CTRL, 0x66);
	rga_mod(rga, RGA_INT, 0x700, 0x700);
	rga_write(rga, RGA_CMD_CTRL, 0x1);
#else
	rga_mod(rga, RGA_SYS_CTRL, 0x1, 0x1);
#endif

	msleep(100);

	for (i = 0; i < 32; i++) {
		if (i && (i % 4 == 0))
			printk("\t [REG 0x1%2x] \n", i);
		printk("0x%08x, ", readl(rga->reg_buffer + i * 4));
	}


	for (i = 0; i < width * height; i++) {
		if (i && (i % 16 == 0))
			printk("\t [DEST %4x] \n", i);
		printk("0x%4x, ", dest[i]);
	}

	kfree(source);
	kfree(dest);

	return count;
}

/* RGA ColorFill Test Function */
static int rockchip_rga_colorfill_show(struct seq_file *s, void *data)
{
	struct rockchip_rga *rga = (struct rockchip_rga *)s->private;

	return 0;
}
static int rockchip_rga_colorfill_open(struct inode *inode, struct file *file)
{
	return single_open(file, rockchip_rga_colorfill_show, inode->i_private);
}


/* RGA Rotate Scale Function */
static int rockchip_rga_scale_show(struct seq_file *s, void *data)
{
	struct rockchip_rga *rga = (struct rockchip_rga *)s->private;

	return 0;
}
static int rockchip_rga_scale_open(struct inode *inode, struct file *file)
{
	return single_open(file, rockchip_rga_scale_show, inode->i_private);
}

/* RGA Rotate Test Function */
static int rockchip_rga_rotate_show(struct seq_file *s, void *data)
{
	struct rockchip_rga *rga = (struct rockchip_rga *)s->private;

	return 0;
}
static int rockchip_rga_rotate_open(struct inode *inode, struct file *file)
{
	return single_open(file, rockchip_rga_rotate_show, inode->i_private);
}

static const struct file_operations rockchip_rga_bitflit_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_rga_bitflit_open,
	.read = seq_read,
	.write = rockchip_rga_bitflit_write,
	.llseek = seq_lseek,
	.release = single_release,
};
static const struct file_operations rockchip_rga_colorfill_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_rga_colorfill_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static const struct file_operations rockchip_rga_scale_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_rga_scale_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static const struct file_operations rockchip_rga_rotate_fops = {
	.owner = THIS_MODULE,
	.open = rockchip_rga_rotate_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static irqreturn_t rockchip_rga_hardirq(int irq, void *dev_id)
{
	struct rockchip_rga *rga = dev_id;
	int intr;

	intr = rga_read(rga, RGA_INT) & 0xf;

	dev_info(rga->dev, "%s:%d interrupt flag = %x\n", __func__, __LINE__,
		 intr);

	rga_mod(rga, RGA_INT, intr << 4, 0xf << 4);

	return IRQ_HANDLED;
}

static irqreturn_t rockchip_rga_irq(int irq, void *dev_id)
{
	struct rockchip_rga *rga = dev_id;
	int intr;

	intr = rga_read(rga, RGA_INT) & 0xf;

	dev_info(rga->dev, "%s:%d interrupt flag = %x\n", __func__, __LINE__,
		 intr);

	rga_mod(rga, RGA_INT, intr << 4, 0xf << 4);

	return IRQ_HANDLED;
}

static int rockchip_rga_parse_dt(struct rockchip_rga *rga)
{
	struct reset_control *sclk_rst, *aclk_rst, *hclk_rst;
	int ret;

	sclk_rst = devm_reset_control_get(rga->dev, "sclk");
	if (IS_ERR(sclk_rst)) {
		dev_err(rga->dev, "failed to get sclk reset controller\n");
		return PTR_ERR(sclk_rst);
	}

	aclk_rst = devm_reset_control_get(rga->dev, "aclk");
	if (IS_ERR(aclk_rst)) {
		dev_err(rga->dev, "failed to get aclk reset controller\n");
		return PTR_ERR(aclk_rst);
	}

	hclk_rst = devm_reset_control_get(rga->dev, "hclk");
	if (IS_ERR(hclk_rst)) {
		dev_err(rga->dev, "failed to get hclk reset controller\n");
		return PTR_ERR(hclk_rst);
	}

	reset_control_assert(sclk_rst);
	usleep_range(10, 20);
	reset_control_deassert(sclk_rst);

	reset_control_assert(aclk_rst);
	usleep_range(10, 20);
	reset_control_deassert(aclk_rst);

	reset_control_assert(hclk_rst);
	usleep_range(10, 20);
	reset_control_deassert(hclk_rst);

	rga->sclk = devm_clk_get(rga->dev, "sclk");
	if (IS_ERR(rga->sclk)) {
		dev_err(rga->dev, "failed to get sclk clock\n");
		return PTR_ERR(rga->sclk);
	}

	rga->aclk = devm_clk_get(rga->dev, "aclk");
	if (IS_ERR(rga->aclk)) {
		dev_err(rga->dev, "failed to get aclk clock\n");
		return PTR_ERR(rga->aclk);
	}

	rga->hclk = devm_clk_get(rga->dev, "hclk");
	if (IS_ERR(rga->hclk)) {
		dev_err(rga->dev, "failed to get hclk clock\n");
		return PTR_ERR(rga->hclk);
	}

	ret = clk_prepare_enable(rga->sclk);
	if (ret) {
		dev_err(rga->dev, "Cannot enable rga sclk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(rga->aclk);
	if (ret) {
		dev_err(rga->dev, "Cannot enable rga aclk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(rga->hclk);
	if (ret) {
		dev_err(rga->dev, "Cannot enable rga hclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id rockchip_rga_dt_ids[] = {
	{ .compatible = "rockchip,rk3228-rga",
	},
	{},
};
MODULE_DEVICE_TABLE(of, rockchip_rga_dt_ids);

static int rockchip_rga_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct dentry *debugfs_root;
	struct rockchip_rga *rga;
	struct resource *iores;
	struct dentry *ent;
	int irq;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	rga = devm_kzalloc(&pdev->dev, sizeof(*rga), GFP_KERNEL);
	if (!rga)
		return -ENOMEM;

	match = of_match_node(rockchip_rga_dt_ids, pdev->dev.of_node);
	rga->dev = &pdev->dev;

	ret = rockchip_rga_parse_dt(rga);
	if (ret) {
		dev_err(rga->dev, "Unable to parse OF data\n");
		return ret;
	}

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores)
		return -ENXIO;

	rga->regs = devm_ioremap_resource(rga->dev, iores);
	if (IS_ERR(rga->regs))
		return PTR_ERR(rga->regs);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(rga->dev, irq, rockchip_rga_hardirq,
					rockchip_rga_irq, 0,
					dev_name(rga->dev), rga);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, rga);
	
	debugfs_root = debugfs_create_dir("rga", NULL);
	if (!debugfs_root) {
		dev_err(rga->dev, "failed to create rga debugfs dir\n");
		return PTR_ERR(debugfs_root);
	}

	ent = debugfs_create_file("bitfilt", S_IFREG | S_IRUGO, debugfs_root,
				  rga, &rockchip_rga_bitflit_fops);
	if (!ent) {
		dev_err(rga->dev, "Cannot create /sys/kernel/debug/bitflit\n");
		return PTR_ERR(ent);
	}

	ent = debugfs_create_file("colorfill", S_IFREG | S_IRUGO, debugfs_root,
				  rga, &rockchip_rga_colorfill_fops);
	if (!ent) {
		dev_err(rga->dev, "Cannot create /sys/kernel/debug/colorfill\n");
		return PTR_ERR(ent);
	}

	ent = debugfs_create_file("scale", S_IFREG | S_IRUGO, debugfs_root,
				  rga, &rockchip_rga_scale_fops);
	if (!ent) {
		dev_err(rga->dev, "Cannot create /sys/kernel/debug/scale\n");
		return PTR_ERR(ent);
	}

	ent = debugfs_create_file("rotate", S_IFREG | S_IRUGO, debugfs_root,
				  rga, &rockchip_rga_rotate_fops);
	if (!ent) {
		dev_err(rga->dev, "Cannot create /sys/kernel/debug/rotate\n");
		return PTR_ERR(ent);
	}

	dev_info(rga->dev, "RGA version info: %x.%x.%x\n",
		 (rga_read(rga, RGA_VERSION_INFO) >> 24) & 0xFF,
		 (rga_read(rga, RGA_VERSION_INFO) >> 20) & 0x0F,
		 rga_read(rga, RGA_VERSION_INFO) & 0xFFFFF);

	rga->reg_buffer = kmalloc(32 * 4, GFP_KERNEL);
	memset(rga->reg_buffer, 0x00, 32 * 4);

	return 0;
}

static int rockchip_rga_remove(struct platform_device *pdev)
{
	return 0;
}

static int rockchip_rga_suspend(struct device *dev)
{
	return 0;
}

static int rockchip_rga_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops rockchip_rga_pm = {
	.resume_early = rockchip_rga_resume,
	.suspend_late = rockchip_rga_suspend,
};

static struct platform_driver rockchip_rga_pltfm_driver = {
	.probe  = rockchip_rga_probe,
	.remove = rockchip_rga_remove,
	.driver = {
		.name = "rockchip-rga",
		.pm = &rockchip_rga_pm,
		.of_match_table = rockchip_rga_dt_ids,
	},
};

module_platform_driver(rockchip_rga_pltfm_driver);

MODULE_AUTHOR("Yakir Yang <ykk@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RGA Driver Extension");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rockchip-rga");
