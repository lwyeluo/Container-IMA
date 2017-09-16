/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Kylene Hall <kjhall@us.ibm.com>
 * Reiner Sailer <sailer@us.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_fs.c
 *	implemenents security file system for reporting
 *	current measurement list and IMA statistics
 */
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/parser.h>

#include <linux/mount.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/fs.h>

#include "ima.h"

// modified the struct of ima_measurements, for that
//	we have multiple measurement lists, e.g. PCR10 for the default IMA,
//	PCR11 for kernel modules...
struct ima_mesurement_list {
	int pcr_idx;
	struct list_head* measurements;
};

static int valid_policy = 1;
#define TMPBUFLEN 12
static ssize_t ima_show_htable_value(char __user *buf, size_t count,
				     loff_t *ppos, atomic_long_t *val)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t len;

	len = scnprintf(tmpbuf, TMPBUFLEN, "%li\n", atomic_long_read(val));
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, len);
}

static ssize_t ima_show_htable_violations(struct file *filp,
					  char __user *buf,
					  size_t count, loff_t *ppos)
{
	return ima_show_htable_value(buf, count, ppos, &ima_htable.violations);
}

static const struct file_operations ima_htable_violations_ops = {
	.read = ima_show_htable_violations,
	.llseek = generic_file_llseek,
};

static ssize_t ima_show_measurements_count(struct file *filp,
					   char __user *buf,
					   size_t count, loff_t *ppos)
{
	return ima_show_htable_value(buf, count, ppos, &ima_htable.len);

}

static const struct file_operations ima_measurements_count_ops = {
	.read = ima_show_measurements_count,
	.llseek = generic_file_llseek,
};

/* returns pointer to hlist_node */
static void *ima_measurements_start(struct seq_file *m, loff_t *pos)
{
	loff_t l = *pos;
	struct ima_queue_entry *qe;

	// point to the corresponding measument list
	struct ima_mesurement_list* node = m->private;

	if (node == NULL)
		return NULL;

	/* we need a lock since pos could point beyond last element */
	rcu_read_lock();
	list_for_each_entry_rcu(qe, node->measurements, later) {
		if (!l--) {
			rcu_read_unlock();
			return qe;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static void *ima_measurements_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ima_queue_entry *qe = v;

	// point to the corresponding measument list
	struct ima_mesurement_list* node = m->private;
	if (node == NULL)
		return NULL;

	/* lock protects when reading beyond last element
	 * against concurrent list-extension
	 */
	rcu_read_lock();
	qe = list_entry_rcu(qe->later.next, struct ima_queue_entry, later);
	rcu_read_unlock();
	(*pos)++;

	return (&qe->later == node->measurements) ? NULL : qe;
}

static void ima_measurements_stop(struct seq_file *m, void *v)
{
}

void ima_putc(struct seq_file *m, void *data, int datalen)
{
	while (datalen--)
		seq_putc(m, *(char *)data++);
}

/* print format:
 *       32bit-le=pcr#
 *       char[20]=template digest
 *       32bit-le=template name size
 *       char[n]=template name
 *       [eventdata length]
 *       eventdata[n]=template specific data
 */
static int ima_measurements_show(struct seq_file *m, void *v)
{
	/* the list never shrinks, so we don't need a lock here */
	struct ima_queue_entry *qe = v;
	struct ima_template_entry *e;
	int namelen;
	u32 pcr = CONFIG_IMA_MEASURE_PCR_IDX;
	bool is_ima_template = false;
	int i;

	// point to the corresponding measument list
	struct ima_mesurement_list* node = m->private;
	if (node == NULL)
		return -1;

	pcr = node->pcr_idx;

	/* get entry */
	e = qe->entry;
	if (e == NULL)
		return -1;

	/*
	 * 1st: PCRIndex
	 * PCR used is always the same (config option) in
	 * little-endian format
	 */
	ima_putc(m, &pcr, sizeof pcr);

	/* 2nd: template digest */
	ima_putc(m, e->digest, TPM_DIGEST_SIZE);

	/* 3rd: template name size */
	namelen = strlen(e->template_desc->name);
	ima_putc(m, &namelen, sizeof namelen);

	/* 4th:  template name */
	ima_putc(m, e->template_desc->name, namelen);

	/* 5th:  template length (except for 'ima' template) */
	if (strcmp(e->template_desc->name, IMA_TEMPLATE_IMA_NAME) == 0)
		is_ima_template = true;

	if (!is_ima_template)
		ima_putc(m, &e->template_data_len,
			 sizeof(e->template_data_len));

	/* 6th:  template specific data */
	for (i = 0; i < e->template_desc->num_fields; i++) {
		enum ima_show_type show = IMA_SHOW_BINARY;
		struct ima_template_field *field = e->template_desc->fields[i];

		if (is_ima_template && strcmp(field->field_id, "d") == 0)
			show = IMA_SHOW_BINARY_NO_FIELD_LEN;
		if (is_ima_template && strcmp(field->field_id, "n") == 0)
			show = IMA_SHOW_BINARY_OLD_STRING_FMT;
		field->field_show(m, show, &e->template_data[i]);
	}
	return 0;
}

static const struct seq_operations ima_measurments_seqops = {
	.start = ima_measurements_start,
	.next = ima_measurements_next,
	.stop = ima_measurements_stop,
	.show = ima_measurements_show
};

static int ima_measurements_open(struct inode *inode, struct file *file)
{
	struct seq_file *p = NULL;
	struct ima_measurement_list* node = inode->i_private;
	int ret = -1;

	if (!node) {
		printk("[Wu Luo][ERROR] do not refer to a ima_measurement_list\n");
		return -1;
	}

	ret = seq_open(file, &ima_measurments_seqops);
	if (ret != 0)
		return ret;

	p = file->private_data;

	p->private = node;

	return 0;
}

static const struct file_operations ima_measurements_ops = {
	.open = ima_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

void ima_print_digest(struct seq_file *m, u8 *digest, u32 size)
{
	u32 i;

	for (i = 0; i < size; i++)
		seq_printf(m, "%02x", *(digest + i));
}

/* print in ascii */
static int ima_ascii_measurements_show(struct seq_file *m, void *v)
{
	/* the list never shrinks, so we don't need a lock here */
	struct ima_queue_entry *qe = v;
	struct ima_template_entry *e;
	int i;

	// point to the corresponding measument list
	struct ima_mesurement_list* node = m->private;

	if (node == NULL)
		return -1;

	/* get entry */
	e = qe->entry;
	if (e == NULL)
		return -1;

	/* 1st: PCR used (config option) */
	seq_printf(m, "%2d ", node->pcr_idx);

	/* 2nd: SHA1 template hash */
	ima_print_digest(m, e->digest, TPM_DIGEST_SIZE);

	/* 3th:  template name */
	seq_printf(m, " %s", e->template_desc->name);

	/* 4th:  template specific data */
	for (i = 0; i < e->template_desc->num_fields; i++) {
		seq_puts(m, " ");
		if (e->template_data[i].len == 0)
			continue;

		e->template_desc->fields[i]->field_show(m, IMA_SHOW_ASCII,
							&e->template_data[i]);
	}
	seq_puts(m, "\n");
	return 0;
}

static const struct seq_operations ima_ascii_measurements_seqops = {
	.start = ima_measurements_start,
	.next = ima_measurements_next,
	.stop = ima_measurements_stop,
	.show = ima_ascii_measurements_show
};

static int ima_ascii_measurements_open(struct inode *inode, struct file *file)
{
	struct seq_file *p = NULL;
	struct ima_measurement_list* node = inode->i_private;
	int ret = -1;

	if (!node) {
		printk("[Wu Luo][ERROR] do not refer to a ima_measurement_list\n");
		return -1;
	}

	ret = seq_open(file, &ima_ascii_measurements_seqops);
	if(ret != 0)
		return ret;

	p = file->private_data;

	p->private = node;

	return 0;
}

static const struct file_operations ima_ascii_measurements_ops = {
	.open = ima_ascii_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static ssize_t ima_write_policy(struct file *file, const char __user *buf,
				size_t datalen, loff_t *ppos)
{
	char *data = NULL;
	ssize_t result;

	if (datalen >= PAGE_SIZE)
		datalen = PAGE_SIZE - 1;

	/* No partial writes. */
	result = -EINVAL;
	if (*ppos != 0)
		goto out;

	result = -ENOMEM;
	data = kmalloc(datalen + 1, GFP_KERNEL);
	if (!data)
		goto out;

	*(data + datalen) = '\0';

	result = -EFAULT;
	if (copy_from_user(data, buf, datalen))
		goto out;

	result = ima_parse_add_rule(data);
out:
	if (result < 0)
		valid_policy = 0;
	kfree(data);
	return result;
}

static struct dentry *ima_dir;
static struct dentry *binary_runtime_measurements;
static struct dentry *kernel_module_measurements;
static struct dentry *ascii_runtime_measurements;
static struct dentry *runtime_measurements_count;
static struct dentry *violations;
static struct dentry *ima_policy;
static struct dentry *ima_cpcr;

static atomic_t policy_opencount = ATOMIC_INIT(1);
/*
 * ima_open_policy: sequentialize access to the policy file
 */
static int ima_open_policy(struct inode * inode, struct file * filp)
{
	/* No point in being allowed to open it if you aren't going to write */
	if (!(filp->f_flags & O_WRONLY))
		return -EACCES;
	if (atomic_dec_and_test(&policy_opencount))
		return 0;
	return -EBUSY;
}

/*
 * ima_release_policy - start using the new measure policy rules.
 *
 * Initially, ima_measure points to the default policy rules, now
 * point to the new policy rules, and remove the securityfs policy file,
 * assuming a valid policy.
 */
static int ima_release_policy(struct inode *inode, struct file *file)
{
	if (!valid_policy) {
		ima_delete_rules();
		valid_policy = 1;
		atomic_set(&policy_opencount, 1);
		return 0;
	}
	ima_update_policy();
	securityfs_remove(ima_policy);
	ima_policy = NULL;
	return 0;
}

static const struct file_operations ima_measure_policy_ops = {
	.open = ima_open_policy,
	.write = ima_write_policy,
	.release = ima_release_policy,
	.llseek = generic_file_llseek,
};

/* For Trusted Container Begin */

/* returns pointer to hlist_node */
static void *ima_ns_measurements_start(struct seq_file *m, loff_t *pos)
{
	loff_t l = *pos;
	struct ima_queue_entry *qe;

	// point to all measument list in this namespace
	struct mnt_namespace_list* node = m->private;

	if (node == NULL)
		return NULL;

//	printk("[Wu Luo] show measurements log for [%u]\n",
//				node->ns->proc_inum);

	/* we need a lock since pos could point beyond last element */
	rcu_read_lock();
	list_for_each_entry_rcu(qe, &node->measurements, later) {
		if (!l--) {
			rcu_read_unlock();
			return qe;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static void *ima_ns_measurements_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ima_queue_entry *qe = v;

	// point to all measument list in this namespace
	struct mnt_namespace_list* node = m->private;

	if (node == NULL)
		return NULL;

	/* lock protects when reading beyond last element
	 * against concurrent list-extension
	 */
	rcu_read_lock();
	qe = list_entry_rcu(qe->later.next, struct ima_queue_entry, later);
	rcu_read_unlock();
	(*pos)++;

	return (&qe->later == &node->measurements) ? NULL : qe;
}

/* print in ascii */
static int ima_ns_measurements_show(struct seq_file *m, void *v)
{
	/* the list never shrinks, so we don't need a lock here */
	struct ima_queue_entry *qe = v;
	struct ima_template_entry *e;
	int i;

	// point to all measument list in this namespace
	struct mnt_namespace_list* node = m->private;

	if (node == NULL)
		return -1;

	/* get entry */
	e = qe->entry;
	if (e == NULL)
		return -1;

	/* 1st: PCR used (config option) */
	seq_printf(m, "%u ", node->proc_inum);

	/* 2nd: SHA1 template hash */
	ima_print_digest(m, e->digest, TPM_DIGEST_SIZE);

	/* 3th:  template name */
	seq_printf(m, " %s", e->template_desc->name);

	/* 4th:  template specific data */
	for (i = 0; i < e->template_desc->num_fields; i++) {
		seq_puts(m, " ");
		if (e->template_data[i].len == 0)
			continue;

		e->template_desc->fields[i]->field_show(m, IMA_SHOW_ASCII,
							&e->template_data[i]);
	}
	seq_puts(m, "\n");
	return 0;
}

static const struct seq_operations ima_ns_measurements_seqops = {
	.start = ima_ns_measurements_start,
	.next = ima_ns_measurements_next,
	.stop = ima_measurements_stop,
	.show = ima_ns_measurements_show
};

static int ima_ns_measurements_open(struct inode *inode, struct file *file)
{
	struct seq_file *p = NULL;
	int ret = -1;

	struct mnt_namespace_list* node = inode->i_private;

	ret = seq_open(file, &ima_ns_measurements_seqops);
	if(ret != 0)
		return ret;

	p = file->private_data;

	p->private = node;
	printk("[Wu Luo] allocate file's priavte data:[%u]\n",
			node->proc_inum);

	return 0;
}

static const struct file_operations ima_ns_measurements_ops = {
	.open = ima_ns_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* create a new measure_log for a new namespace
 * If success, return 0, otherwise return -1
 */
int ima_create_measurement_log(struct mnt_namespace_list* node) {
	char file_name[20] = ""; //it will convert from a unsigned int

	printk("[Wu Luo] enter ima_create_measurement_log\n");
	if(!node)
		return -1;

	sprintf(file_name, "%u", node->proc_inum);

	node->measurement_log = securityfs_create_file(file_name,
			   S_IRUSR | S_IRGRP, ima_dir, node,
			   &ima_ns_measurements_ops);
	if (IS_ERR(node->measurement_log))
			return -1;
	return 0;
}

/* for cpcr */

static void *ima_cpcr_measurements_start(struct seq_file *m, loff_t *pos)
{
	loff_t l = *pos;
	struct mnt_namespace_list *qe;
	int i = 0;

	/* we need a lock since pos could point beyond last element */
	rcu_read_lock();

	seq_puts(m, "history ");
	for (i = 0; i < CPCR_DATA_SIZE; i ++) {
		seq_printf(m, "%02x", cpcr_for_history.data[i]);
	}
	seq_puts(m, "\n");

	list_for_each_entry_rcu(qe, &mnt_ns_list.list, list) {
		if (!l--) {
			rcu_read_unlock();
			return qe;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static void *ima_cpcr_measurements_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct mnt_namespace_list *qe = v;

	/* lock protects when reading beyond last element
	 * against concurrent list-extension
	 */
	rcu_read_lock();
	qe = list_entry_rcu(qe->list.next, struct mnt_namespace_list, list);
	rcu_read_unlock();
	(*pos)++;

	return (&qe->list == &mnt_ns_list.list) ? NULL : qe;
}

static void ima_cpcr_measurements_stop(struct seq_file *m, void *v)
{
}

/* print in ascii */
static int ima_cpcr_measurements_show(struct seq_file *m, void *v)
{
	/* the list never shrinks, so we don't need a lock here */
	struct mnt_namespace_list *qe = v;

	int i;

	if(qe && qe->cpcr) {
		seq_printf(m, "%u ", qe->proc_inum);
		for (i = 0; i < CPCR_DATA_SIZE; i ++) {
			seq_printf(m, "%02x", qe->cpcr->data[i]);
		}
		seq_puts(m, "\n");
	} else
		return -1;

	return 0;
}


static const struct seq_operations ima_cpcr_measurements_seqops = {
	.start = ima_cpcr_measurements_start,
	.next = ima_cpcr_measurements_next,
	.stop = ima_cpcr_measurements_stop,
	.show = ima_cpcr_measurements_show
};

static int ima_cpcr_measurements_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ima_cpcr_measurements_seqops);
}

static const struct file_operations ima_cpcr_measurements_ops = {
	.open = ima_cpcr_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* For Trusted Container End */

int __init ima_fs_init(void)
{
	struct ima_mesurement_list* ima_kernel_ml = NULL;
	struct ima_mesurement_list* ima_default_ml = NULL;

	ima_dir = securityfs_create_dir("ima", NULL);
	if (IS_ERR(ima_dir))
		return -1;

	ima_default_ml = kmalloc(sizeof(struct ima_mesurement_list), GFP_KERNEL);
	if(!ima_default_ml) {
		printk("[Wu Luo] failed to kmalloc struct ima_mesurement_list\n");
		goto out;
	}
	ima_default_ml->pcr_idx = CONFIG_IMA_MEASURE_PCR_IDX;
	ima_default_ml->measurements = &ima_measurements;

	binary_runtime_measurements =
	    securityfs_create_file("binary_runtime_measurements",
				   S_IRUSR | S_IRGRP, ima_dir, ima_default_ml,
				   &ima_measurements_ops);
	if (IS_ERR(binary_runtime_measurements))
		goto out;

	ascii_runtime_measurements =
	    securityfs_create_file("ascii_runtime_measurements",
				   S_IRUSR | S_IRGRP, ima_dir, ima_default_ml,
				   &ima_ascii_measurements_ops);
	if (IS_ERR(ascii_runtime_measurements))
		goto out;

	ima_kernel_ml = kmalloc(sizeof(struct ima_mesurement_list), GFP_KERNEL);
	if(!ima_kernel_ml) {
		printk("[Wu Luo] failed to kmalloc struct ima_mesurement_list\n");
		goto out;
	}
	ima_kernel_ml->pcr_idx = CONFIG_IMA_KERNEL_MODULE_PCR_IDX;
	ima_kernel_ml->measurements = &ima_kernel_measurements;

	kernel_module_measurements =
		securityfs_create_file("kernel_module_measurements",
				   S_IRUSR | S_IRGRP, ima_dir, ima_kernel_ml,
				   &ima_ascii_measurements_ops);
	if (IS_ERR(kernel_module_measurements))
		goto out;

	runtime_measurements_count =
	    securityfs_create_file("runtime_measurements_count",
				   S_IRUSR | S_IRGRP, ima_dir, NULL,
				   &ima_measurements_count_ops);
	if (IS_ERR(runtime_measurements_count))
		goto out;

	violations =
	    securityfs_create_file("violations", S_IRUSR | S_IRGRP,
				   ima_dir, NULL, &ima_htable_violations_ops);
	if (IS_ERR(violations))
		goto out;

	ima_policy = securityfs_create_file("policy",
					    S_IWUSR,
					    ima_dir, NULL,
					    &ima_measure_policy_ops);
	if (IS_ERR(ima_policy))
		goto out;

	/* add cpcr */
	ima_cpcr = securityfs_create_file("cpcr",
			S_IRUSR | S_IRGRP, ima_dir, NULL,
			&ima_cpcr_measurements_ops
	);
	if(IS_ERR(ima_cpcr))
		goto out;

	return 0;
out:
	securityfs_remove(violations);
	securityfs_remove(runtime_measurements_count);
	securityfs_remove(ascii_runtime_measurements);
	securityfs_remove(binary_runtime_measurements);
	securityfs_remove(ima_dir);
	securityfs_remove(ima_policy);
	securityfs_remove(ima_cpcr);
	return -1;
}
