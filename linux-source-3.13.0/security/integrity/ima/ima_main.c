/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Reiner Sailer <sailer@watson.ibm.com>
 * Serge Hallyn <serue@us.ibm.com>
 * Kylene Hall <kylene@us.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_main.c
 *	implements the IMA hooks: ima_bprm_check, ima_file_mmap,
 *	and ima_file_check.
 */
#include <linux/module.h>
#include <linux/file.h>
#include <linux/binfmts.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/xattr.h>
#include <linux/ima.h>
#include <linux/crypto.h>
#include <crypto/hash_info.h>
#include <crypto/hash.h>

#include <linux/mount.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/list.h>

#include "ima.h"

/* list of all mnt_namespace, used to find cpcr through proc_num */
struct mnt_namespace_list mnt_ns_list;

/* record the history value of physical PCR to bind all cPCRs into
 *  a physical PCR, i.e. PCR12
 */
struct cPCR cpcr_for_history;

int ima_initialized;

#ifdef CONFIG_IMA_APPRAISE
int ima_appraise = IMA_APPRAISE_ENFORCE;
#else
int ima_appraise;
#endif

int ima_hash_algo = HASH_ALGO_SHA1;
static int hash_setup_done;

static int __init hash_setup(char *str)
{
	struct ima_template_desc *template_desc = ima_template_desc_current();
	int i;

	if (hash_setup_done)
		return 1;

	if (strcmp(template_desc->name, IMA_TEMPLATE_IMA_NAME) == 0) {
		if (strncmp(str, "sha1", 4) == 0)
			ima_hash_algo = HASH_ALGO_SHA1;
		else if (strncmp(str, "md5", 3) == 0)
			ima_hash_algo = HASH_ALGO_MD5;
		goto out;
	}

	for (i = 0; i < HASH_ALGO__LAST; i++) {
		if (strcmp(str, hash_algo_name[i]) == 0) {
			ima_hash_algo = i;
			break;
		}
	}
out:
	hash_setup_done = 1;
	return 1;
}
__setup("ima_hash=", hash_setup);

/*
 * ima_rdwr_violation_check
 *
 * Only invalidate the PCR for measured files:
 * 	- Opening a file for write when already open for read,
 *	  results in a time of measure, time of use (ToMToU) error.
 *	- Opening a file for read when already open for write,
 * 	  could result in a file measurement error.
 *
 */
static void ima_rdwr_violation_check(struct file *file)
{
	struct dentry *dentry = file->f_path.dentry;
	struct inode *inode = file_inode(file);
	fmode_t mode = file->f_mode;
	int must_measure;
	bool send_tomtou = false, send_writers = false;
	char *pathbuf = NULL;
	const char *pathname;

	if (!S_ISREG(inode->i_mode) || !ima_initialized)
		return;

	mutex_lock(&inode->i_mutex);	/* file metadata: permissions, xattr */

	if (mode & FMODE_WRITE) {
		if (atomic_read(&inode->i_readcount) && IS_IMA(inode))
			send_tomtou = true;
		goto out;
	}

	must_measure = ima_must_measure(inode, MAY_READ, FILE_CHECK);
	if (!must_measure)
		goto out;

	if (atomic_read(&inode->i_writecount) > 0)
		send_writers = true;
out:
	mutex_unlock(&inode->i_mutex);

	if (!send_tomtou && !send_writers)
		return;

	pathname = ima_d_path(&file->f_path, &pathbuf);
	if (!pathname || strlen(pathname) > IMA_EVENT_NAME_LEN_MAX)
		pathname = dentry->d_name.name;

	if (send_tomtou)
		ima_add_violation(file, pathname, "invalid_pcr", "ToMToU");
	if (send_writers)
		ima_add_violation(file, pathname,
				  "invalid_pcr", "open_writers");
	kfree(pathbuf);
}

static void ima_check_last_writer(struct integrity_iint_cache *iint,
				  struct inode *inode, struct file *file)
{
	fmode_t mode = file->f_mode;

	if (!(mode & FMODE_WRITE))
		return;

	mutex_lock(&inode->i_mutex);
	if (atomic_read(&inode->i_writecount) == 1 &&
	    iint->version != inode->i_version) {
		iint->flags &= ~IMA_DONE_MASK;
		if (iint->flags & IMA_APPRAISE)
			ima_update_xattr(iint, file);
	}
	mutex_unlock(&inode->i_mutex);
}

/**
 * ima_file_free - called on __fput()
 * @file: pointer to file structure being freed
 *
 * Flag files that changed, based on i_version
 */
void ima_file_free(struct file *file)
{
	struct inode *inode = file_inode(file);
	struct integrity_iint_cache *iint;

	if (!iint_initialized || !S_ISREG(inode->i_mode))
		return;

	iint = integrity_iint_find(inode);
	if (!iint)
		return;

	ima_check_last_writer(iint, inode, file);
}

static int process_measurement(struct file *file, const char *filename,
			       int mask, int function)
{
	struct inode *inode = file_inode(file);
	struct integrity_iint_cache *iint;
	struct ima_template_desc *template_desc = ima_template_desc_current();
	char *pathbuf = NULL;
	const char *pathname = NULL;
	char *ns_pathname = NULL;
	int rc = -ENOMEM, action, must_appraise, _func;
	struct evm_ima_xattr_data *xattr_value = NULL, **xattr_ptr = NULL;
	int xattr_len = 0;

	struct mnt_namespace *ns = NULL;
	unsigned int mnt_ns_num = CPCR_NULL_NAMESPACE;

	if (!ima_initialized || !S_ISREG(inode->i_mode))
		return 0;

	/* Return an IMA_MEASURE, IMA_APPRAISE, IMA_AUDIT action
	 * bitmask based on the appraise/audit/measurement policy.
	 * Included is the appraise submask.
	 */
	action = ima_get_action(inode, mask, function);
	if (!action)
		return 0;

	must_appraise = action & IMA_APPRAISE;

	/*  Is the appraise rule hook specific?  */
	_func = (action & IMA_FILE_APPRAISE) ? FILE_CHECK : function;

	mutex_lock(&inode->i_mutex);

	iint = integrity_inode_get(inode);
	if (!iint)
		goto out;

	/* Determine if already appraised/measured based on bitmask
	 * (IMA_MEASURE, IMA_MEASURED, IMA_XXXX_APPRAISE, IMA_XXXX_APPRAISED,
	 *  IMA_AUDIT, IMA_AUDITED)
	 */
	iint->flags |= action;
	action &= IMA_DO_MASK;
	action &= ~((iint->flags & IMA_DONE_MASK) >> 1);

	/* Nothing to do, just return existing appraised status */
	if (!action) {
		if (must_appraise)
			rc = ima_get_cache_status(iint, _func);
		goto out_digsig;
	}

	if (strcmp(template_desc->name, IMA_TEMPLATE_IMA_NAME) == 0) {
		if (action & IMA_APPRAISE_SUBMASK)
			xattr_ptr = &xattr_value;
	} else
		xattr_ptr = &xattr_value;

	rc = ima_collect_measurement(iint, file, xattr_ptr, &xattr_len);
	if (rc != 0) {
		if (file->f_flags & O_DIRECT)
			rc = (iint->flags & IMA_PERMIT_DIRECTIO) ? 0 : -EACCES;
		goto out_digsig;
	}

	pathname = !filename ? ima_d_path(&file->f_path, &pathbuf) : filename;
	if (!pathname)
		pathname = (const char *)file->f_dentry->d_name.name;

	//get the proc number of current pid's namespace
	ns_pathname = kmalloc(PATH_MAX + 11, GFP_KERNEL);
	if (ns_pathname && pathname) {
		if(current->nsproxy)
			ns = current->nsproxy->mnt_ns;
	} else {
		printk("[Wu Luo] ERROR: failed to kmalloc ns_pathname\n");
		goto out;
	}

	if(ns) {
		mnt_ns_num = mntns_inum(ns);
		sprintf(ns_pathname, "%u:%s", mnt_ns_num, pathname);
	} else {
		strcpy(ns_pathname, pathname);
	}

	if (action & IMA_MEASURE) {
		ima_store_measurement(iint, file, ns_pathname,
				      xattr_value, xattr_len, mnt_ns_num, function);
	}
	if (action & IMA_APPRAISE_SUBMASK)
		rc = ima_appraise_measurement(_func, iint, file, ns_pathname,
					      xattr_value, xattr_len);
	if (action & IMA_AUDIT)
		ima_audit_measurement(iint, ns_pathname);
	kfree(pathbuf);
out_digsig:
	if ((mask & MAY_WRITE) && (iint->flags & IMA_DIGSIG))
		rc = -EACCES;
out:

//	if(ns_pathname) {
//		kfree(ns_pathname);
//	}

	mutex_unlock(&inode->i_mutex);
	kfree(xattr_value);
	if ((rc && must_appraise) && (ima_appraise & IMA_APPRAISE_ENFORCE))
		return -EACCES;
	return 0;
}

/**
 * ima_file_mmap - based on policy, collect/store measurement.
 * @file: pointer to the file to be measured (May be NULL)
 * @prot: contains the protection that will be applied by the kernel.
 *
 * Measure files being mmapped executable based on the ima_must_measure()
 * policy decision.
 *
 * On success return 0.  On integrity appraisal error, assuming the file
 * is in policy and IMA-appraisal is in enforcing mode, return -EACCES.
 */
int ima_file_mmap(struct file *file, unsigned long prot)
{
	if (file && (prot & PROT_EXEC))
		return process_measurement(file, NULL, MAY_EXEC, MMAP_CHECK);
	return 0;
}

/**
 * ima_bprm_check - based on policy, collect/store measurement.
 * @bprm: contains the linux_binprm structure
 *
 * The OS protects against an executable file, already open for write,
 * from being executed in deny_write_access() and an executable file,
 * already open for execute, from being modified in get_write_access().
 * So we can be certain that what we verify and measure here is actually
 * what is being executed.
 *
 * On success return 0.  On integrity appraisal error, assuming the file
 * is in policy and IMA-appraisal is in enforcing mode, return -EACCES.
 */
int ima_bprm_check(struct linux_binprm *bprm)
{
	return process_measurement(bprm->file,
				   (strcmp(bprm->filename, bprm->interp) == 0) ?
				   bprm->filename : bprm->interp,
				   MAY_EXEC, BPRM_CHECK);
}

/**
 * ima_path_check - based on policy, collect/store measurement.
 * @file: pointer to the file to be measured
 * @mask: contains MAY_READ, MAY_WRITE or MAY_EXECUTE
 *
 * Measure files based on the ima_must_measure() policy decision.
 *
 * On success return 0.  On integrity appraisal error, assuming the file
 * is in policy and IMA-appraisal is in enforcing mode, return -EACCES.
 */
int ima_file_check(struct file *file, int mask)
{
	ima_rdwr_violation_check(file);
	return process_measurement(file, NULL,
				   mask & (MAY_READ | MAY_WRITE | MAY_EXEC),
				   FILE_CHECK);
}
EXPORT_SYMBOL_GPL(ima_file_check);

/**
 * ima_module_check - based on policy, collect/store/appraise measurement.
 * @file: pointer to the file to be measured/appraised
 *
 * Measure/appraise kernel modules based on policy.
 *
 * On success return 0.  On integrity appraisal error, assuming the file
 * is in policy and IMA-appraisal is in enforcing mode, return -EACCES.
 */
int ima_module_check(struct file *file)
{
	if (!file) {
#ifndef CONFIG_MODULE_SIG_FORCE
		if ((ima_appraise & IMA_APPRAISE_MODULES) &&
		    (ima_appraise & IMA_APPRAISE_ENFORCE))
			return -EACCES;	/* INTEGRITY_UNKNOWN */
#endif
		return 0;	/* We rely on module signature checking */
	}
	return process_measurement(file, NULL, MAY_EXEC, MODULE_CHECK);
}

int ima_add_ns_task(struct file* file, char* filename,
		unsigned int mnt_ns_num)
{
	struct ima_template_entry *entry;
	struct integrity_iint_cache tmp_iint, *iint = &tmp_iint;

	int result = -ENOMEM;
	int violation = 0;
	struct {
		struct ima_digest_data hdr;
		char digest[TPM_DIGEST_SIZE];
	} hash;

	memset(iint, 0, sizeof(*iint));
	memset(&hash, 0, sizeof(hash));
	iint->ima_hash = &hash.hdr;
	iint->ima_hash->algo = HASH_ALGO_SHA1;
	iint->ima_hash->length = SHA1_DIGEST_SIZE;

	printk(">>> prepare to calculate file[%s] hash\n", filename);

	result = ima_calc_file_hash(file, &hash.hdr);
	if (result < 0) {
		return result;
	}

	printk(">>> prepare to succeed\n");

	result = ima_alloc_init_template(iint, NULL, filename,
					 NULL, 0, &entry);
	if (result < 0)
		return result;

	printk(">>> prepare to record\n");
	result = ima_store_template(entry, violation, NULL,
			filename, mnt_ns_num, FILE_CHECK);
	if (result < 0)
		ima_free_template_entry(entry);

	printk(">>> succeed\n");
	return result;
}

/*
 * ima_record_task_for_ns - based on Trusted-Container.
 *
 * locates all files related to the task that creates a namspace,
 * and records it into this namespace's measurement list.
 */
int ima_record_task_for_ns(unsigned int mnt_ns_num) {

	struct task_struct* task_p;
	struct path files_path;
	const unsigned char *cwd;

	char *filename = NULL, *tempname = NULL;

	filename = kmalloc(PATH_MAX + 11, GFP_KERNEL);
	tempname = kmalloc(PATH_MAX + 11, GFP_KERNEL);
	if ((!filename) || (!tempname)) {
		printk("kmalloc failed.\n");
		return -1;
	}

	printk(">>> locate all ancestors' pids\n");
	task_p = current;
	sprintf(tempname, "%d", current->pid);

	while(task_p->real_parent && task_p != task_p->real_parent) {
		task_p = task_p->real_parent;
		sprintf(filename, "->%d", task_p->pid);
		strcat(tempname, filename);
	}

	printk(">>>> measure it\n");
	task_p = current;
	if (task_p->mm && task_p->mm->exe_file) {
		files_path = task_p->mm->exe_file->f_path;
		cwd = ima_d_path(&files_path, &filename);
		if (!cwd)
			cwd = (const char *)task_p->mm->exe_file->f_dentry->d_name.name;
		printk("[Wu Luo][DEBUG] record the current file[%s] for ns[%u]\n",
				cwd, mnt_ns_num);

		// get a ns_pathname, for that IMA does not record a same file twice
		sprintf(filename, "%s %u:%s", tempname, mnt_ns_num, cwd);

		// record it...
		ima_add_ns_task(task_p->mm->exe_file, filename, mnt_ns_num);
	}

	return 0;
}

/*
 * ima_create_namespace - based on Trusted-Container.
 * @pid_ns: pointer to the pid namespace to be created
 *
 * create a cPCR towards this new mnt_namespace.
 *
 * On success return 0.  On error, return -1.
 */
 int ima_create_namespace(unsigned int mnt_ns_num)
 {
 	struct mnt_namespace_list *node;
 	struct list_head *pos;
 	struct mnt_namespace_list *p = NULL, *list = NULL;
 	struct mnt_namespace_hash_entry *entry;

 	unsigned long key;
 	int rc = -1;

 	printk("[Wu Luo] create a new namespace<proc_inum>[%u]!\n", mnt_ns_num);

 	// check whether the systemd creates this namespace, we should ignore that
 	if (!strcmp(current->real_parent->comm, "systemd")) {
 		printk("[Wu Luo] systemd creates a new mnt namespace, "
 				"we ignore it. %s<--->%s\n",
				current->comm, current->real_parent->comm);
 		return 0;
 	}

 	// check whether this namespace exists
 	list = ima_lookup_namespace_entry(mnt_ns_num);
 	if (list && list->cpcr && list->cpcr->tfm) {
 		printk("[Wu Luo][INFO] namespace exists. We do nothing currently\n");
 		return 0;
 	}

	printk("[Wu Luo] cpcr does not exist, create it...\n");
	node = (struct mnt_namespace_list*)kmalloc(sizeof(struct mnt_namespace_list), GFP_KERNEL);
	if(!node) {
		printk("[Wu Luo] failed to kmalloc struct mnt_namespace_list[%u]\n",
				sizeof(struct mnt_namespace_list));
		return 0;//-1;
	}
	node->cpcr = kmalloc(sizeof(struct cPCR), GFP_KERNEL);
	if(!node->cpcr)
		goto out;

	node->cpcr->tfm = crypto_alloc_shash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(node->cpcr->tfm)) {
		rc = PTR_ERR(node->cpcr->tfm);
		printk("[Wu Luo] ERROR: Can not allocate tfm (reason: %d)\n",
			   "sha1", rc);
		goto out;
	}

	memset(node->cpcr->data, 0x00, CPCR_DATA_SIZE);

	node->proc_inum = mnt_ns_num;

	// record all measurement events in this namespace
	INIT_LIST_HEAD(&node->measurements);

	if(ima_create_measurement_log(node) != 0)
		goto out;

	if(!mnt_ns_list.list.next) {
		INIT_LIST_HEAD(&mnt_ns_list.list);
	}
	list_add_tail(&node->list, &mnt_ns_list.list);
	printk("[Wu Luo] ns add into mnt_ns_list\n");
	printk("[Wu Luo] list test!");
	list_for_each(pos, &mnt_ns_list.list) {
		p = list_entry(pos, struct mnt_namespace_list, list);
		printk("\t-> %u ", p->proc_inum);
	}

	// Update the hash table
	atomic_long_inc(&ima_ns_htable.len);
	key = ima_hash_ns_key(node->proc_inum);

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL) {
		printk("[Wu Luo] failed to kmalloc struct mnt_namespace_hash_entry[%u]\n",
				sizeof(struct mnt_namespace_hash_entry));
		goto out;
	}
	entry->ns_list = node;
	INIT_HLIST_NODE(&entry->hnext);
	hlist_add_head_rcu(&entry->hnext, &ima_ns_htable.queue[key]);

	// record the dummy program for Trusted Container
	printk("[Wu Luo][DEBUG] the parent process is %s<--->%s\n",
			current->comm, current->real_parent->comm);
	ima_record_task_for_ns(mnt_ns_num);

	return 0;

out:
	if (node) {
		if (node->cpcr)
			if (!IS_ERR(node->cpcr->tfm))
				kfree(node->cpcr->tfm);
			kfree(node->cpcr);
		kfree(node);
	}
 	return 0;
}
EXPORT_SYMBOL_GPL(ima_create_namespace);

int __init ima_init_cpcr_structures(void) {
	int rc = 0;

	cpcr_for_history.tfm = crypto_alloc_shash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(cpcr_for_history.tfm)) {
		rc = PTR_ERR(cpcr_for_history.tfm);
		printk("[Wu Luo] ERROR: Can not allocate tfm (reason: %d)\n",
			   "sha1", rc);
		return -1;
	}

	memset(cpcr_for_history.data, 0x00, CPCR_DATA_SIZE);

	return 0;
}

static int __init init_ima(void)
{
	int error;

	hash_setup(CONFIG_IMA_DEFAULT_HASH);
	error = ima_init();
	if (!error)
		ima_initialized = 1;
	return error;
}

late_initcall(init_ima);	/* Start IMA after the TPM is available */

MODULE_DESCRIPTION("Integrity Measurement Architecture");
MODULE_LICENSE("GPL");
