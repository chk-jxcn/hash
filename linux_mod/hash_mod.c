#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#include <asm/atomic.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "../src/hash.c"
#include "hash_mod.h"

static HashHandle g_handle = 0;
int make_rw(unsigned long address)
{
    unsigned int level;
    pte_t *pte = lookup_address(address, &level); //查找虚拟地址所在的页表地址
    if (pte->pte & ~_PAGE_RW)                     //设置页表读写属性
        pte->pte |= _PAGE_RW;

    return 0;
}

int make_ro(unsigned long address)
{
    unsigned int level;
    pte_t *pte = lookup_address(address, &level);
    pte->pte &= ~_PAGE_RW; //设置只读属性

    return 0;
}

asmlinkage long my_sys_mycall(CallContext* context)
{
    CallContext temp ;
    copy_from_user(&temp, context, sizeof(temp)); // just for test
    switch(temp.oper) {
        case kHashInsert:
        hash_insert(g_handle, temp.key, &temp.key, sizeof(temp.key),  &temp.value, sizeof(temp.value));
        break;
        case kHashFind:
        hash_find(g_handle, temp.key, &temp.key, sizeof(temp.key),  &temp.value, sizeof(temp.value));
        copy_to_user(context, &temp, sizeof(temp));
        break;
        case kHashErase:
        hash_erase(g_handle, temp.key, &temp.key, sizeof(temp.key));
        break;
        default:
        printk("<0>" "i am hack syscall! key:%d, value:%d\n",
           temp.key, temp.value);
        break;
    }
    
    return 0;
}

unsigned long **find_sys_call_table(void) {
    
    unsigned long ptr;
    unsigned long *p;

    for (ptr = (unsigned long)sys_close;
         ptr < (unsigned long)&loops_per_jiffy;
         ptr += sizeof(void *)) {
             
        p = (unsigned long *)ptr;

        if (p[__NR_close] == (unsigned long)sys_close) {
            printk(KERN_DEBUG "Found the sys_call_table!!!\n");
            return (unsigned long **)p;
        }
    }
    return 0;
}

static const unsigned int g_my_cid = OVER_WRITE_CALL_ID; // call id for overwrite
static unsigned long *g_syscall_addr_backup = 0;
static unsigned long *sys_call_table = 0;
int replace_syscall(void)
{
    sys_call_table = (unsigned long *)find_sys_call_table();
    if(sys_call_table == 0) {
        printk("<0>"
           "cannot find sys_call_table\n");
        return -1;
    }
    g_syscall_addr_backup = (unsigned long *)(sys_call_table[g_my_cid]); //保存原有的223号的系统调用表的地址
    make_rw((unsigned long)sys_call_table);
    sys_call_table[g_my_cid] = (unsigned long)my_sys_mycall;
    make_ro((unsigned long)sys_call_table);
    return 0;
}
int reset_syscall(void)
{
    make_rw((unsigned long)sys_call_table);
    sys_call_table[g_my_cid] = (unsigned long)g_syscall_addr_backup;
    make_ro((unsigned long)sys_call_table);
    return 0;
}

static struct proc_dir_entry *tempdir = 0, *processinfo_file = 0;
static int proc_read_processinfo(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
    int ret = 0;
    Hash* hash = (Hash*)g_handle;
    static char buf[1024] = {0};

    if (offset > 0)
    {
        return 0;
    }

    ret = snprintf(buf, sizeof(buf), "current_size:%ld, max_size:%ld, max_hash_node_size:%ld\r\n"
                                     "vmalloc:%d,  vfree:%d, kmalloc:%d, kfree:%d\r\n"
                                     "stat: m_fail:%d, i_fail:%d: i_conf:%d, ei_fail:%d\r\n",
        hash->elem_container.size, hash->elem_container.max_size, hash->node_container->max_node_array_size,
        stat_item_value(&g_stat.vmalloc), stat_item_value(&g_stat.vfree),
        stat_item_value(&g_stat.kmalloc), stat_item_value(&g_stat.kfree),
        stat_item_value(&g_stat.malloc_failed),
        stat_item_value(&g_stat.insert_failed),
        stat_item_value(&g_stat.insert_conflict),
        stat_item_value(&g_stat.insert_failed_when_expand));
    *start = buf;
    return ret;
}


int __init
hash_module_init(void)
{
    int rv = 0;

    printk("<0>"
           "%s loaded\n",
           __FUNCTION__);
    g_handle=hash_malloc("default_table", 1024);
    if(!g_handle) {
        printk("<0>"
           "hash_malloc() failed\n");
        return -12;
    }
    if(replace_syscall() != 0) {
        printk("<0>"
           "replace_syscall() failed\n");
        return -1;
    }

    

    tempdir = proc_mkdir("myhash", NULL);
    if (tempdir == 0)
    {
        rv = -ENOMEM;
        return rv;
    }
    processinfo_file = create_proc_read_entry(hash_get_name(g_handle), 0444, tempdir, proc_read_processinfo, NULL);
    if (processinfo_file == NULL)
    {
        rv = -ENOMEM;
        remove_proc_entry("myhash", NULL);
        return rv;
    }
    return 0;
}

void __exit
hash_module_exit(void)
{
    printk("<0>"
           "%s unloaded\n",
           __FUNCTION__);
    remove_proc_entry(hash_get_name(g_handle), tempdir);
    remove_proc_entry("myhash", NULL);
    reset_syscall();
    hash_free(g_handle);
}

#ifdef __linux__
module_init(hash_module_init)
    module_exit(hash_module_exit)
        MODULE_LICENSE("Dual BSD/GPL"); /* the code here is all BSD. */
#endif
