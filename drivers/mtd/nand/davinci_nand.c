/*
 * linux/drivers/mtd/nand/davinci_nand.c
 *
 * NAND Flash Driver
 *
 * Copyright (C) 2006 Texas Instruments.
 *
 * ported to 2.6.23 (C) 2008 by
 * Sander Huijsen <Shuijsen@optelecom-nkf.com>
 * Troy Kisky <troy.kisky@boundarydevices.com>
 * Dirk Behme <Dirk.Behme@gmail.com>
 *
 * --------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * --------------------------------------------------------------------------
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   DaVinci board which utilizes the Samsung k9k2g08 part.
 *
 *  Modifications:
 *  ver. 1.0: Feb 2005, Vinod/Sudhakar
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <asm/arch/hardware.h>
#include <asm/arch/nand.h>
#include <asm/arch/mux.h>

#include <asm/mach/flash.h>

#ifdef CONFIG_NAND_FLASH_HW_ECC

#ifdef CONFIG_MACH_NTOSD_644XA
#define DAVINCI_NAND_ECC_MODE NAND_ECC_HW16_2048
#else
#define DAVINCI_NAND_ECC_MODE NAND_ECC_HW3_512
#endif

#else
#define DAVINCI_NAND_ECC_MODE NAND_ECC_SOFT
#endif

#define DRIVER_NAME "davinci_nand"

static struct clk *nand_clock;
static void __iomem *nand_vaddr;

/*
 * MTD structure for DaVinici board
 */
static struct mtd_info *nand_davinci_mtd;

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

/* BB marker is byte 5 in OOB of page 0 */
static struct nand_bbt_descr davinci_memorybased_small = {
	.options = NAND_BBT_SCAN2NDPAGE,
	.offs = 5,
	.len = 1,
	.pattern = scan_ff_pattern
};

/* BB marker is bytes 0-1 in OOB of page 0 */
static struct nand_bbt_descr davinci_memorybased_large = {
	.options = 0,
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern
};

#ifdef CONFIG_MACH_NTOSD_644XA
static struct nand_ecclayout davinci_nand_oob_16 = {
	.eccbytes = 4,
	.eccpos = {0, 1, 2, 3},
	.oobfree = { {.offset = 8, .length = 8} }
};

static struct nand_ecclayout davinci_nand_oob_64 = {
	.eccbytes = 16,
	.eccpos = {8, 9, 10, 11, 24, 25, 26, 27, 40, 41, 42, 43, 56, 57, 58, 59},
	.oobfree = { {.offset =  2, .length = 6},
		     {.offset = 12, .length = 12},
		     {.offset = 28, .length = 12},
		     {.offset = 44, .length = 12},
		     {.offset = 60, .length =  4} }
};
#endif

inline unsigned int davinci_nand_readl(int offset)
{
	return davinci_readl(DAVINCI_ASYNC_EMIF_CNTRL_BASE + offset);
}

inline void davinci_nand_writel(unsigned long value, int offset)
{
	davinci_writel(value, DAVINCI_ASYNC_EMIF_CNTRL_BASE + offset);
}

/*
 * Hardware specific access to control-lines
 */
static void nand_davinci_hwcontrol(struct mtd_info *mtd, int cmd,
				   unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;
	u32 IO_ADDR_W = (u32)chip->IO_ADDR_W;

	/* Did the control lines change? */
	if (ctrl & NAND_CTRL_CHANGE) {
		IO_ADDR_W &= ~(MASK_ALE|MASK_CLE);

		if ((ctrl & NAND_CTRL_CLE) == NAND_CTRL_CLE)
			IO_ADDR_W |= MASK_CLE;
		else if ((ctrl & NAND_CTRL_ALE) == NAND_CTRL_ALE)
			IO_ADDR_W |= MASK_ALE;

		chip->IO_ADDR_W = (void __iomem *)IO_ADDR_W;
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->IO_ADDR_W);
}

static void nand_davinci_select_chip(struct mtd_info *mtd, int chip)
{
	/* do nothing */
}

#ifdef CONFIG_NAND_FLASH_HW_ECC
static void nand_davinci_enable_hwecc(struct mtd_info *mtd, int mode)
{
	u32 retval;

	/* Reset ECC hardware */
	retval = davinci_nand_readl(NANDF1ECC_OFFSET);

	/* Restart ECC hardware */
	retval = davinci_nand_readl(NANDFCR_OFFSET);
	retval |= (1 << 8);
	davinci_nand_writel(retval, NANDFCR_OFFSET);
}

/*
 * Read DaVinci ECC register
 */
static u32 nand_davinci_readecc(struct mtd_info *mtd)
{
	/* Read register ECC and clear it */
	return davinci_nand_readl(NANDF1ECC_OFFSET);
}

#ifdef CONFIG_MACH_NTOSD_644XA
/*
 * Read DaVinci ECC registers and rework into MTD format
 */
static int nand_davinci_calculate_ecc(struct mtd_info *mtd,
				      const u_char *dat, u_char *ecc_code)
{
	unsigned int ecc_val = nand_davinci_readecc(mtd);
	unsigned int tmp;

	/* invert so that erased block ecc is correct */
	tmp = ecc_val;
	if(tmp == 0) tmp = ~tmp;
	ecc_code[0] = (u_char)(tmp >> 24);
	ecc_code[1] = (u_char)(tmp >> 16);
	ecc_code[2] = (u_char)(tmp >> 8);
	ecc_code[3] = (u_char)(tmp);
	return 0;
}

static int nand_davinci_correct_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
	struct nand_chip *chip = mtd->priv;
	u_int32_t eccNand = (read_ecc[0] << 24) | (read_ecc[1] << 16) |
			    (read_ecc[2] << 8)  | (read_ecc[3]);
	u_int32_t eccCalc = (calc_ecc[0] << 24) | (calc_ecc[1] << 16) |
			    (calc_ecc[2] << 8)  | (calc_ecc[3]);
	u_int32_t diff;

	diff = eccCalc ^ eccNand;
	if (diff) {
		if ((((diff>>16)^diff) & 0xffff) == 0xffff) {
			/* Correctable error */
			if ((diff>>(16+4)) < chip->ecc.size) {
				dat[diff>>(16+4)] ^= (1 << ((diff>>16)&7));
				return 1;
			} else {
				return -1;
			}
		} else if (!(diff & (diff-1))) {
			/* Single bit ECC error in the ECC itself,
			   nothing to fix */
			return 1;
		} else {
			/* Uncorrectable error */
			return -1;
		}
	}
	return 0;
}
#else /* not NTOSD_644XA */
/*
 * Read DaVinci ECC registers and rework into MTD format
 */
static int nand_davinci_calculate_ecc(struct mtd_info *mtd,
				      const u_char *dat, u_char *ecc_code)
{
	unsigned int ecc_val = nand_davinci_readecc(mtd);
	/* squeeze 0 middle bits out so that it fits in 3 bytes */
	unsigned int tmp = (ecc_val&0x0fff)|((ecc_val&0x0fff0000)>>4);
	/* invert so that erased block ecc is correct */
	tmp = ~tmp;
	ecc_code[0] = (u_char)(tmp);
	ecc_code[1] = (u_char)(tmp >> 8);
	ecc_code[2] = (u_char)(tmp >> 16);

	return 0;
}

static int nand_davinci_correct_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
	struct nand_chip *chip = mtd->priv;
	u_int32_t eccNand = read_ecc[0] | (read_ecc[1] << 8) |
					  (read_ecc[2] << 16);
	u_int32_t eccCalc = calc_ecc[0] | (calc_ecc[1] << 8) |
					  (calc_ecc[2] << 16);
	u_int32_t diff = eccCalc ^ eccNand;

	if (diff) {
		if ((((diff>>12)^diff) & 0xfff) == 0xfff) {
			/* Correctable error */
			if ((diff>>(12+3)) < chip->ecc.size) {
				dat[diff>>(12+3)] ^= (1 << ((diff>>12)&7));
				return 1;
			} else {
				return -1;
			}
		} else if (!(diff & (diff-1))) {
			/* Single bit ECC error in the ECC itself,
			   nothing to fix */
			return 1;
		} else {
			/* Uncorrectable error */
			return -1;
		}

	}
	return 0;
}
#endif /* CONFIG_MACH_NTOSD_644XA */
#endif

/*
 * Read OOB data from flash.
 */
static int read_oob_and_check(struct mtd_info *mtd, loff_t offs, uint8_t *buf,
			      struct nand_bbt_descr *bd)
{
	int i, ret;
	int page;
	struct nand_chip *chip = mtd->priv;

	/* Calculate page address from offset */
	page = (int)(offs >> chip->page_shift);
	page &= chip->pagemask;

	/* Read OOB data from flash */
	ret = chip->ecc.read_oob(mtd, chip, page, 1);
	if (ret < 0)
		return ret;

	/* Copy read OOB data to the buffer*/
	memcpy(buf, chip->oob_poi, mtd->oobsize);

	/* Check pattern against BBM in OOB area */
	for (i = 0; i < bd->len; i++) {
		if (buf[bd->offs + i] != bd->pattern[i])
			return 1;
	}
	return 0;
}

/*
 * Fill in the memory based Bad Block Table (BBT).
 */
static int nand_davinci_memory_bbt(struct mtd_info *mtd,
				   struct nand_bbt_descr *bd)
{
	int i, numblocks;
	int startblock = 0;
	loff_t from = 0;
	struct nand_chip *chip = mtd->priv;
	int blocksize = 1 << chip->bbt_erase_shift;
	uint8_t *buf = chip->buffers->databuf;
	int len = bd->options & NAND_BBT_SCAN2NDPAGE ? 2 : 1;

	/* -numblocks- is 2 times the actual number of eraseblocks */
	numblocks = mtd->size >> (chip->bbt_erase_shift - 1);

	/* Now loop through all eraseblocks in the flash */
	for (i = startblock; i < numblocks; i += 2) {
		int j, ret;
		int offs = from;

		/* If NAND_BBT_SCAN2NDPAGE flag is set in bd->options,
		 * also each 2nd page of an eraseblock is checked
		 * for a Bad Block Marker. In that case, len equals 2.
		 */
		for (j = 0; j < len; j++) {
			/* Read OOB data and check pattern */
			ret = read_oob_and_check(mtd, from, buf, bd);
			if (ret < 0)
				return ret;

			/* Check pattern for bad block markers */
			if (ret) {
				/* Mark bad block by writing 0b11 in the
				   table */
				chip->bbt[i >> 3] |= 0x03 << (i & 0x6);

				printk(KERN_WARNING "Bad eraseblock %d at " \
						    "0x%08x\n", i >> 1,
						     (unsigned int)from);

				mtd->ecc_stats.badblocks++;
				break;
			}
			offs += mtd->writesize;
		}

		/* Make -from- point to next eraseblock */
		from += blocksize;
	}

	printk(KERN_NOTICE "Bad block scan: %d out of %d blocks are bad.\n",
			    mtd->ecc_stats.badblocks, numblocks>>1);

	return 0;
}

/*
 * This function creates a memory based bad block table (BBT).
 * It is largely based on the standard BBT function, but all
 * unnecessary junk is thrown out to speed up.
 */
static int nand_davinci_scan_bbt(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_bbt_descr *bd;
	int len, ret = 0;

	chip->bbt_td = NULL;
	chip->bbt_md = NULL;

	/* pagesize determines location of BBM */
	if (mtd->writesize > 512)
		bd = &davinci_memorybased_large;
	else
		bd = &davinci_memorybased_small;

	chip->badblock_pattern = bd;

	/* Use 2 bits per page meaning 4 page markers per byte */
	len = mtd->size >> (chip->bbt_erase_shift + 2);

	/* Allocate memory (2bit per block) and clear the memory bad block
	   table */
	chip->bbt = kzalloc(len, GFP_KERNEL);
	if (!chip->bbt) {
		printk(KERN_ERR "nand_davinci_scan_bbt: Out of memory\n");
		return -ENOMEM;
	}

	/* Now try to fill in the BBT */
	ret = nand_davinci_memory_bbt(mtd, bd);
	if (ret) {
		printk(KERN_ERR "nand_davinci_scan_bbt: "
		       "Can't scan flash and build the RAM-based BBT\n");

		kfree(chip->bbt);
		chip->bbt = NULL;
	}

	return ret;
}

/*
 * Read from memory register: we can read 4 bytes at a time.
 * The hardware takes care of actually reading the NAND flash.
 */
static void nand_davinci_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	int num_words = len >> 2;
	u32 *p = (u32 *)buf;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < num_words; i++)
		p[i] = readl(chip->IO_ADDR_R);
}

/*
 * Check hardware register for wait status. Returns 1 if device is ready,
 * 0 if it is still busy.
 */
static int nand_davinci_dev_ready(struct mtd_info *mtd)
{
	return (davinci_nand_readl(NANDFSR_OFFSET) & NAND_BUSY_FLAG);
}

static void nand_davinci_set_eccsize(struct nand_chip *chip)
{
	chip->ecc.size = 256;

#ifdef CONFIG_NAND_FLASH_HW_ECC
	switch (chip->ecc.mode) {
	case NAND_ECC_HW12_2048:
	case NAND_ECC_HW16_2048:
		chip->ecc.size = 2048;
		break;

	case NAND_ECC_HW3_512:
	case NAND_ECC_HW4_512:
	case NAND_ECC_HW6_512:
	case NAND_ECC_HW8_512:
	chip->ecc.size = 512;
		break;

	case NAND_ECC_HW3_256:
	default:
		/* do nothing */
		break;
	}
#endif
}

static void nand_davinci_set_eccbytes(struct nand_chip *chip)
{
	chip->ecc.bytes = 3;

#ifdef CONFIG_NAND_FLASH_HW_ECC
	switch (chip->ecc.mode) {
	case NAND_ECC_HW16_2048:
		chip->ecc.bytes += 4;
	case NAND_ECC_HW12_2048:
		chip->ecc.bytes += 4;
	case NAND_ECC_HW8_512:
		chip->ecc.bytes += 2;
	case NAND_ECC_HW6_512:
		chip->ecc.bytes += 2;
	case NAND_ECC_HW4_512:
		chip->ecc.bytes += 1;	     
	case NAND_ECC_HW3_512:
	case NAND_ECC_HW3_256:
	default:
		/* do nothing */
		break;
	}
#endif
}

static void __devinit nand_davinci_flash_init(void)
{
	u32 regval, tmp;

	/* Check for correct pin mux, reconfigure if necessary */
	tmp = davinci_readl(DAVINCI_SYSTEM_MODULE_BASE + PINMUX0);

	if ((tmp & 0x20020C1F) != 0x00000C1F) {
		/* Disable HPI and ATA mux */
		davinci_mux_peripheral(DAVINCI_MUX_HPIEN, 0);
		davinci_mux_peripheral(DAVINCI_MUX_ATAEN, 0);
#ifndef CONFIG_MACH_NTOSD_644XA
		/* Enable VLYNQ and AEAW */
		davinci_mux_peripheral(DAVINCI_MUX_AEAW0, 1);
		davinci_mux_peripheral(DAVINCI_MUX_AEAW1, 1);
		davinci_mux_peripheral(DAVINCI_MUX_AEAW2, 1);
		davinci_mux_peripheral(DAVINCI_MUX_AEAW3, 1);
		davinci_mux_peripheral(DAVINCI_MUX_AEAW4, 1);
		davinci_mux_peripheral(DAVINCI_MUX_VLSCREN, 1);
		davinci_mux_peripheral(DAVINCI_MUX_VLYNQEN, 1);
#endif
		regval = davinci_readl(DAVINCI_SYSTEM_MODULE_BASE + PINMUX0);

		printk(KERN_WARNING "Warning: MUX config for NAND: Set " \
		       "PINMUX0 reg to 0x%08x, was 0x%08x, should be done " \
		       "by bootloader.\n", regval, tmp);
	}

	regval = davinci_nand_readl(AWCCR_OFFSET);
	regval |= 0x10000000;
	davinci_nand_writel(regval, AWCCR_OFFSET);

	/*------------------------------------------------------------------*
	 *  NAND FLASH CHIP TIMEOUT @ 459 MHz                               *
	 *                                                                  *
	 *  AEMIF.CLK freq   = PLL1/6 = 459/6 = 76.5 MHz                    *
	 *  AEMIF.CLK period = 1/76.5 MHz = 13.1 ns                         *
	 *                                                                  *
	 *------------------------------------------------------------------*/
	regval = 0
		| (0 << 31)           /* selectStrobe */
		| (0 << 30)           /* extWait */
		| (1 << 26)           /* writeSetup      10 ns */
		| (3 << 20)           /* writeStrobe     40 ns */
		| (1 << 17)           /* writeHold       10 ns */
		| (0 << 13)           /* readSetup       10 ns */
		| (3 << 7)            /* readStrobe      60 ns */
		| (0 << 4)            /* readHold        10 ns */
		| (3 << 2)            /* turnAround      ?? ns */
		| (0 << 0)            /* asyncSize       8-bit bus */
		;
	tmp = davinci_nand_readl(A1CR_OFFSET);
	if (tmp != regval) {
		printk(KERN_WARNING "Warning: NAND config: Set A1CR " \
		       "reg to 0x%08x, was 0x%08x, should be done by " \
		       "bootloader.\n", regval, tmp);
		davinci_nand_writel(regval, A1CR_OFFSET); /* 0x0434018C */
	}

	davinci_nand_writel(0x00000101, NANDFCR_OFFSET);
}

/*
 * Main initialization routine
 */
int __devinit nand_davinci_probe(struct platform_device *pdev)
{
	struct nand_platform_data *pdata = pdev->dev.platform_data;
	struct resource		  *res = pdev->resource;
	struct nand_chip     	  *chip;
	struct device        	  *dev = NULL;
	u32                  	  nand_rev_code;
#ifdef CONFIG_MTD_CMDLINE_PARTS
	char                 	  *master_name;
	int 		     	  mtd_parts_nb = 0;
	struct mtd_partition 	  *mtd_parts = 0;
#endif

	nand_clock = clk_get(dev, "AEMIFCLK");
	if (IS_ERR(nand_clock)) {
		printk(KERN_ERR "Error %ld getting AEMIFCLK clock?\n",
		       PTR_ERR(nand_clock));
		return -1;
	}

	clk_enable(nand_clock);

	/* Allocate memory for MTD device structure and private data */
	nand_davinci_mtd = kmalloc(sizeof(struct mtd_info) +
				   sizeof(struct nand_chip), GFP_KERNEL);

	if (!nand_davinci_mtd) {
		printk(KERN_ERR "Unable to allocate davinci NAND MTD device " \
		       "structure.\n");
		clk_disable(nand_clock);
		return -ENOMEM;
	}

	/* Get pointer to private data */
	chip = (struct nand_chip *) (&nand_davinci_mtd[1]);

	/* Initialize structures */
	memset((char *)nand_davinci_mtd, 0, sizeof(struct mtd_info));
	memset((char *)chip, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	nand_davinci_mtd->priv = chip;

	nand_rev_code = davinci_nand_readl(NRCSR_OFFSET);

	printk("DaVinci NAND Controller rev. %d.%d\n",
	       (nand_rev_code >> 8) & 0xff, nand_rev_code & 0xff);

	nand_vaddr = ioremap(res->start, res->end - res->start);
	if (nand_vaddr == NULL) {
		printk(KERN_ERR "DaVinci NAND: ioremap failed.\n");
		clk_disable(nand_clock);
		kfree(nand_davinci_mtd);
		return -ENOMEM;
	}

	chip->IO_ADDR_R   = (void __iomem *)nand_vaddr;
	chip->IO_ADDR_W   = (void __iomem *)nand_vaddr;
	chip->chip_delay  = 0;
	chip->select_chip = nand_davinci_select_chip;
	chip->options     = 0;
	chip->ecc.mode	  = DAVINCI_NAND_ECC_MODE;

	/* Set ECC size and bytes */
	nand_davinci_set_eccsize(chip);
	nand_davinci_set_eccbytes(chip);

	/* Set address of hardware control function */
	chip->cmd_ctrl  = nand_davinci_hwcontrol;
	chip->dev_ready = nand_davinci_dev_ready;

#ifdef CONFIG_NAND_FLASH_HW_ECC
#ifdef CONFIG_MACH_NTOSD_644XA
	if(chip->ecc.size > 512)
	     chip->ecc.layout = &davinci_nand_oob_64;
	else 
	     chip->ecc.layout = &davinci_nand_oob_16;
#endif
	chip->ecc.calculate = nand_davinci_calculate_ecc;
	chip->ecc.correct   = nand_davinci_correct_data;
	chip->ecc.hwctl     = nand_davinci_enable_hwecc;
#endif

	/* Speed up the read buffer */
	chip->read_buf      = nand_davinci_read_buf;

	/* Speed up the creation of the bad block table */
	chip->scan_bbt      = nand_davinci_scan_bbt;

	nand_davinci_flash_init();

	nand_davinci_mtd->owner = THIS_MODULE;

	/* Scan to find existence of the device */
	if (nand_scan(nand_davinci_mtd, 1)) {
		printk(KERN_ERR "Chip Select is not set for NAND\n");
		clk_disable(nand_clock);
		kfree(nand_davinci_mtd);
		return -ENXIO;
	}

	/* Register the partitions */
	add_mtd_partitions(nand_davinci_mtd, pdata->parts, pdata->nr_parts);

#ifdef CONFIG_MTD_CMDLINE_PARTS
	/* Set nand_davinci_mtd->name = 0 temporarily */
	master_name = nand_davinci_mtd->name;
	nand_davinci_mtd->name = (char *)0;

	/* nand_davinci_mtd->name == 0, means: don't bother checking
	   <mtd-id> */
	mtd_parts_nb = parse_mtd_partitions(nand_davinci_mtd, part_probes,
					    &mtd_parts, 0);

	/* Restore nand_davinci_mtd->name */
	nand_davinci_mtd->name = master_name;

	add_mtd_partitions(nand_davinci_mtd, mtd_parts, mtd_parts_nb);
#endif

	return 0;
}

/*
 * Clean up routine
 */
static int nand_davinci_remove(struct platform_device *pdev)
{
	clk_disable(nand_clock);

	if (nand_vaddr)
		iounmap(nand_vaddr);

	/* Release resources, unregister device */
	nand_release(nand_davinci_mtd);

	/* Free the MTD device structure */
	kfree(nand_davinci_mtd);

	return 0;
}


static struct platform_driver nand_davinci_driver = {
	.probe		= nand_davinci_probe,
	.remove		= nand_davinci_remove,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

static int __init nand_davinci_init(void)
{
	return platform_driver_register(&nand_davinci_driver);
}
module_init(nand_davinci_init);

#ifdef MODULE
static void __exit nand_davinci_exit(void)
{
	platform_driver_unregister(&nand_davinci_driver);
}
module_exit(nand_davinci_exit);
#endif

MODULE_ALIAS(DRIVER_NAME);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on davinci" \
		   "board");
