/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * (C) Copyright 2008-2010
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef __ZPL_BUILD__
#include <common.h>
#include <memalign.h>
#include "ubifs.h"
#include <u-boot/zlib.h>

#include <linux/err.h>
#include <linux/lzo.h>

DECLARE_GLOBAL_DATA_PTR;
#else /* __ZPL_BUILD__ */
#include "zplCompat.h"
#include "memalign.h"
#include "ubifs.h"
#include "linux/lzo.h"
#endif /* __ZPL_BUILD__ */

/* compress.c */

/*
 * We need a wrapper for zunzip() because the parameters are
 * incompatible with the lzo decompressor.
 */
static int gzip_decompress(const unsigned char *in, size_t in_len,
			   unsigned char *out, size_t *out_len)
{
	return zunzip(out, *out_len, (unsigned char *)in,
		      (unsigned long *)out_len, 0, 0);
}

/* Fake description object for the "none" compressor */
static struct ubifs_compressor none_compr = {
	.compr_type = UBIFS_COMPR_NONE,
	.name = "none",
	.capi_name = "",
	.decompress = NULL,
};

static struct ubifs_compressor lzo_compr = {
	.compr_type = UBIFS_COMPR_LZO,
#ifndef __UBOOT__
	.comp_mutex = &lzo_mutex,
#endif
	.name = "lzo",
	.capi_name = "lzo",
	.decompress = lzo1x_decompress_safe,
};

static struct ubifs_compressor zlib_compr = {
	.compr_type = UBIFS_COMPR_ZLIB,
#ifndef __UBOOT__
	.comp_mutex = &deflate_mutex,
	.decomp_mutex = &inflate_mutex,
#endif
	.name = "zlib",
	.capi_name = "deflate",
	.decompress = gzip_decompress,
};

/* All UBIFS compressors */
struct ubifs_compressor *ubifs_compressors[UBIFS_COMPR_TYPES_CNT];


#ifdef __UBOOT__
/* from mm/util.c */

/**
 * kmemdup - duplicate region of memory
 *
 * @src: memory region to duplicate
 * @len: memory region length
 * @gfp: GFP mask to use
 */
void *kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *p;

	p = kmalloc(len, gfp);
	if (p)
		memcpy(p, src, len);
	return p;
}

struct crypto_comp {
	int compressor;
};

static inline struct crypto_comp
*crypto_alloc_comp(const char *alg_name, u32 type, u32 mask)
{
	struct ubifs_compressor *comp;
	struct crypto_comp *ptr;
	int i = 0;

	ptr = malloc_cache_aligned(sizeof(struct crypto_comp));
	while (i < UBIFS_COMPR_TYPES_CNT) {
		comp = ubifs_compressors[i];
		if (!comp) {
			i++;
			continue;
		}
		if (strncmp(alg_name, comp->capi_name, strlen(alg_name)) == 0) {
			ptr->compressor = i;
			return ptr;
		}
		i++;
	}
	if (i >= UBIFS_COMPR_TYPES_CNT) {
		dbg_gen("invalid compression type %s", alg_name);
		free (ptr);
		return NULL;
	}
	return ptr;
}
static inline int
crypto_comp_decompress(const struct ubifs_info *c, struct crypto_comp *tfm,
		       const u8 *src, unsigned int slen, u8 *dst,
		       unsigned int *dlen)
{
	struct ubifs_compressor *compr = ubifs_compressors[tfm->compressor];
	int err;

	if (compr->compr_type == UBIFS_COMPR_NONE) {
		memcpy(dst, src, slen);
		*dlen = slen;
		return 0;
	}

	err = compr->decompress(src, slen, dst, (size_t *)dlen);
	if (err)
		ubifs_err(c, "cannot decompress %d bytes, compressor %s, "
			  "error %d", slen, compr->name, err);

	return err;

	return 0;
}

/* from shrinker.c */

/* Global clean znode counter (for all mounted UBIFS instances) */
atomic_long_t ubifs_clean_zn_cnt;

#endif

/**
 * ubifs_decompress - decompress data.
 * @in_buf: data to decompress
 * @in_len: length of the data to decompress
 * @out_buf: output buffer where decompressed data should
 * @out_len: output length is returned here
 * @compr_type: type of compression
 *
 * This function decompresses data from buffer @in_buf into buffer @out_buf.
 * The length of the uncompressed data is returned in @out_len. This functions
 * returns %0 on success or a negative error code on failure.
 */
int ubifs_decompress(const struct ubifs_info *c, const void *in_buf,
		     int in_len, void *out_buf, int *out_len, int compr_type)
{
	int err;
	struct ubifs_compressor *compr;

	if (unlikely(compr_type < 0 || compr_type >= UBIFS_COMPR_TYPES_CNT)) {
		ubifs_err(c, "invalid compression type %d", compr_type);
		return -EINVAL;
	}

	compr = ubifs_compressors[compr_type];

	if (unlikely(!compr->capi_name)) {
		ubifs_err(c, "%s compression is not compiled in", compr->name);
		return -EINVAL;
	}

	if (compr_type == UBIFS_COMPR_NONE) {
		memcpy(out_buf, in_buf, in_len);
		*out_len = in_len;
		return 0;
	}

	if (compr->decomp_mutex)
		mutex_lock(compr->decomp_mutex);
	err = crypto_comp_decompress(c, compr->cc, in_buf, in_len, out_buf,
				     (unsigned int *)out_len);
	if (compr->decomp_mutex)
		mutex_unlock(compr->decomp_mutex);
	if (err)
		ubifs_err(c, "cannot decompress %d bytes, compressor %s,"
			  " error %d", in_len, compr->name, err);

	return err;
}

void ubifs_compress(const struct ubifs_info *c, const void *in_buf,
		    int in_len, void *out_buf, int *out_len, int *compr_type)
{
	int err;
	struct ubifs_compressor *compr = ubifs_compressors[*compr_type];

	if (*compr_type == UBIFS_COMPR_NONE)
		goto no_compr;

	/* If the input data is small, do not even try to compress it */
	if (in_len < UBIFS_MIN_COMPR_LEN)
		goto no_compr;

	if (compr->comp_mutex)
		mutex_lock(compr->comp_mutex);
	err = -EIO;//FIXME crypto_comp_compress(compr->cc, in_buf, in_len, out_buf,
	//	   (unsigned int *)out_len);
	if (compr->comp_mutex)
		mutex_unlock(compr->comp_mutex);
	if (unlikely(err)) {
		ubifs_warn(c, "cannot compress %d bytes, compressor %s, error %d, leave data uncompressed",
			   in_len, compr->name, err);
		goto no_compr;
	}

	/*
	 * If the data compressed only slightly, it is better to leave it
	 * uncompressed to improve read speed.
	 */
	if (in_len - *out_len < UBIFS_MIN_COMPRESS_DIFF)
		goto no_compr;

	return;

no_compr:
	memcpy(out_buf, in_buf, in_len);
	*out_len = in_len;
	*compr_type = UBIFS_COMPR_NONE;
}

/**
 * compr_init - initialize a compressor.
 * @compr: compressor description object
 *
 * This function initializes the requested compressor and returns zero in case
 * of success or a negative error code in case of failure.
 */
static int __init compr_init(struct ubifs_compressor *compr)
{
	ubifs_compressors[compr->compr_type] = compr;

#ifdef CONFIG_NEEDS_MANUAL_RELOC
	ubifs_compressors[compr->compr_type]->name += gd->reloc_off;
	ubifs_compressors[compr->compr_type]->capi_name += gd->reloc_off;
	ubifs_compressors[compr->compr_type]->decompress += gd->reloc_off;
#endif

	if (compr->capi_name) {
		compr->cc = crypto_alloc_comp(compr->capi_name, 0, 0);
		if (IS_ERR(compr->cc)) {
			dbg_gen("cannot initialize compressor %s,"
				  " error %ld", compr->name,
				  PTR_ERR(compr->cc));
			return PTR_ERR(compr->cc);
		}
	}

	return 0;
}

/**
 * ubifs_compressors_init - initialize UBIFS compressors.
 *
 * This function initializes the compressor which were compiled in. Returns
 * zero in case of success and a negative error code in case of failure.
 */
int __init ubifs_compressors_init(void)
{
	int err;

	err = compr_init(&lzo_compr);
	if (err)
		return err;

	err = compr_init(&zlib_compr);
	if (err)
		return err;

	err = compr_init(&none_compr);
	if (err)
		return err;

	return 0;
}

/*
 * ubifsls...
 */

static int filldir(struct ubifs_info *c, const char *name, int namlen,
		   u64 ino, unsigned int d_type)
{
	struct inode *inode;
	char filetime[32];

	switch (d_type) {
	case UBIFS_ITYPE_REG:
		debug("\t");
		break;
	case UBIFS_ITYPE_DIR:
		debug("<DIR>\t");
		break;
	case UBIFS_ITYPE_LNK:
		debug("<LNK>\t");
		break;
	default:
		debug("other\t");
		break;
	}

	inode = ubifs_iget(c->vfs_sb, ino);
	if (IS_ERR(inode)) {
		debug("%s: Error in ubifs_iget(), ino=%ld ret=%p!\n",
		       __func__, ino, inode);
		return -1;
	}
	ctime_r((time_t *)&inode->i_mtime, filetime);
#ifndef __ZPL_BUILD__
	debug("%9lld  %24.24s  ", inode->i_size, filetime);
#ifndef __UBOOT__
	ubifs_iput(inode);
#endif

	debug("%s\n", name);
#else
	debug("%9ld  %24.24s  %s\n", (u32)(inode->i_size), filetime, name);
	ubifs_iput(inode);
#endif /* __ZPL_BUILD__ */
	return 0;
}

static int ubifs_printdir(struct file *file, void *dirent)
{
	int err, over = 0;
	struct qstr nm;
	union ubifs_key key;
	struct ubifs_dent_node *dent;
	struct inode *dir = file->f_path.dentry->d_inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;

	dbg_gen("dir ino %lu, f_pos %#llx", dir->i_ino, file->f_pos);

	if (file->f_pos > UBIFS_S_KEY_HASH_MASK || file->f_pos == 2)
		/*
		 * The directory was seek'ed to a senseless position or there
		 * are no more entries.
		 */
		return 0;

	if (file->f_pos == 1) {
		/* Find the first entry in TNC and save it */
		lowest_dent_key(c, &key, dir->i_ino);
		nm.name = NULL;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
	}

	dent = file->private_data;
	if (!dent) {
		/*
		 * The directory was seek'ed to and is now readdir'ed.
		 * Find the entry corresponding to @file->f_pos or the
		 * closest one.
		 */
		dent_key_init_hash(c, &key, dir->i_ino, file->f_pos);
		nm.name = NULL;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}
		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
	}

	while (1) {
		dbg_gen("feed '%s', ino %lu, new f_pos %#x",
			dent->name, (unsigned long long)le64_to_cpu(dent->inum),
			key_hash_flash(c, &dent->key));
		ubifs_assert(le64_to_cpu(dent->ch.sqnum) > ubifs_inode(dir)->creat_sqnum);

		nm.len = le16_to_cpu(dent->nlen);
		over = filldir(c, (char *)dent->name, nm.len,
			       le64_to_cpu(dent->inum), dent->type);
		if (over)
			return 0;

		/* Switch to the next entry */
		key_read(c, &dent->key, &key);
		nm.name = (char *)dent->name;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		kfree(file->private_data);
		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
		cond_resched();
	}

out:
	if (err != -ENOENT) {
		ubifs_err(c, "cannot find next direntry, error %d", err);
		return err;
	}

	kfree(file->private_data);
	file->private_data = NULL;
	file->f_pos = 2;
	return 0;
}

static int ubifs_finddir(struct super_block *sb, char *dirname,
			 unsigned long root_inum, unsigned long *inum)
{
	int err;
	struct qstr nm;
	union ubifs_key key;
	struct ubifs_dent_node *dent;
	struct ubifs_info *c;
	struct file *file;
	struct dentry *dentry;
	struct inode *dir = NULL;
	int ret = 0;

	file = kzalloc(sizeof(struct file), 0);
	dentry = kzalloc(sizeof(struct dentry), 0);
	if (!file || !dentry) {
		debug("%s: Error, no memory for malloc!\n", __func__);
		err = -ENOMEM;
		goto out_free;
	}

	dir = ubifs_iget(ubifs_sb, root_inum);
	if (IS_ERR(dir)) {
		debug("%s: Error reading inode %ld!\n", __func__, root_inum);
		ret = PTR_ERR(dir);
		goto out_free;
	}

	file->f_path.dentry = dentry;
	file->f_path.dentry->d_parent = dentry;
	file->f_path.dentry->d_inode = dir;
	c = sb->s_fs_info;

	dbg_gen("dir ino %lu, f_pos %#llx", dir->i_ino, file->f_pos);
	/* Find the first entry in TNC and save it */
	lowest_dent_key(c, &key, dir->i_ino);
	nm.name = NULL;
	dent = ubifs_tnc_next_ent(c, &key, &nm);
	if (IS_ERR(dent)) {
		err = PTR_ERR(dent);
		goto out;
	}

	file->f_pos = key_hash_flash(c, &dent->key);
	file->private_data = dent;

	while (1) {
		dbg_gen("feed '%s', ino %lu, new f_pos %#x",
			dent->name, (unsigned long long)le64_to_cpu(dent->inum),
			key_hash_flash(c, &dent->key));
		ubifs_assert(le64_to_cpu(dent->ch.sqnum) > ubifs_inode(dir)->creat_sqnum);

		nm.len = le16_to_cpu(dent->nlen);
		if ((strncmp(dirname, (char *)dent->name, nm.len) == 0) &&
		    (strlen(dirname) == nm.len)) {
			*inum = le64_to_cpu(dent->inum);
			ret = 1;
			goto out;
		}

		/* Switch to the next entry */
		key_read(c, &dent->key, &key);
		nm.name = (char *)dent->name;
		dent = ubifs_tnc_next_ent(c, &key, &nm);
		if (IS_ERR(dent)) {
			err = PTR_ERR(dent);
			goto out;
		}

		kfree(file->private_data);
		file->f_pos = key_hash_flash(c, &dent->key);
		file->private_data = dent;
		cond_resched();
	}

out:
	if (err != -ENOENT)
		dbg_gen("cannot find next direntry, error %d", err);

	if (!IS_ERR(dir))
		ubifs_iput(dir);

	if (file->private_data)
		kfree(file->private_data);

out_free:
	if (file)
		kfree(file);
	if (dentry)
		kfree(dentry);

	return ret;
}
static unsigned long ubifs_findfile(struct super_block *sb, char *filename,
	unsigned long *parent_dir)
{
	int ret;
	char *next;
	char fpath[128];
	char symlinkpath[128];
	char *name = fpath;
	unsigned long root_inum = 1;
	unsigned long inum;
	int symlink_count = 0; /* Don't allow symlink recursion */
	char link_name[64];

	strcpy(fpath, filename);

	/* Remove all leading slashes */
	while (*name == '/')
		name++;

	/*
	 * Handle root-direcoty ('/')
	 */
	inum = root_inum;
	if (parent_dir)
		*parent_dir = root_inum;
	if (!name || *name == '\0')
		return inum;

	for (;;) {
		struct inode *inode;
		struct ubifs_inode *ui;

		/* Extract the actual part from the pathname.  */
		next = strchr(name, '/');
		if (next) {
			/* Remove all leading slashes.  */
			while (*next == '/')
				*(next++) = '\0';
		}

		ret = ubifs_finddir(sb, name, root_inum, &inum);
		if (!ret)
			return 0;
		inode = ubifs_iget(sb, inum);

		if (!inode || IS_ERR(inode))
			return 0;
		ui = ubifs_inode(inode);

		if ((inode->i_mode & S_IFMT) == S_IFLNK) {
			char buf[128];

			/* We have some sort of symlink recursion, bail out */
			if (symlink_count++ > 8) {
				debug("Symlink recursion, aborting\n");
				ubifs_iput(inode);
				return 0;
			}
			memcpy(link_name, ui->data, ui->data_len);
			link_name[ui->data_len] = '\0';

			if (link_name[0] == '/') {
				/* Absolute path, redo everything without
				 * the leading slash */
				next = name = link_name + 1;
				root_inum = 1;
				continue;
			}
			/* Relative to cur dir */
			sprintf(buf, "%s/%s",
					link_name, next == NULL ? "" : next);
			memcpy(symlinkpath, buf, sizeof(buf));
			next = name = symlinkpath;
			ubifs_iput(inode);
			continue;
		}

		/*
		 * Check if directory with this name exists
		 */

		/* Found the node!  */
		if (!next || *next == '\0') {
			ubifs_iput(inode);
			return inum;
		}

		root_inum = inum;
		name = next;
		if (parent_dir)
			*parent_dir = inum;
		ubifs_iput(inode);
	}

	return 0;
}

int ubifs_set_blk_dev(struct blk_desc *rbdd, disk_partition_t *info)
{
	if (rbdd) {
		debug("UBIFS cannot be used with normal block devices\n");
		return -1;
	}

	/*
	 * Should never happen since blk_get_device_part_str() already checks
	 * this, but better safe then sorry.
	 */
	if (!ubifs_is_mounted()) {
		debug("UBIFS not mounted, use ubifsmount to mount volume first!\n");
		return -1;
	}

	return 0;
}

int ubifs_ls(const char *filename)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	struct file *file;
	struct dentry *dentry;
	struct inode *dir;
	void *dirent = NULL;
	unsigned long inum;
	int ret = 0;

	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READONLY);
	inum = ubifs_findfile(ubifs_sb, (char *)filename, NULL);
	if (!inum) {
		ret = -1;
		goto out;
	}


	file = kzalloc(sizeof(struct file), 0);
	dentry = kzalloc(sizeof(struct dentry), 0);
	if (!file || !dentry) {
		debug("%s: Error, no memory for malloc!\n", __func__);
		ret = -ENOMEM;
		goto out_mem;
	}

	dir = ubifs_iget(ubifs_sb, inum);
	if (IS_ERR(dir)) {
		debug("%s: Error reading inode %ld!\n", __func__, inum);
		ret = PTR_ERR(dir);
		goto out_mem;
	}

	dir->i_sb = ubifs_sb;
	file->f_path.dentry = dentry;
	file->f_path.dentry->d_parent = dentry;
	file->f_path.dentry->d_inode = dir;
	file->f_pos = 1;
	file->private_data = NULL;
	ubifs_printdir(file, dirent);

	ubifs_iput(dir);

out_mem:
	if (file)
		free(file);
	if (dentry)
		free(dentry);

out:
	ubi_close_volume(c->ubi);
	return ret;
}

int ubifs_exists(const char *filename)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum;

	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READONLY);
	inum = ubifs_findfile(ubifs_sb, (char *)filename, NULL);
	ubi_close_volume(c->ubi);

	return inum != 0;
}

int ubifs_size(const char *filename, loff_t *size)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum;
	struct inode *inode;
	int err = 0;

	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READONLY);

	inum = ubifs_findfile(ubifs_sb, (char *)filename, NULL);
	if (!inum) {
		err = -1;
		goto out;
	}

	inode = ubifs_iget(ubifs_sb, inum);
	if (IS_ERR(inode)) {
		debug("%s: Error reading inode %ld!\n", __func__, inum);
		err = PTR_ERR(inode);
		goto out;
	}

	*size = inode->i_size;

	ubifs_iput(inode);
out:
	ubi_close_volume(c->ubi);
	return err;
}

/*
 * ubifsload...
 */

/* file.c */

static inline void *kmap(struct page *page)
{
	return page->addr;
}

static int read_block(struct inode *inode, void *addr, unsigned int block,
		      struct ubifs_data_node *dn)
{
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	int err, len, out_len;
	union ubifs_key key;
	unsigned int dlen;

	data_key_init(c, &key, inode->i_ino, block);
	err = ubifs_tnc_lookup(c, &key, dn);
	if (err) {
		if (err == -ENOENT)
			/* Not found, so it must be a hole */
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		return err;
	}

	ubifs_assert(le64_to_cpu(dn->ch.sqnum) > ubifs_inode(inode)->creat_sqnum);

	len = le32_to_cpu(dn->size);
	if (len <= 0 || len > UBIFS_BLOCK_SIZE)
		goto dump;

	dlen = le32_to_cpu(dn->ch.len) - UBIFS_DATA_NODE_SZ;
	out_len = UBIFS_BLOCK_SIZE;
	err = ubifs_decompress(c, &dn->data, dlen, addr, &out_len,
			       le16_to_cpu(dn->compr_type));
	if (err || len != out_len)
		goto dump;

	/*
	 * Data length can be less than a full block, even for blocks that are
	 * not the last in the file (e.g., as a result of making a hole and
	 * appending data). Ensure that the remainder is zeroed out.
	 */
	if (len < UBIFS_BLOCK_SIZE)
		memset(addr + len, 0, UBIFS_BLOCK_SIZE - len);

	return 0;

dump:
	ubifs_err(c, "bad data node (block %u, inode %lu)",
		  block, inode->i_ino);
	ubifs_dump_node(c, dn);
	return -EINVAL;
}

static int do_readpage(struct ubifs_info *c, struct inode *inode,
		       struct page *page, int last_block_size)
{
	void *addr;
	int err = 0, i;
	unsigned int block, beyond;
	struct ubifs_data_node *dn;
	loff_t i_size = inode->i_size;

	dbg_gen("ino %lu, pg %lu, i_size %ld",
		inode->i_ino, page->index, i_size);

	addr = kmap(page);

	block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	beyond = (i_size + UBIFS_BLOCK_SIZE - 1) >> UBIFS_BLOCK_SHIFT;
	if (block >= beyond) {
		/* Reading beyond inode */
		memset(addr, 0, PAGE_CACHE_SIZE);
		goto out;
	}

	dn = kmalloc(UBIFS_MAX_DATA_NODE_SZ, GFP_NOFS);
	if (!dn)
		return -ENOMEM;

	i = 0;
	while (1) {
		int ret;

		if (block >= beyond) {
			/* Reading beyond inode */
			err = -ENOENT;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		} else {
			/*
			 * Reading last block? Make sure to not write beyond
			 * the requested size in the destination buffer.
			 */
			if (((block + 1) == beyond) || last_block_size) {
				void *buff;
				int dlen;

				/*
				 * We need to buffer the data locally for the
				 * last block. This is to not pad the
				 * destination area to a multiple of
				 * UBIFS_BLOCK_SIZE.
				 */
				buff = malloc_cache_aligned(UBIFS_BLOCK_SIZE);
				if (!buff) {
					debug("%s: Error, malloc fails!\n",
					       __func__);
					err = -ENOMEM;
					break;
				}

				/* Read block-size into temp buffer */
				ret = read_block(inode, buff, block, dn);
				if (ret) {
					err = ret;
					if (err != -ENOENT) {
						kfree(buff);
						break;
					}
				}

				if (last_block_size)
					dlen = last_block_size;
				else
					dlen = le32_to_cpu(dn->size);

				/* Now copy required size back to dest */
				memcpy(addr, buff, dlen);

				kfree(buff);
			} else {
				ret = read_block(inode, addr, block, dn);
				if (ret) {
					err = ret;
					if (err != -ENOENT)
						break;
				}
			}
		}
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		block += 1;
		addr += UBIFS_BLOCK_SIZE;
	}
	if (err) {
		if (err == -ENOENT) {
			/* Not found, so it must be a hole */
			dbg_gen("hole");
			goto out_free;
		}
		ubifs_err(c, "cannot read page %lu of inode %lu, error %d",
			  page->index, inode->i_ino, err);
		goto error;
	}

out_free:
	kfree(dn);
out:
	return 0;

error:
	kfree(dn);
	return err;
}

int ubifs_read(const char *filename, void *buf, loff_t offset,
	       loff_t size, loff_t *actread)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum;
	struct inode *inode;
	struct page page;
	int err = 0;
	int i;
	int count;
	int last_block_size = 0;

	*actread = 0;

	if (offset & (PAGE_SIZE - 1)) {
		debug("ubifs: Error offset must be a multiple of %d\n",
		       PAGE_SIZE);
		return -1;
	}

	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READONLY);
	/* ubifs_findfile will resolve symlinks, so we know that we get
	 * the real file here */
	inum = ubifs_findfile(ubifs_sb, (char *)filename, NULL);
	if (!inum) {
		err = -1;
		goto out;
	}

	/*
	 * Read file inode
	 */
	inode = ubifs_iget(ubifs_sb, inum);
	if (IS_ERR(inode)) {
		debug("%s: Error reading inode %ld!\n", __func__, inum);
		err = PTR_ERR(inode);
		goto out;
	}

	if (offset > inode->i_size) {
		debug("ubifs: Error offset (%ld) > file-size (%ld)\n",
		       offset, size);
		err = -1;
		goto put_inode;
	}

	/*
	 * If no size was specified or if size bigger than filesize
	 * set size to filesize
	 */
	if ((size == 0) || (size > (inode->i_size - offset)))
		size = inode->i_size - offset;

	count = (size + UBIFS_BLOCK_SIZE - 1) >> UBIFS_BLOCK_SHIFT;

	page.addr = buf;
	page.index = offset / PAGE_SIZE;
	page.inode = inode;
	for (i = 0; i < count; i++) {
		/*
		 * Make sure to not read beyond the requested size
		 */
		if (((i + 1) == count) && (size < inode->i_size))
			last_block_size = size - (i * PAGE_SIZE);

		err = do_readpage(c, inode, &page, last_block_size);
		if (err)
			break;

		page.addr += PAGE_SIZE;
		page.index++;
	}

	if (err) {
		debug("Error reading file '%s'\n", filename);
		*actread = i * PAGE_SIZE;
	} else {
		*actread = size;
	}

put_inode:
	ubifs_iput(inode);

out:
	ubi_close_volume(c->ubi);
	return err;
}

/**
 * inherit_flags - inherit flags of the parent inode.
 * @dir: parent inode
 * @mode: new inode mode flags
 *
 * This is a helper function for 'ubifs_new_inode()' which inherits flag of the
 * parent directory inode @dir. UBIFS inodes inherit the following flags:
 * o %UBIFS_COMPR_FL, which is useful to switch compression on/of on
 *   sub-directory basis;
 * o %UBIFS_SYNC_FL - useful for the same reasons;
 * o %UBIFS_DIRSYNC_FL - similar, but relevant only to directories.
 *
 * This function returns the inherited flags.
 */
static int inherit_flags(const struct inode *dir, umode_t mode)
{
	int flags;
	const struct ubifs_inode *ui = ubifs_inode(dir);

	if (!S_ISDIR(dir->i_mode))
		/*
		 * The parent is not a directory, which means that an extended
		 * attribute inode is being created. No flags.
		 */
		return 0;

	flags = ui->flags & (UBIFS_COMPR_FL | UBIFS_SYNC_FL | UBIFS_DIRSYNC_FL);
	if (!S_ISDIR(mode))
		/* The "DIRSYNC" flag only applies to directories */
		flags &= ~UBIFS_DIRSYNC_FL;
	return flags;
}

/**
 * ubifs_set_inode_flags - set VFS inode flags.
 * @inode: VFS inode to set flags for
 *
 * This function propagates flags from UBIFS inode object to VFS inode object.
 */
void ubifs_set_inode_flags(struct inode *inode)
{
	unsigned int flags = ubifs_inode(inode)->flags;

	inode->i_flags &= ~(S_SYNC | S_APPEND | S_IMMUTABLE | S_DIRSYNC);
	if (flags & UBIFS_SYNC_FL)
		inode->i_flags |= S_SYNC;
	if (flags & UBIFS_APPEND_FL)
		inode->i_flags |= S_APPEND;
	if (flags & UBIFS_IMMUTABLE_FL)
		inode->i_flags |= S_IMMUTABLE;
	if (flags & UBIFS_DIRSYNC_FL)
		inode->i_flags |= S_DIRSYNC;
}

/**
 * ubifs_new_inode - allocate new UBIFS inode object.
 * @c: UBIFS file-system description object
 * @dir: parent directory inode
 * @mode: inode mode flags
 *
 * This function finds an unused inode number, allocates new inode and
 * initializes it. Returns new inode in case of success and an error code in
 * case of failure.
 */
struct inode *ubifs_new_inode(struct ubifs_info *c, const struct inode *dir,
			      umode_t mode)
{
	struct inode *inode;
	struct ubifs_inode *ui;

	inode = c->vfs_sb->s_op->alloc_inode(c->vfs_sb);
	ui = ubifs_inode(inode);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	inode->i_sb = c->vfs_sb;
	list_add(&inode->i_sb_list, &c->vfs_sb->s_inodes);
	inode->i_state = I_LOCK | I_NEW;

	/*
	 * Set 'S_NOCMTIME' to prevent VFS form updating [mc]time of inodes and
	 * marking them dirty in file write path (see 'file_update_time()').
	 * UBIFS has to fully control "clean <-> dirty" transitions of inodes
	 * to make budgeting work.
	 */
	inode->i_flags |= S_NOCMTIME;

	inode->i_uid = dir->i_uid;
	inode->i_gid = dir->i_gid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = dir->i_ctime;
	inode->i_mode = mode;
	inode->__i_nlink = 1;
	switch (mode & S_IFMT) {
	case S_IFREG:
		break;
	case S_IFDIR:
		inode->i_size = ui->ui_size = UBIFS_INO_NODE_SZ;
		break;
	case S_IFLNK:
		break;
	case S_IFSOCK:
	case S_IFIFO:
	case S_IFBLK:
	case S_IFCHR:
		break;
	default:
		BUG();
	}
	ui->flags = inherit_flags(dir, mode);
	ubifs_set_inode_flags(inode);
	if (S_ISREG(mode))
		ui->compr_type = c->default_compr;
	else
		ui->compr_type = UBIFS_COMPR_NONE;
	ui->synced_i_size = 0;

	spin_lock(&c->cnt_lock);
	/* Inode number overflow is currently not supported */
	if (c->highest_inum >= INUM_WARN_WATERMARK) {
		if (c->highest_inum >= INUM_WATERMARK) {
			spin_unlock(&c->cnt_lock);
			ubifs_err(c, "out of inode numbers");
			return ERR_PTR(-EINVAL);
		}
		ubifs_warn(c, "running out of inode numbers (current %lu, max %u)",
			   (unsigned long)c->highest_inum, INUM_WATERMARK);
	}

	inode->i_ino = ++c->highest_inum;
	/*
	 * The creation sequence number remains with this inode for its
	 * lifetime. All nodes for this inode have a greater sequence number,
	 * and so it is possible to distinguish obsolete nodes belonging to a
	 * previous incarnation of the same inode number - for example, for the
	 * purpose of rebuilding the index.
	 */
	ui->creat_sqnum = ++c->max_sqnum;
	spin_unlock(&c->cnt_lock);

	return inode;
}

static struct inode *ubifs_create(struct inode *dir, struct qstr *nm, umode_t mode,
			bool excl)
{
	struct inode *inode;
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct ubifs_budget_req req = { .new_ino = 1, .new_dent = 1,
					.dirtied_ino = 1 };
	struct ubifs_inode *dir_ui = ubifs_inode(dir);
	int err, sz_change;

	/*
	 * Budget request settings: new inode, new direntry, changing the
	 * parent directory inode.
	 */

	dbg_gen("dent '%s', mode %#hx in dir ino %lu",
		nm->name, mode, dir->i_ino);

	err = ubifs_budget_space(c, &req);
	if (err)
		return ERR_PTR(err);

	sz_change = CALC_DENT_SIZE(fname_len(nm));

	inode = ubifs_new_inode(c, dir, mode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fname;
	}

	err = ubifs_init_security(dir, inode, nm);
	if (err)
		goto out_inode;

	mutex_lock(&dir_ui->ui_mutex);
	dir->__i_nlink++;
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	inode->i_size = 0;
	err = ubifs_jnl_update(c, dir, nm, inode, 0, 0);
	if (err)
		goto out_cancel;
	mutex_unlock(&dir_ui->ui_mutex);

	err = ubifs_run_commit(c);
	if (err)
		goto out_cancel;

	ubifs_release_budget(c, &req);

	return inode;

out_cancel:
	dir->__i_nlink--;
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	mutex_unlock(&dir_ui->ui_mutex);
out_inode:
out_fname:
	ubifs_release_budget(c, &req);
	ubifs_err(c, "cannot create regular file, error %d", err);
	return ERR_PTR(err);
}

/**
 * release_new_page_budget - release budget of a new page.
 * @c: UBIFS file-system description object
 *
 * This is a helper function which releases budget corresponding to the budget
 * of one new page of data.
 */
static void release_new_page_budget(struct ubifs_info *c)
{
	struct ubifs_budget_req req = { .recalculate = 1, .new_page = 1 };

	ubifs_release_budget(c, &req);
}

static int do_writepage(struct ubifs_info *c, struct inode *inode, struct page *page, int len)
{
	int err = 0, i, blen;
	unsigned int block;
	void *addr;
	union ubifs_key key;
	struct ubifs_budget_req req = { .recalculate = 1, .new_page = 1 };

	err = ubifs_budget_space(c, &req);
	if (unlikely(err))
		return err;

#ifdef UBIFS_DEBUG
	struct ubifs_inode *ui = ubifs_inode(inode);
	spin_lock(&ui->ui_lock);
	ubifs_assert(page->index <= ui->synced_i_size >> PAGE_SHIFT);
	spin_unlock(&ui->ui_lock);
#endif

	/* Update radix tree tags */
	set_page_writeback(page);

	addr = kmap(page);
	block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	i = 0;
	while (len) {
		blen = min_t(int, len, UBIFS_BLOCK_SIZE);
		data_key_init(c, &key, inode->i_ino, block);
		err = ubifs_jnl_write_data(c, inode, &key, addr, blen);
		if (err)
			break;
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		block += 1;
		addr += blen;
		len -= blen;
	}
	if (err) {
		SetPageError(page);
		ubifs_err(c, "cannot write page %lu of inode %lu, error %d",
			  page->index, inode->i_ino, err);
		ubifs_ro_mode(c, err);
	}
	release_new_page_budget(c);
	atomic_long_dec(&c->dirty_pg_cnt);
	return err;
}

/**
 * check_dir_empty - check if a directory is empty or not.
 * @dir: VFS inode object of the directory to check
 *
 * This function checks if directory @dir is empty. Returns zero if the
 * directory is empty, %-ENOTEMPTY if it is not, and other negative error codes
 * in case of of errors.
 */
static int ubifs_check_dir_empty(struct inode *dir)
{
	struct ubifs_info *c = dir->i_sb->s_fs_info;
	struct qstr nm = { 0 };
	struct ubifs_dent_node *dent;
	union ubifs_key key;
	int err;
	lowest_dent_key(c, &key, dir->i_ino);
	dent = ubifs_tnc_next_ent(c, &key, &nm);
	if (IS_ERR(dent)) {
		err = PTR_ERR(dent);
		if (err == -ENOENT)
			err = 0;
	} else {
		kfree(dent);
		err = -ENOTEMPTY;
	}
	return err;
}

int ubifs_rmdir(const char *filename)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum, idir;
	struct inode *inode, *dir;
	int err, sz_change, budgeted = 1;
	struct ubifs_inode *dir_ui;
	struct ubifs_budget_req req = { .mod_dent = 1, .dirtied_ino = 2 };
	struct qstr nm;
	char *p;

	p = strrchr(filename, '/');
	if (!p) {
		debug("%s: File path is not absolute '%s'!\n", __func__, filename);
		err = -EINVAL;
		goto out;
	}
		
	nm.name = p + 1;
	nm.len = strlen(p + 1);
		
	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READWRITE);


	/*
	 * Budget request settings: deletion direntry, deletion inode and
	 * changing the parent inode. If budgeting fails, go ahead anyway
	 * because we have extra space reserved for deletions.
	 */

	inum = ubifs_findfile(ubifs_sb, (char *)filename, &idir);

	if (!inum) {
		debug("%s: No inode for '%s'!\n", __func__, filename);
		err = -ENOENT;
		goto out;
	}

	dir = ubifs_iget(ubifs_sb, idir);
	if (IS_ERR(dir)) {
		debug("%s: No parent dir inode for '%s'!\n", __func__, filename);
		err = PTR_ERR(dir);
		goto out;
	}
	dir_ui = ubifs_inode(dir);

	inode = ubifs_iget(ubifs_sb, inum);
	if (IS_ERR(inode)) {
		debug("%s: Error reading inode %ld!\n", __func__, inum);
		err = PTR_ERR(inode);
		goto out_dir;
	}
	
	dbg_gen("directory '%s', ino %lu in dir ino %lu", filename,
		inode->i_ino, dir->i_ino);

	err = ubifs_check_dir_empty(inode);
	if (err)
		goto out_inode;
	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	err = ubifs_budget_space(c, &req);
	if (err) {
		if (err != -ENOSPC)
			goto out_inode;
		budgeted = 0;
	}

	inode->i_ctime = dir->i_ctime;
	inode->__i_nlink = 0;
	dir->__i_nlink--;
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	dir->__i_nlink--;
	err = ubifs_jnl_update(c, dir, &nm, inode, 1, 0);
	if (err)
		goto out_cancel;
	err = ubifs_jnl_write_inode(c, inode);
	if (err)
		goto out_cancel;

	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		/* We've deleted something - clean the "no space" flags */
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}

	ubifs_iput(dir);
	ubifs_iput(inode);

	ubi_close_volume(c->ubi);
	return 0;

out_cancel:
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->__i_nlink++;
	inode->__i_nlink = 2;
	if (budgeted)
		ubifs_release_budget(c, &req);
out_inode:
	ubifs_iput(inode);
out_dir:
	ubifs_iput(dir);
out:
	ubi_close_volume(c->ubi);
	return err;
}

int ubifs_mkdir(const char *filename)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum, parent_dir;
	struct inode *inode, *iparent_dir;
	int err = 0;

	struct qstr fn;
	char *p;

	p = strrchr(filename, '/');
	if (!p) {
		debug("%s: File path is not absolute '%s'!\n", __func__, filename);
		err = -EINVAL;
		goto out;
	}
		
	fn.name = p + 1;
	fn.len = strlen(p + 1);
		
	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READWRITE);
	inum = ubifs_findfile(ubifs_sb, (char *)filename, &parent_dir);
	if (!inum) {
		iparent_dir = ubifs_iget(ubifs_sb, parent_dir);
		if (IS_ERR(iparent_dir)) {
			debug("%s: No parent dir inode for '%s'!\n", __func__, filename);
			err = PTR_ERR(iparent_dir);
			goto out;
		}

		inode = ubifs_create(iparent_dir, &fn, S_IFDIR, 0);
		if (IS_ERR(inode)) {
			debug("%s: Can't create inode %d!\n", __func__, (int)inode);
			err = PTR_ERR(inode);
			goto out_dir;
		}

	} else {
		debug("%s: This name already exists: %s!\n", __func__, filename);
		err = -EACCES;
		goto out;
	}

	ubifs_iput(inode);
out_dir:
	ubifs_iput(iparent_dir);
out:
	ubi_close_volume(c->ubi);
	return err;
}

int ubifs_write(const char *filename, void *buf, loff_t offset,
	       loff_t size, loff_t *actwritten)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum, parent_dir;
	struct inode *inode, *iparent_dir;
	struct page page;
	struct ubifs_inode *ui;
	int err = 0;
	int i;
	int count;
	int last_block_size = 0;
	struct qstr fn;
	char *p;

	p = strrchr(filename, '/');
	if (!p) {
		debug("%s: File path is not absolute '%s'!\n", __func__, filename);
		err = -EINVAL;
		goto out;
	}
		
	fn.name = p + 1;
	fn.len = strlen(p + 1);
		
	*actwritten = 0;

	if (offset & (PAGE_SIZE - 1)) {
		debug("ubifs: Error offset must be a multiple of %d\n",
		       PAGE_SIZE);
		return -1;
	}

	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READWRITE);
	/* ubifs_findfile will resolve symlinks, so we know that we get
	 * the real file here */
	inum = ubifs_findfile(ubifs_sb, (char *)filename, &parent_dir);

	iparent_dir = ubifs_iget(ubifs_sb, parent_dir);
	if (IS_ERR(iparent_dir)) {
		debug("%s: No parent dir inode for '%s'!\n", __func__, filename);
		err = PTR_ERR(iparent_dir);
		goto out;
	}

	if (!inum) {
		inode = ubifs_create(iparent_dir, &fn, S_IFREG, 0);
		if (IS_ERR(inode)) {
			debug("%s: Can't create inode %d!\n", __func__, (int)inode);
			err = PTR_ERR(inode);
			goto out;
		}
		ubifs_iput(inode);
		inum = ubifs_findfile(ubifs_sb, (char *)filename, &parent_dir);
		if (!inum) {
			debug("%s: Can't find created inode!\n", __func__);
			err = PTR_ERR(inode);
			goto out_inode;
		}
	}

	ubifs_iput(iparent_dir);

	inode = ubifs_iget(ubifs_sb, inum);
	if (IS_ERR(inode)) {
		debug("%s: Error reading inode %ld!\n", __func__, inum);
		err = PTR_ERR(inode);
		goto out;
	}

	ui = ubifs_inode(inode);
	count = (size + UBIFS_BLOCK_SIZE - 1) >> UBIFS_BLOCK_SHIFT;

	page.addr = buf;
	page.index = offset / PAGE_SIZE;
	page.inode = inode;
	last_block_size = PAGE_SIZE;
	for (i = 0; i < count; i++) {
		/*
		 * Make sure to not write beyond the requested size
		 */
		if (((i + 1) == count))
			last_block_size = size - (i * PAGE_SIZE);
		
		err = do_writepage(c, inode, &page, last_block_size);
		if (err)
			break;

		page.addr += PAGE_SIZE;
		page.index++;
	}

	if (err) {
		debug("Error writing file '%s'\n", filename);
		*actwritten = i * PAGE_SIZE;
	} else {
		*actwritten = size;
	}

	if (inode->i_size < offset + *actwritten) {
		inode->i_size = offset + *actwritten;
		ui->ui_size = offset + *actwritten;
		ui->dirty = 1;
		err = ubifs_jnl_write_inode(c, inode);
		if (err)
			goto out_inode;
	}

	ubifs_run_commit(c);

out_inode:
	ubifs_iput(inode);
out:
	ubi_close_volume(c->ubi);
	return err;
}

int ubifs_unlink(const char *filename)
{
	struct ubifs_info *c = ubifs_sb->s_fs_info;
	unsigned long inum, idir;
	struct inode *inode, *dir;
	struct ubifs_inode *dir_ui;
	int err, sz_change, budgeted = 1;
	struct ubifs_budget_req req = { .mod_dent = 1, .dirtied_ino = 2 };
	unsigned int saved_nlink;
	struct qstr nm;
	char *p;

	p = strrchr(filename, '/');
	if (!p) {
		debug("%s: File path is not absolute '%s'!\n", __func__, filename);
		err = -EINVAL;
		goto out;
	}
		
	nm.name = p + 1;
	nm.len = strlen(p + 1);
		
	c->ubi = ubi_open_volume(c->vi.ubi_num, c->vi.vol_id, UBI_READWRITE);

	/*
	 * Budget request settings: deletion direntry, deletion inode (+1 for
	 * @dirtied_ino), changing the parent directory inode. If budgeting
	 * fails, go ahead anyway because we have extra space reserved for
	 * deletions.
	 */

	inum = ubifs_findfile(ubifs_sb, (char *)filename, &idir);

	if (!inum) {
		debug("%s: No inode for '%s'!\n", __func__, filename);
		err = -ENOENT;
		goto out;
	}

	dir = ubifs_iget(ubifs_sb, idir);
	if (IS_ERR(dir)) {
		debug("%s: No parent dir inode for '%s'!\n", __func__, filename);
		err = PTR_ERR(dir);
		goto out;
	}
	dir_ui = ubifs_inode(dir);

	if (!inum) {
		debug("%s: No inode for '%s'!\n", __func__, filename);
		err = PTR_ERR(inode);
		goto out_dir;
	}

	inode = ubifs_iget(ubifs_sb, inum);
	if (IS_ERR(inode)) {
		debug("%s: Error reading inode %ld!\n", __func__, inum);
		err = PTR_ERR(inode);
		goto out_dir;
	}
	
	sz_change = CALC_DENT_SIZE(fname_len(&nm));

	err = ubifs_budget_space(c, &req);
	if (err) {
		if (err != -ENOSPC)
			goto out_inode;
		budgeted = 0;
	}

	inode->i_ctime = dir->i_ctime;
	saved_nlink = inode->i_nlink;
	inode->__i_nlink--;
	dir->i_size -= sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->i_mtime = dir->i_ctime = inode->i_ctime;
	dir->__i_nlink--;
	err = ubifs_jnl_update(c, dir, &nm, inode, 1, 0);
	if (err)
		goto out_cancel;

	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		/* We've deleted something - clean the "no space" flags */
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}

	ubifs_iput(dir);
	ubifs_iput(inode);

	ubi_close_volume(c->ubi);
	return 0;

out_cancel:
	dir->i_size += sz_change;
	dir_ui->ui_size = dir->i_size;
	dir->__i_nlink++;
	set_nlink(inode, saved_nlink);
	if (budgeted)
		ubifs_release_budget(c, &req);
out_inode:
	ubifs_iput(inode);
out_dir:
	ubifs_iput(dir);
out:
	ubi_close_volume(c->ubi);
	return err;
}

void ubifs_close(void)
{
}

/* Compat wrappers for common/cmd_ubifs.c */
int ubifs_load(char *filename, u32 addr, u32 size)
{
	loff_t actread;
	int err;

	debug("Loading file '%s' to addr 0x%08x...\n", filename, addr);

	err = ubifs_read(filename, (void *)(uintptr_t)addr, 0, size, &actread);
	if (err == 0) {
		setenv_hex("filesize", actread);
		debug("Done\n");
	}

	return err;
}

void uboot_ubifs_umount(void)
{
	if (ubifs_sb) {
		struct ubifs_info *c = ubifs_sb->s_fs_info;

		debug("Unmounting UBIFS volume %s!\n",
		       ((struct ubifs_info *)(ubifs_sb->s_fs_info))->vi.name);
		ubifs_umount(c);
		ubifs_sb = NULL;
	}
}
