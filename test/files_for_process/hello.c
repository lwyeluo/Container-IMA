#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/ima.h>
#include <linux/integrity.h>
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <crypto/hash.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#define TEMPLATE_NAME "ima"
#include <uapi/linux/limits.h>
u8 digest[20];
static struct crypto_shash *ima_shash_tfm;
char *ima_hash = "sha1";
char *str;
unsigned  char *buf = NULL;
struct ima_template_data {
    u8 digest[20];
    char file_name[256];
};
struct ima_template_entry {
    u8 digest[20];
    const char *template_name;
    int template_len;
    struct ima_template_data template;
};
static int init_desc(struct hash_desc *desc)
{
    int rc;
    desc->tfm = crypto_alloc_hash(ima_hash,0,CRYPTO_ALG_ASYNC);
    if(IS_ERR(desc->tfm)){
        pr_info("IMA: failed to load %s transform: %ld\n",ima_hash,PTR_ERR(desc->tfm));
        rc = PTR_ERR(desc->tfm);
        return rc;
    }
    desc->flags = 0;
    rc = crypto_hash_init(desc);
    if(rc)
        crypto_free_hash(desc->tfm);
    return rc;
}
ima_calc_template_hash(int template_len, void *template, char *digest)
{
    struct hash_desc desc;
    struct scatterlist sg[1];
    int rc;
    int ii;
    rc = init_desc(&desc);
    if(rc != 0)
        return rc;
    sg_init_one(sg,template,template_len);
    rc = crypto_hash_update(&desc, sg, template_len);
    if(!rc)
        rc = crypto_hash_final(&desc,digest);
    printk("Begin\n");
    for( ii = 0; ii < 20 ; ii++){
        printk("%02x", *(((u8 *)digest) + ii));
    }
    printk("\nThe end\n");
    crypto_free_hash(desc.tfm);
    return rc;
}
ssize_t container_write(struct file *filep, const char __user *buf,size_t count, loff_t *data)
{
#if 0
    unsigned long k;
    if(!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
        return -EACCES;
    kstrtoul_from_user(buf,count,0,&k);
#endif
    str = (char *)kzalloc(count + 1 ,GFP_KERNEL);
    if(str == NULL)
        return -ENOMEM;
    if(copy_from_user(str,buf,count)){
        kfree(str);
        return -EFAULT;
    }
    return count;
}
static int ima_init_crypto(void)
{
    long rc;
    ima_shash_tfm = crypto_alloc_shash(ima_hash,0,0);
    if(IS_ERR(ima_shash_tfm)){
        rc = PTR_ERR(ima_shash_tfm);
        pr_err("Can not allocate %s (reason: %ld)\n",ima_hash,rc);
        return rc;
    }
    return 0;
}
#if 1
int ima_calc_file_hash(struct file *file, char *digest)
{
    loff_t i_size,offset = 0;
    char *rbuf;
    int rc, read = 0;
    struct {
        struct shash_desc shash;
        char ctx[crypto_shash_descsize(ima_shash_tfm)];
    }desc;
    desc.shash.tfm = ima_shash_tfm;
    desc.shash.flags = 0;
    rc = crypto_shash_init(&desc.shash);
    if(rc != 0)
        return rc;
    rbuf = kzalloc(PAGE_SIZE,GFP_KERNEL);
    if(!rbuf){
        rc = -ENOMEM;
        goto out;
    }
    if( !(file->f_mode & FMODE_READ)){
        file->f_mode |= FMODE_READ;
        read = 1;
    }
    //i_size = i_size_read(file_inode(file));
    i_size = i_size_read(file->f_dentry->d_inode);
#if 1
    while (offset < i_size){
        int rbuf_len;
        rbuf_len = kernel_read(file,offset,rbuf,PAGE_SIZE);
        if(rbuf_len < 0){
            rc = rbuf_len;
            break;
        }
        if(rbuf_len == 0)
            break;
        offset += rbuf_len;
        rc = crypto_shash_update(&desc.shash, rbuf,rbuf_len);
        if(rc)
            break;
    }
#endif
    kfree(rbuf);
    if(!rc)
        rc = crypto_shash_final(&desc.shash,digest);
    if(read)
        file->f_mode &= ~FMODE_READ;
out:
    return rc;
}
#endif
static int container_proc_show(struct seq_file *m, void *v)
{
#if 1
    int ii;
    int i = 0 ;
    int rc;
    struct pid *pid_p;
    struct task_struct *task_p;
    struct files_struct *files_p;
    struct fdtable *files_table;
    struct path files_path;
    const unsigned char *cwd;
    struct ima_template_entry *entry;
    long res;
#endif
#if 1
    if(str != NULL){
        entry = kmalloc(sizeof(*entry),GFP_KERNEL);
        if(!entry){
            printk("entry: kmalloc()\n");
            return;
        }
        kstrtol(str,0,&res);
        pid_p = find_get_pid((pid_t)res);
        if(pid_p == NULL){
            printk("%d maybe exit\n",(pid_t)res);
            return -1;
        }
        task_p = pid_task(pid_p,0);
        printk("pid:%d\n",task_p->pid);
        files_p = task_p->files;
        files_table = files_fdtable(files_p);

	printk(">>> show process name: %s <-> %s <-> %s", task_p->comm, task_p->real_parent->comm,
		task_p->mm->exe_file->f_path.dentry->d_name.name);

        while( files_table->fd[i] != NULL) {
            files_path = files_table->fd[i]->f_path;
            cwd = d_path(&files_path,buf, PATH_MAX + 11);		
            if(cwd == NULL){
                printk("d_path\n");
                kfree(buf);
                return 0;
            }
            printk(KERN_ALERT "Open file with fd %d %s \n",i,cwd);
            if(i >= 3){
                rc = ima_calc_file_hash(files_table->fd[i],digest);
                memset(&(entry->template), 0, sizeof(entry->template));	
                memcpy(entry->template.digest, digest, 20);
                strcpy(entry->template.file_name, cwd);
                
                memset(entry->digest,0,sizeof(entry->digest));
                entry->template_name = TEMPLATE_NAME;
                entry->template_len = sizeof(entry->template);
                
                ima_calc_template_hash(entry->template_len,&(entry->template),entry->digest);	
                seq_printf(m,"10 ");
                for( ii = 0; ii < 20 ; ii++){
                                                seq_printf(m,"%02x", (entry->digest[ii]));
                                }
                seq_printf(m," ");
                seq_printf(m,"%s ",TEMPLATE_NAME);
                for( ii = 0; ii < 20 ; ii++){
                        seq_printf(m,"%02x", *(digest + ii));
                }
                seq_printf(m," %s\n", cwd);
            }
            i = i + 1;
        }
#endif
    }
    else
        printk("You should echo %d to /proc/container_hash\n", task_p->pid);
    return 0;
}
static int container_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file,container_proc_show, NULL);
}
static const struct file_operations container_proc_fops = {
    .owner = THIS_MODULE,
    .open = container_proc_open,
    .read = seq_read,
    .write = container_write,
    .llseek = seq_lseek,
    .release = single_release,
};
static int __init hello_init(void)
{
    pr_info("hello\n");
    proc_create_data("container_hash",0666,NULL,&container_proc_fops,NULL);
    pr_info("proc succeed\n");
    ima_init_crypto();
    buf = (char *)kmalloc(PATH_MAX + 11,GFP_KERNEL);
    /*error todo*/
    if(buf == NULL){
        printk("kmalloc() error\n");
        return 0;
    }
    return 0;
}
static void __exit hello_exit(void)
{
    printk("hello,I am leaving\n");
    remove_proc_entry("container_hash",NULL);
    kfree(buf);
    kfree(str);
}
module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("sam");
