/*
 * Copyright (C) 2006-2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING. If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Check MTD device read.
 *
 * Author: Adrian Hunter <ext-adrian.hunter@nokia.com>
 */

#ifndef __ZPL_BUILD__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/sched.h>
#else
#include "zplCompat.h"
#include "linux/mtd/mtd.h"
#endif /* __ZPL_BUILD__ */

#define PRINT_PREF KERN_INFO "mtd_readtest: "

static int dev;
module_param(dev, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");

static struct mtd_info *mtd;
static unsigned char *iobuf;
static unsigned char *iobuf1;
static unsigned char *bbt;

#ifdef __ZPL_BUILD__
#define START_OF_TEST_ERASEBLOCK        (36)
#define COUNT_OF_TEST_ERASEBLOCK        (100)
static unsigned char iobufSto[131072];
static unsigned char iobuf1Sto[131072];
static unsigned char bbtSto[1024];
#endif /* __ZPL_BUILD__ */

static int pgsize;
static int ebcnt;
static int pgcnt;

static int read_eraseblock_by_page(int ebnum)
{
	size_t read = 0;
	int i, ret, err = 0;
	u32 addr = ebnum * mtd->erasesize;
	void *buf = iobuf;
	void *oobbuf = iobuf1;

	for (i = 0; i < pgcnt; i++) {
		memset(buf, 0 , pgsize);
#ifndef __ZPL_BUILD__
		ret = mtd->read(mtd, addr, pgsize, &read, buf);
#else
		ret = mtd->_read(mtd, addr, pgsize, &read, buf);
#endif /* __ZPL_BUILD__ */
		if (ret == -EUCLEAN)
			ret = 0;
		if (ret || read != pgsize) {
			printk(PRINT_PREF "error: read failed at 0x%08x\n", addr);
			if (!err)
				err = ret;
			if (!err)
				err = -EINVAL;
		}
		if (mtd->oobsize) {
			struct mtd_oob_ops ops;

#ifndef __ZPL_BUILD__
			ops.mode      = MTD_OOB_PLACE;
#else
			ops.mode      = MTD_OPS_PLACE_OOB;
#endif /* __ZPL_BUILD__ */
			ops.len       = 0;
			ops.retlen    = 0;
			ops.ooblen    = mtd->oobsize;
			ops.oobretlen = 0;
			ops.ooboffs   = 0;
			ops.datbuf    = NULL;
			ops.oobbuf    = oobbuf;
#ifndef __ZPL_BUILD__
			ret = mtd->read_oob(mtd, addr, &ops);
#else
			ret = mtd->_read_oob(mtd, addr, &ops);
#endif /* __ZPL_BUILD__ */
			if (ret || ops.oobretlen != mtd->oobsize) {
				printk(PRINT_PREF "error: read oob failed at "
						  "%#llx\n", (long long)addr);
				if (!err)
					err = ret;
				if (!err)
					err = -EINVAL;
			}
			oobbuf += mtd->oobsize;
		}
		addr += pgsize;
		buf += pgsize;
	}

	return err;
}

static void dump_eraseblock(int ebnum)
{
	int i, j, n;
	char line[128];
	int pg, oob;

	printk(PRINT_PREF "dumping eraseblock %d\n", ebnum);
	n = mtd->erasesize;
	for (i = 0; i < n;) {
		char *p = line;

		p += sprintf(p, "%05x: ", i);
		for (j = 0; j < 32 && i < n; j++, i++)
			p += sprintf(p, "%02x", iobuf[i]);
		printk(KERN_CRIT "%s\n", line);
		cond_resched();
	}
	if (!mtd->oobsize)
		return;
	printk(PRINT_PREF "dumping oob from eraseblock %d\n", ebnum);
	n = mtd->oobsize;
	for (pg = 0, i = 0; pg < pgcnt; pg++)
		for (oob = 0; oob < n;) {
			char *p = line;

			p += sprintf(p, "%05x: ", i);
			for (j = 0; j < 32 && oob < n; j++, oob++, i++)
				p += sprintf(p, "%02x",
					     (unsigned int)iobuf1[i]);
			printk(KERN_CRIT "%s\n", line);
			cond_resched();
		}
}

static int is_block_bad(int ebnum)
{
	loff_t addr = ebnum * mtd->erasesize;
	int ret;

#ifndef __ZPL_BUILD__
	ret = mtd->block_isbad(mtd, addr);
#else
	ret = mtd->_block_isbad(mtd, addr);
#endif /* __ZPL_BUILD__ */
	if (ret)
		printk(PRINT_PREF "block %d is bad\n", ebnum);
	return ret;
}

static int scan_for_bad_eraseblocks(void)
{
	int i, bad = 0;

#ifndef __ZPL_BUILD__
	bbt = kmalloc(ebcnt, GFP_KERNEL);
	if (!bbt) {
		printk(PRINT_PREF "error: cannot allocate memory\n");
		return -ENOMEM;
	}
#else
	bbt = bbtSto;
#endif /* __ZPL_BUILD__ */
	memset(bbt, 0 , ebcnt);

	/* NOR flash does not implement block_isbad */
#ifndef __ZPL_BUILD__
	if (mtd->block_isbad == NULL)
#else
	if (mtd->_block_isbad == NULL)
#endif
		return 0;

	printk(PRINT_PREF "scanning for bad eraseblocks\n");
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
	printk(PRINT_PREF "scanned %d eraseblocks, %d are bad\n", i, bad);
	return 0;
}

#ifndef __ZPL_BUILD__
static int __init mtd_readtest_init(void)
#else
int mtd_readtest_init(void)
#endif /* __ZPL_BUILD__ */
{
	uint64_t tmp;
	int err, i;

	printk(KERN_INFO "\n");
	printk(KERN_INFO "=================================================\n");
	printk(PRINT_PREF "MTD device: %d\n", dev);

	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		printk(PRINT_PREF "error: Cannot get MTD device\n");
		return err;
	}

	if (mtd->writesize == 1) {
		printk(PRINT_PREF "not NAND flash, assume page size is 512 "
		       "bytes.\n");
		pgsize = 512;
	} else
		pgsize = mtd->writesize;

	tmp = mtd->size;
#ifndef __ZPL_BUILD__
	do_div(tmp, mtd->erasesize);
#else
	tmp = tmp / mtd->erasesize;
#endif /* __ZPL_BUILD__ */
	ebcnt = tmp;
	pgcnt = mtd->erasesize / pgsize;

	printk(PRINT_PREF "MTD device size %d, eraseblock size %d, "
	       "page size %d, count of eraseblocks %d, pages per "
	       "eraseblock %d, OOB size %d\n",
	       (unsigned long)mtd->size, mtd->erasesize,
	       pgsize, ebcnt, pgcnt, mtd->oobsize);

	err = -ENOMEM;
#ifndef __ZPL_BUILD__
	iobuf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!iobuf) {
		printk(PRINT_PREF "error: cannot allocate memory\n");
		goto out;
	}
	iobuf1 = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!iobuf1) {
		printk(PRINT_PREF "error: cannot allocate memory\n");
		goto out;
	}
#else
	iobuf = iobufSto;
	iobuf1 = iobuf1Sto;
#endif /* __ZPL_BUILD__ */

	err = scan_for_bad_eraseblocks();
	if (err)
		goto out;

	/* Read all eraseblocks 1 page at a time */
	printk(PRINT_PREF "testing page read\n");

	/*
	 * Starts from block 36 (u-boot-env partition)
	 * Note: partitions "bcb", "u-boot1", "u-boot2" doesn't use ECC bytes
	 */
	for (i = START_OF_TEST_ERASEBLOCK; i < (START_OF_TEST_ERASEBLOCK + COUNT_OF_TEST_ERASEBLOCK); ++i) {
		int ret;

		printk(PRINT_PREF "blk %d\n", i);
		if (bbt[i])
			continue;
		ret = read_eraseblock_by_page(i);
		if (ret) {
			dump_eraseblock(i);

			if (!err)
				err = ret;
		}
		cond_resched();
	}

	if (err)
		printk(PRINT_PREF "finished with errors\n");
	else
		printk(PRINT_PREF "finished\n");

out:

#ifndef __ZPL_BUILD__
	kfree(iobuf);
	kfree(iobuf1);
	kfree(bbt);
#endif /* __ZPL_BUILD__ */
	put_mtd_device(mtd);
	if (err)
		printk(PRINT_PREF "error %d occurred\n", err);
	printk(KERN_INFO "=================================================\n");
	return err;
}
module_init(mtd_readtest_init);

static void __exit mtd_readtest_exit(void)
{
	return;
}
module_exit(mtd_readtest_exit);

MODULE_DESCRIPTION("Read test module");
MODULE_AUTHOR("Adrian Hunter");
MODULE_LICENSE("GPL");
