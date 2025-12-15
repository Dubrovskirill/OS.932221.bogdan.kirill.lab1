#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BKS");
MODULE_DESCRIPTION("TSU Lab Module showing Voyager 2 distance.");

#define PROCFS_NAME "tsulab" 
static struct proc_dir_entry* proc_file = NULL;

static ssize_t proc_read(struct file* file, char __user* buffer, size_t len, loff_t* offset)
{
    char message[256]; 
    size_t msg_len;
    
    
    // Базовое расстояние в км (на момент 15.12.2025)
    long long base_distance_km = 21215499149LL;

    u64 seconds_elapsed = jiffies_to_msecs(jiffies) / 1000;
    
    
    long long distance_increase_km = (long long)seconds_elapsed * 15; 
    
    long long current_distance_km = base_distance_km + distance_increase_km;
    
    msg_len = scnprintf(message, sizeof(message),
        "Voyager 2 Distance: %lld km\n(Approx. %lld AU)\n", 
        current_distance_km, (current_distance_km / 149597870)); 
        

    if (*offset >= msg_len) {
        return 0; 
    }

    if (copy_to_user(buffer, message, msg_len)) {
        pr_err("Failed to copy data to user space\n");
        return -EFAULT;
    }

    *offset += msg_len; 
    return msg_len; 
}


static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read, 
};

static int __init tsu_init(void)
{
   
    pr_info("Welcome to the Tomsk State University\n"); 

    // Требование Части 2: Создание файла /proc/tsulab
    proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_ops); 
    if (!proc_file) {
        pr_err("Failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM; 
    }

    pr_info("/proc/%s created successfully with Voyager 2 data\n", PROCFS_NAME);
    return 0; 
}


static void __exit tsu_exit(void)
{

    if (proc_file) {
        proc_remove(proc_file); 
        pr_info("/proc/%s removed\n", PROCFS_NAME);
    }

    pr_info("Tomsk State University forever!\n");
}

module_init(tsu_init);
module_exit(tsu_exit);