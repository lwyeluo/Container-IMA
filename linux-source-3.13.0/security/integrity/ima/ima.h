/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Reiner Sailer <sailer@watson.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima.h
 *	internal Integrity Measurement Architecture (IMA) definitions
 */

#ifndef __LINUX_IMA_H
#define __LINUX_IMA_H

#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/security.h>
#include <linux/hash.h>
#include <linux/tpm.h>
#include <linux/audit.h>
#include <linux/mnt_namespace.h>

#include "../integrity.h"

enum ima_show_type { IMA_SHOW_BINARY, IMA_SHOW_BINARY_NO_FIELD_LEN,
		     IMA_SHOW_BINARY_OLD_STRING_FMT, IMA_SHOW_ASCII };
enum tpm_pcrs { TPM_PCR0 = 0, TPM_PCR8 = 8 };

/* digest size for IMA, fits SHA1 or MD5 */
#define IMA_DIGEST_SIZE		SHA1_DIGEST_SIZE
#define IMA_EVENT_NAME_LEN_MAX	255

#define IMA_HASH_BITS 9
#define IMA_MEASURE_HTABLE_SIZE (1 << IMA_HASH_BITS)

#define IMA_TEMPLATE_FIELD_ID_MAX_LEN	16
#define IMA_TEMPLATE_NUM_FIELDS_MAX	15

#define IMA_TEMPLATE_IMA_NAME "ima"
#define IMA_TEMPLATE_IMA_FMT "d|n"

/* set during initialization */
extern int ima_initialized;
extern int ima_used_chip;
extern int ima_hash_algo;
extern int ima_appraise;

/* IMA template field data definition */
struct ima_field_data {
	u8 *data;
	u32 len;
};

/* IMA template field definition */
struct ima_template_field {
	const char field_id[IMA_TEMPLATE_FIELD_ID_MAX_LEN];
	int (*field_init) (struct integrity_iint_cache *iint, struct file *file,
			   const unsigned char *filename,
			   struct evm_ima_xattr_data *xattr_value,
			   int xattr_len, struct ima_field_data *field_data);
	void (*field_show) (struct seq_file *m, enum ima_show_type show,
			    struct ima_field_data *field_data);
};

/* IMA template descriptor definition */
struct ima_template_desc {
	char *name;
	char *fmt;
	int num_fields;
	struct ima_template_field **fields;
};

struct ima_template_entry {
	u8 digest[TPM_DIGEST_SIZE];	/* sha1 or md5 measurement hash */
	struct ima_template_desc *template_desc; /* template descriptor */
	u32 template_data_len;
	struct ima_field_data template_data[0];	/* template related data */
};

struct ima_queue_entry {
	struct hlist_node hnext;	/* place in hash collision list */
	struct list_head later;		/* place in ima_measurements list */
	struct ima_template_entry *entry;
};

/* For Trusted Container Begin*/
#define CPCR_NULL_NAMESPACE	-1
#define CPCR_DATA_SIZE      20

struct cPCR {
	struct crypto_shash *tfm;
    unsigned char data[CPCR_DATA_SIZE];
    unsigned char secret[CPCR_DATA_SIZE];
};

/* list of all measurements */
extern struct list_head ima_measurements;
/* list of all kernel modules */
extern struct list_head ima_kernel_measurements;

/* list of all mnt_namespace, used to find cpcr through proc_num */
struct mnt_namespace_list {
	unsigned int proc_inum;
	struct cPCR* cpcr; /* pid namespace */
	struct entry* measurement_log; /* entry for securityfs */
	/* all measures for this namespace, it is similar with ima_measurements */
	struct list_head measurements;
	struct list_head list; /* place to connect all pid namespaces */
};

extern struct mnt_namespace_list mnt_ns_list;

// used to create a hash table to check whether a namespace belong to
//	our mnt_namespace_list
struct ima_ns_h_table {
	atomic_long_t len;	/* number of stored measurements in the list */
	struct hlist_head queue[IMA_MEASURE_HTABLE_SIZE];
};

extern struct ima_ns_h_table ima_ns_htable;

// a hash table to save all ns->proc_inum created by Trusted Container
//	namespace hook
struct mnt_namespace_hash_entry {
	struct hlist_node hnext;	/* place in hash collision list */
	struct mnt_namespace_list* ns_list; /* pid namespace */
};

/* create a new measure_log for a new namespace */
int ima_create_namespace(unsigned int mnt_ns_num);
/* create a new measure_log for a new namespace
 * If success, return 0, otherwise return -1
 */
int ima_create_measurement_log(struct mnt_namespace_list* node);
/* lookup up the digest value in the hash table, and return the entry */
struct mnt_namespace_list *ima_lookup_namespace_entry(unsigned int proc_inum);

/* record the history value of physical PCR to bind all cPCRs into
 *  a physical PCR, i.e. PCR12
 */
extern struct cPCR cpcr_for_history;

int ima_init_cpcr_structures(void);

/* For Trusted Container End*/

/* Internal IMA function definitions */
int ima_init(void);
void ima_cleanup(void);
int ima_fs_init(void);
void ima_fs_cleanup(void);
int ima_inode_alloc(struct inode *inode);
int ima_add_template_entry(struct ima_template_entry *entry, int violation,
			   const char *op, struct inode *inode,
			   const unsigned char *filename,
			   unsigned int mnt_ns_num, int function);
int ima_calc_file_hash(struct file *file, struct ima_digest_data *hash);
int ima_calc_field_array_hash(struct ima_field_data *field_data,
			      struct ima_template_desc *desc, int num_fields,
			      struct ima_digest_data *hash);
int __init ima_calc_boot_aggregate(struct ima_digest_data *hash);
void ima_add_violation(struct file *file, const unsigned char *filename,
		       const char *op, const char *cause);
int ima_init_crypto(void);
void ima_putc(struct seq_file *m, void *data, int datalen);
void ima_print_digest(struct seq_file *m, u8 *digest, u32 size);
struct ima_template_desc *ima_template_desc_current(void);
int ima_init_template(void);

int ima_init_template(void);

/*
 * used to protect h_table and sha_table
 */
extern spinlock_t ima_queue_lock;

struct ima_h_table {
	atomic_long_t len;	/* number of stored measurements in the list */
	atomic_long_t violations;
	struct hlist_head queue[IMA_MEASURE_HTABLE_SIZE];
};
extern struct ima_h_table ima_htable;

static inline unsigned long ima_hash_key(u8 *digest)
{
	return hash_long(*digest, IMA_HASH_BITS);
}

static inline unsigned long ima_hash_ns_key(unsigned int proc_inum)
{
	return hash_long(proc_inum, IMA_HASH_BITS);
}

/* LIM API function definitions */
int ima_get_action(struct inode *inode, int mask, int function);
int ima_must_measure(struct inode *inode, int mask, int function);
int ima_collect_measurement(struct integrity_iint_cache *iint,
			    struct file *file,
			    struct evm_ima_xattr_data **xattr_value,
			    int *xattr_len);
void ima_store_measurement(struct integrity_iint_cache *iint, struct file *file,
			   const unsigned char *filename,
			   struct evm_ima_xattr_data *xattr_value,
			   int xattr_len,
			   unsigned int mnt_ns_num, int function);
void ima_audit_measurement(struct integrity_iint_cache *iint,
			   const unsigned char *filename);
int ima_alloc_init_template(struct integrity_iint_cache *iint,
			    struct file *file, const unsigned char *filename,
			    struct evm_ima_xattr_data *xattr_value,
			    int xattr_len, struct ima_template_entry **entry);
int ima_store_template(struct ima_template_entry *entry, int violation,
		       struct inode *inode, const unsigned char *filename,
			   unsigned int mnt_ns_num, int function);
void ima_free_template_entry(struct ima_template_entry *entry);
const char *ima_d_path(struct path *path, char **pathbuf);

/* rbtree tree calls to lookup, insert, delete
 * integrity data associated with an inode.
 */
struct integrity_iint_cache *integrity_iint_insert(struct inode *inode);
struct integrity_iint_cache *integrity_iint_find(struct inode *inode);

/* IMA policy related functions */
enum ima_hooks { FILE_CHECK = 1, MMAP_CHECK, BPRM_CHECK, MODULE_CHECK, POST_SETATTR };

int ima_match_policy(struct inode *inode, enum ima_hooks func, int mask,
		     int flags);
void ima_init_policy(void);
void ima_update_policy(void);
ssize_t ima_parse_add_rule(char *);
void ima_delete_rules(void);

/* Appraise integrity measurements */
#define IMA_APPRAISE_ENFORCE	0x01
#define IMA_APPRAISE_FIX	0x02
#define IMA_APPRAISE_MODULES	0x04

#ifdef CONFIG_IMA_APPRAISE
int ima_appraise_measurement(int func, struct integrity_iint_cache *iint,
			     struct file *file, const unsigned char *filename,
			     struct evm_ima_xattr_data *xattr_value,
			     int xattr_len);
int ima_must_appraise(struct inode *inode, int mask, enum ima_hooks func);
void ima_update_xattr(struct integrity_iint_cache *iint, struct file *file);
enum integrity_status ima_get_cache_status(struct integrity_iint_cache *iint,
					   int func);
void ima_get_hash_algo(struct evm_ima_xattr_data *xattr_value, int xattr_len,
		       struct ima_digest_data *hash);
int ima_read_xattr(struct dentry *dentry,
		   struct evm_ima_xattr_data **xattr_value);

#else
static inline int ima_appraise_measurement(int func,
					   struct integrity_iint_cache *iint,
					   struct file *file,
					   const unsigned char *filename,
					   struct evm_ima_xattr_data *xattr_value,
					   int xattr_len)
{
	return INTEGRITY_UNKNOWN;
}

static inline int ima_must_appraise(struct inode *inode, int mask,
				    enum ima_hooks func)
{
	return 0;
}

static inline void ima_update_xattr(struct integrity_iint_cache *iint,
				    struct file *file)
{
}

static inline enum integrity_status ima_get_cache_status(struct integrity_iint_cache
							 *iint, int func)
{
	return INTEGRITY_UNKNOWN;
}

static inline void ima_get_hash_algo(struct evm_ima_xattr_data *xattr_value,
				     int xattr_len,
				     struct ima_digest_data *hash)
{
}

static inline int ima_read_xattr(struct dentry *dentry,
				 struct evm_ima_xattr_data **xattr_value)
{
	return 0;
}

#endif

/* LSM based policy rules require audit */
#ifdef CONFIG_IMA_LSM_RULES

#define security_filter_rule_init security_audit_rule_init
#define security_filter_rule_match security_audit_rule_match

#else

static inline int security_filter_rule_init(u32 field, u32 op, char *rulestr,
					    void **lsmrule)
{
	return -EINVAL;
}

static inline int security_filter_rule_match(u32 secid, u32 field, u32 op,
					     void *lsmrule,
					     struct audit_context *actx)
{
	return -EINVAL;
}
#endif /* CONFIG_IMA_LSM_RULES */
#endif
