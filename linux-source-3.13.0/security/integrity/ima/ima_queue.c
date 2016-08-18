/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Serge Hallyn <serue@us.ibm.com>
 * Reiner Sailer <sailer@watson.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_queue.c
 *       Implements queues that store template measurements and
 *       maintains aggregate over the stored measurements
 *       in the pre-configured TPM PCR (if available).
 *       The measurement list is append-only. No entry is
 *       ever removed or changed during the boot-cycle.
 */
#include <linux/module.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/pid_namespace.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <crypto/hash.h>
#include <crypto/hash_info.h>
#include "ima.h"

#define AUDIT_CAUSE_LEN_MAX 32

LIST_HEAD(ima_measurements);	/* list of all measurements */

/* key: inode (before secure-hashing a file) */
struct ima_h_table ima_htable = {
	.len = ATOMIC_LONG_INIT(0),
	.violations = ATOMIC_LONG_INIT(0),
	.queue[0 ... IMA_MEASURE_HTABLE_SIZE - 1] = HLIST_HEAD_INIT
};

/* mutex protects atomicity of extending measurement list
 * and extending the TPM PCR aggregate. Since tpm_extend can take
 * long (and the tpm driver uses a mutex), we can't use the spinlock.
 */
static DEFINE_MUTEX(ima_extend_list_mutex);

/* lookup up the digest value in the hash table, and return the entry */
static struct ima_queue_entry *ima_lookup_digest_entry(u8 *digest_value)
{
	struct ima_queue_entry *qe, *ret = NULL;
	unsigned int key;
	int rc;

	key = ima_hash_key(digest_value);
	rcu_read_lock();
	hlist_for_each_entry_rcu(qe, &ima_htable.queue[key], hnext) {
		rc = memcmp(qe->entry->digest, digest_value, TPM_DIGEST_SIZE);
		if (rc == 0) {
			ret = qe;
			break;
		}
	}
	rcu_read_unlock();
	return ret;
}

/* ima_add_template_entry helper function:
 * - Add template entry to measurement list and hash table.
 *
 * (Called with ima_extend_list_mutex held.)
 */
static int ima_add_digest_entry(struct ima_template_entry *entry)
{
	struct ima_queue_entry *qe;
	unsigned int key;

	qe = kmalloc(sizeof(*qe), GFP_KERNEL);
	if (qe == NULL) {
		pr_err("IMA: OUT OF MEMORY ERROR creating queue entry.\n");
		return -ENOMEM;
	}
	qe->entry = entry;

	INIT_LIST_HEAD(&qe->later);
	list_add_tail_rcu(&qe->later, &ima_measurements);

	atomic_long_inc(&ima_htable.len);
	key = ima_hash_key(entry->digest);
	hlist_add_head_rcu(&qe->hnext, &ima_htable.queue[key]);
	return 0;
}

/* For Trusted Container Begin*/

/* extend to the related cPCR and set the iterative value into PCR */
static int ima_cpcr_extend(const u8 *hash, struct pid_namespace *ns) {

	int i;
	int rc = 0;

	struct {
		struct shash_desc shash;
		char ctx[crypto_shash_descsize(ns->cpcr->tfm)];
	} desc;

	desc.shash.tfm = ns->cpcr->tfm;
	desc.shash.flags = 0;

	rc = crypto_shash_init(&desc.shash);
	if (rc != 0)
		return rc;

	rc = crypto_shash_update(&desc.shash, ns->cpcr->data, sizeof(ns->cpcr->data));

	rc = crypto_shash_update(&desc.shash, hash, sizeof(hash));
	if (!rc)
		crypto_shash_final(&desc.shash, ns->cpcr->data);

	printk("[Wu Luo] extend cpcr for ns[%u]:", ns->proc_inum);
	for (i = 0; i < 20; i++) {
		printk( "%02x\t", ns->cpcr->data[i]);
	}
	printk("\n HASH: ");
	for (i = 0; i < 20; i++) {
		printk( "%02x\t", hash[i]);
	}
	printk("\n");

	return rc;
}

/* For Trusted Container End*/

static int ima_pcr_extend(const u8 *hash)
{
	int result = 0;

	if (!ima_used_chip)
		return result;

	result = tpm_pcr_extend(TPM_ANY_NUM, CONFIG_IMA_MEASURE_PCR_IDX, hash);
	if (result != 0)
		pr_err("IMA: Error Communicating to TPM chip, result: %d\n",
		       result);
	return result;
}

/* Add template entry to the measurement list and hash table,
 * and extend the pcr.
 */
int ima_add_template_entry(struct ima_template_entry *entry, int violation,
			   const char *op, struct inode *inode,
			   const unsigned char *filename,
			   struct pid_namespace *ns)
{
	u8 digest[TPM_DIGEST_SIZE];
	const char *audit_cause = "hash_added";
	char tpm_audit_cause[AUDIT_CAUSE_LEN_MAX];
	int audit_info = 1;
	int result = 0, tpmresult = 0;

	mutex_lock(&ima_extend_list_mutex);
	if (!violation) {
		memcpy(digest, entry->digest, sizeof digest);
		if (ima_lookup_digest_entry(digest)) {
			audit_cause = "hash_exists";
			result = -EEXIST;
			goto out;
		}
	}

	result = ima_add_digest_entry(entry);
	if (result < 0) {
		audit_cause = "ENOMEM";
		audit_info = 0;
		goto out;
	}

	if (violation)		/* invalidate pcr */
		memset(digest, 0xff, sizeof digest);

	// extend cPCR
	printk("[Wu Luo] check ns exists %s?\n", filename);

	if(ns && ns->cpcr && ns->cpcr->tfm) {
		printk("[Wu Luo] prepared to extend cPCR\n");
		tpmresult = 0;
		if(IS_ERR(ns->cpcr->tfm)) {
			printk("[Wu Luo] cpcr->tfm not exists, alloc it...\n");
			ns->cpcr->tfm = crypto_alloc_shash("sha1", 0, CRYPTO_ALG_ASYNC);
			if (IS_ERR(ns->cpcr->tfm)) {
				tpmresult = PTR_ERR(ns->cpcr->tfm);
				printk("[Wu Luo] ERROR: Can not allocate %s (reason: %d)\n",
					   "sha1", tpmresult);
			}
		}

		if(tpmresult == 0) {
			tpmresult = ima_cpcr_extend(digest, ns);
			if(tpmresult != 0) {
				printk("[Wu Luo] failed to extend cPCR\n");
			}
		}
	}

	printk("[Wu Luo] ima_cpcr_extend succeed");

	tpmresult = ima_pcr_extend(digest);

	if (tpmresult != 0) {
		snprintf(tpm_audit_cause, AUDIT_CAUSE_LEN_MAX, "TPM_error(%d)",
			 tpmresult);
		audit_cause = tpm_audit_cause;
		audit_info = 0;
	}

out:
	mutex_unlock(&ima_extend_list_mutex);
	integrity_audit_msg(AUDIT_INTEGRITY_PCR, inode, filename,
			    op, audit_cause, result, audit_info);
	return result;
}
