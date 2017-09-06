#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/slab.h>

const char *ima_d_path(struct path *path, char **pathbuf)
{
	char *pathname = NULL;

	/* We will allow 11 spaces for ' (deleted)' to be appended */
	*pathbuf = kmalloc(PATH_MAX + 11, GFP_KERNEL);
	if (*pathbuf) {
		pathname = d_path(path, *pathbuf, PATH_MAX + 11);
		if (IS_ERR(pathname)) {
			kfree(*pathbuf);
			*pathbuf = NULL;
			pathname = NULL;
		}
	}
	return pathname;
}

static int show_files_by_current(void) {
	struct files_struct *files_p;
	struct fdtable *files_table;
	struct path files_path;
	const unsigned char *cwd;
	char *buf = NULL;
	int i = 0;

	files_p = current->real_parent->files;
	files_table = files_fdtable(files_p);

	buf = kmalloc(PATH_MAX + 11, GFP_KERNEL);
	if (!buf) {
		printk("kmalloc failed.\n");
		return -1;
	}

	printk(">>>> hello? %s <----> %s <------> %s\n", current->comm, 
		current->real_parent->comm, current->mm->exe_file->f_path.dentry->d_name.name);
	while(files_table->fd[i] != NULL) {
		files_path = files_table->fd[i]->f_path;
		cwd = ima_d_path(&files_path, &buf);
		if (!cwd)
			cwd = (const char *)files_table->fd[i]->f_path.dentry->d_name.name;
 		printk(KERN_ALERT "Open file with fd %d %s \n",i,cwd);
		i = i + 1;
		//break;
	}
	return 0;
}

static int __init hello_init(void)
{
	printk(KERN_INFO "hello\n");
	show_files_by_current();
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_INFO "Goodbye\n");
}

module_init(hello_init);
module_exit(hello_exit);
