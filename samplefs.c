	#include <linux/module.h>
	#include <linux/fs.h>
	#include <linux/pagemap.h>
	#include <linux/version.h>
	#include <linux/nls.h>
	#include <linux/proc_fs.h>
	#include <linux/backing-dev.h>
	#include <linux/init.h>
	#include <linux/highmem.h>
	#include <linux/smp_lock.h>

	#define SAMPLEFS_ROOT_I 2
	/* samplefs mount flags */
	#define SFS_MNT_CASE 1
	/* helpful if this is different than other fs */
	#define SAMPLEFS_MAGIC     0x73616d70 /* "SAMP" */

	unsigned int sample_parm = 0;
	module_param(sample_parm, int, 0);
	MODULE_PARM_DESC(sample_parm,"An example parm. Default: x Range: y to z");


	static int sfs_delete_dentry(struct dentry *);

	static int sfs_ci_hash(struct dentry *, struct qstr *);

	static int sfs_ci_compare(struct dentry *, struct qstr *, struct qstr *);

	static int sfs_create(struct inode *, struct dentry *, int , struct nameidata *);

	static struct dentry *sfs_lookup(struct inode *, struct dentry *, struct nameidata *);

	static int sfs_symlink(struct inode *, struct dentry *, const char *);

	static int sfs_mkdir(struct inode * , struct dentry * , int );

	static int sfs_mknod(struct inode *, struct dentry *, int , dev_t );

	static void samplefs_put_super(struct super_block *);


	struct address_space_operations sfs_aops = {
		.readpage       = simple_readpage,
		.write_begin    = simple_write_begin,
		.write_end      = simple_write_end,
	//	.prepare_write  = simple_prepare_write,
	//	.commit_write   = simple_commit_write
	};

	struct dentry_operations sfs_dentry_ops = {
		.d_delete = sfs_delete_dentry,
	};

	struct dentry_operations sfs_ci_dentry_ops = {
	/*	.d_revalidate = xxxd_revalidate, Not needed for this type of fs */
		.d_hash = sfs_ci_hash,
		.d_compare = sfs_ci_compare,
		.d_delete = sfs_delete_dentry,
	};

	static struct backing_dev_info sfs_backing_dev_info = {
		.ra_pages       = 0,    /* No readahead */
		.capabilities   = BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_WRITEBACK |
				  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
				  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP |
				  BDI_CAP_EXEC_MAP,
	};

	struct inode_operations sfs_file_inode_ops = {
		.getattr        = simple_getattr,
	};

	struct inode_operations sfs_dir_inode_ops = {
		.create         = sfs_create,
		.lookup         = sfs_lookup,
		.link		= simple_link,
		.unlink         = simple_unlink,
		.symlink	= sfs_symlink,
		.mkdir          = sfs_mkdir,
		.rmdir          = simple_rmdir,
		.mknod          = sfs_mknod,
		.rename         = simple_rename,
	};

	struct file_operations sfs_file_operations = {
		.read           = do_sync_read,
		.aio_read	= generic_file_aio_read,
		.write          = do_sync_write,
		.aio_write	= generic_file_aio_write,
		.mmap           = generic_file_mmap,
		.fsync          = simple_sync_file,
	//	.sendfile       = generic_file_sendfile,
		.llseek         = generic_file_llseek,
	};

	struct samplefs_sb_info {
		unsigned int rsize;
		unsigned int wsize;
		int flags;
		struct nls_table *local_nls;
	};

	static inline struct samplefs_sb_info *
	SFS_SB(struct super_block *sb)
	{
		return sb->s_fs_info;
	}

	struct super_operations samplefs_super_ops = {
		.statfs         = simple_statfs,
		.drop_inode     = generic_delete_inode, /* Not needed, is the default */
		.put_super      = samplefs_put_super,
	};

	static void samplefs_put_super(struct super_block *sb)
	{
		struct samplefs_sb_info *sfs_sb;

		sfs_sb = SFS_SB(sb);
		if(sfs_sb == NULL) {
		      /* Empty superblock info passed to unmount */
			return;
		}
	 
		unload_nls(sfs_sb->local_nls);
	 
		/* FS-FILLIN your fs specific umount logic here */

		kfree(sfs_sb);
		return;
	}

	static void 
	samplefs_parse_mount_options(char *options, struct samplefs_sb_info * sfs_sb)
	{
		char *value;
		char *data;
		int size;

		if(!options)
			return;

		printk(KERN_INFO "samplefs: parsing mount options %s\n", options);

		while ((data = strsep(&options, ",")) != NULL) {
			if (!*data)
				continue;
			if ((value = strchr(data, '=')) != NULL)
				*value++ = '\0';

			if (strnicmp(data, "rsize", 5) == 0) {
				if (value && *value) {
					size = simple_strtoul(value, &value, 0);
					if(size > 0) {
						sfs_sb->rsize = size;
						printk(KERN_INFO
							"samplefs: rsize %d\n", size);
					}
				}
			} else if (strnicmp(data, "wsize", 5) == 0) {
				if (value && *value) {
					size = simple_strtoul(value, &value, 0);
					if(size > 0) {
						sfs_sb->wsize = size;
						printk(KERN_INFO
							"samplefs: wsize %d\n", size);
					}
				}
			} else if ((strnicmp(data, "nocase", 6) == 0) ||
				   (strnicmp(data, "ignorecase", 10)  == 0)) {
				sfs_sb->flags |= SFS_MNT_CASE;
				printk(KERN_INFO "samplefs: ignore case\n");

			} else {
				printk(KERN_WARNING "samplefs: bad mount option %s\n",
					data);
			} 
		}
	}

	static int sfs_ci_hash(struct dentry *dentry, struct qstr *q)
	{
		struct nls_table *codepage = SFS_SB(dentry->d_inode->i_sb)->local_nls;
		unsigned long hash;
		int i;

		hash = init_name_hash();
		for (i = 0; i < q->len; i++)
			hash = partial_name_hash(nls_tolower(codepage, q->name[i]),
						 hash);
		q->hash = end_name_hash(hash);

		return 0;
	}

	static int sfs_ci_compare(struct dentry *dentry, struct qstr *a, struct qstr *b)
	{
		struct nls_table *codepage = SFS_SB(dentry->d_inode->i_sb)->local_nls;

		if ((a->len == b->len) &&
		    (nls_strnicmp(codepage, a->name, b->name, a->len) == 0)) {
			/*
			 * To preserve case, don't let an existing negative dentry's
			 * case take precedence.  If a is not a negative dentry, this
			 * should have no side effects
			 */
			memcpy((unsigned char *)a->name, b->name, a->len);
			return 0;
		}
		return 1;
	}

	/* No sense hanging on to negative dentries as they are only
	in memory - we are not saving anything as we would for network
	or disk filesystem */

	static int sfs_delete_dentry(struct dentry *dentry)
	{
		return 1;
	}

	struct inode *samplefs_get_inode(struct super_block *sb, int mode, dev_t dev)
	{
		struct inode * inode = new_inode(sb);
		struct samplefs_sb_info * sfs_sb = SFS_SB(sb);

		if (inode) {
			inode->i_mode = mode;
			inode->i_uid = current_fsuid();
                inode->i_gid = current_fsgid();
                inode->i_blocks = 0;
                inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		printk(KERN_INFO "about to set inode ops\n");
		inode->i_mapping->a_ops = &sfs_aops;
		inode->i_mapping->backing_dev_info = &sfs_backing_dev_info;
                switch (mode & S_IFMT) {
                default:
			init_special_inode(inode, mode, dev);
			break;
                case S_IFREG:
			printk(KERN_INFO "file inode\n");
			inode->i_op = &sfs_file_inode_ops;
			inode->i_fop =  &sfs_file_operations;
			break;
                case S_IFDIR:
			printk(KERN_INFO "directory inode sfs_sb: %p\n",sfs_sb);
			inode->i_op = &sfs_dir_inode_ops;
			inode->i_fop = &simple_dir_operations;

                        /* link == 2 (for initial ".." and "." entries) */
                        inode->i_nlink++;
                        break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			break;
                }
        }
        return inode;
	
}

static int samplefs_fill_super(struct super_block * sb, void * data, int silent)
{
	struct inode * inode;
	struct samplefs_sb_info * sfs_sb;

	sb->s_maxbytes = MAX_LFS_FILESIZE; /* NB: may be too large for mem */
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = SAMPLEFS_MAGIC;
	sb->s_op = &samplefs_super_ops;
	sb->s_time_gran = 1; /* 1 nanosecond time granularity */


	printk(KERN_INFO "samplefs: fill super\n");

#ifdef CONFIG_SAMPLEFS_DEBUG
	printk(KERN_INFO "samplefs: about to alloc s_fs_info\n");
#endif
	sb->s_fs_info = kzalloc(sizeof(struct samplefs_sb_info),GFP_KERNEL);
	sfs_sb = SFS_SB(sb);
	if(!sfs_sb) {
		return -ENOMEM;
	}

	inode = samplefs_get_inode(sb, S_IFDIR | 0755, 0);
	if(!inode) {
		kfree(sfs_sb);
		return -ENOMEM;
	}
	
	printk(KERN_INFO "samplefs: about to alloc root inode\n");

	sb->s_root = d_alloc_root(inode);
	if (!sb->s_root) {
		iput(inode);
		kfree(sfs_sb);
		return -ENOMEM;
	}
	
	/* below not needed for many fs - but an example of per fs sb data */
	sfs_sb->local_nls = load_nls_default();

	samplefs_parse_mount_options(data, sfs_sb);
	
	/* FS-FILLIN your filesystem specific mount logic/checks here */

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
struct super_block * samplefs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data)
{
	return get_sb_nodev(fs_type, flags, data, samplefs_fill_super);
}
#else
int samplefs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags, data, samplefs_fill_super, mnt);
}
#endif


static struct file_system_type samplefs_fs_type = {
	.owner = THIS_MODULE,
	.name = "samplefs",
	.get_sb = samplefs_get_sb,
	.kill_sb = kill_litter_super,
	/*  .fs_flags */
};


static struct proc_dir_entry *proc_fs_samplefs;

static int
sfs_debug_read(char *buf, char **beginBuffer, off_t offset,
                     int count, int *eof, void *data)
{
	int length = 0;
	char * original_buf = buf;

	*beginBuffer = buf + offset;

	length = sprintf(buf,
                    "Display Debugging Information\n"
                    "-----------------------------\n");

	buf += length;

	/* FS-FILLIN - add your debug information here */

	length = buf - original_buf;
	if(offset + count >= length)
		*eof = 1;
	if(length < offset) {
		*eof = 1;
		return 0;
	} else {
		length = length - offset;
	}

	if (length > count)
                length = count;

	return length;
}

static struct dentry *sfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct samplefs_sb_info * sfs_sb = SFS_SB(dir->i_sb);
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	if(sfs_sb->flags & SFS_MNT_CASE)
		dentry->d_op = &sfs_ci_dentry_ops;
	else
		dentry->d_op = &sfs_dentry_ops;

	d_add(dentry, NULL);
	return NULL;
}

static int sfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
        struct inode * inode = samplefs_get_inode(dir->i_sb, mode, dev);
        int error = -ENOSPC;
	
	printk(KERN_INFO "samplefs: mknod\n");
        if (inode) {
                if (dir->i_mode & S_ISGID) {
                        inode->i_gid = dir->i_gid;
                        if (S_ISDIR(mode))
                                inode->i_mode |= S_ISGID;
                }
                d_instantiate(dentry, inode);
                dget(dentry);   /* Extra count - pin the dentry in core */
                error = 0;
                dir->i_mtime = dir->i_ctime = CURRENT_TIME;

		/* real filesystems would normally use i_size_write function */
		dir->i_size += 0x20;  /* bogus small size for each dir entry */
        }
        return error;
}


static int sfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
        int retval = 0;
	
	retval = sfs_mknod(dir, dentry, mode | S_IFDIR, 0);

	/* link count is two for dir, for dot and dot dot */
        if (!retval)
                dir->i_nlink++;
        return retval;
}

static int sfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
        return sfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int sfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = samplefs_get_inode(dir->i_sb, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			if (dir->i_mode & S_ISGID)
				inode->i_gid = dir->i_gid;
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		} else
			iput(inode);
	}
	return error;
}



static int __init init_samplefs_fs(void)
{
	printk(KERN_INFO "init samplefs\n");


	/* some filesystems pass optional parms at load time */
	if(sample_parm > 256) {
		printk("sample_parm %d too large, reset to 10\n", sample_parm);
		sample_parm = 10;
	}

	return register_filesystem(&samplefs_fs_type);
}

static void __exit exit_samplefs_fs(void)
{
	printk(KERN_INFO "unloading samplefs\n");

	unregister_filesystem(&samplefs_fs_type);
}

module_init(init_samplefs_fs)
module_exit(exit_samplefs_fs)

MODULE_LICENSE("GPL");
