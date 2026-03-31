#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include "anemometer.h"

#define ANEMOMETER_CHRDEV_NAME "anemometer"
#define ANEMOMETER_CHRDEV_MINORS 1

static int anemometer_chrdev_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int anemometer_chrdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t anemometer_chrdev_read(struct file *file, char __user *buf,
                                       size_t count, loff_t *offset)
{
    struct anemometer_sensor *sensor;
    char *kbuf;
    int len = 0;
    
    kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    mutex_lock(&anemometer_drv.sensors_lock);
    list_for_each_entry(sensor, &anemometer_drv.sensors, list) {
        mutex_lock(&sensor->lock);
        len += snprintf(kbuf + len, PAGE_SIZE - len,
                       "%s gpio=%d freq=%u.%03u speed=%d.%03d stale=%d\n",
                       sensor->name,
                       desc_to_gpio(sensor->gpio),
                       sensor->frequency_millihz / 1000,
                       sensor->frequency_millihz % 1000,
                       sensor->wind_speed_um_s / 1000000,
                       abs(sensor->wind_speed_um_s / 1000) % 1000,
                       sensor->stale);
        mutex_unlock(&sensor->lock);
        
        if (len >= PAGE_SIZE - 256)
            break;
    }
    mutex_unlock(&anemometer_drv.sensors_lock);
    
    if (*offset >= len) {
        kfree(kbuf);
        return 0;
    }
    
    if (count > len - *offset)
        count = len - *offset;
    
    if (copy_to_user(buf, kbuf + *offset, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    
    *offset += count;
    kfree(kbuf);
    return count;
}

static ssize_t anemometer_chrdev_write(struct file *file, const char __user *buf,
                                        size_t count, loff_t *offset)
{
    char *kbuf;
    char cmd[16], name[32];
    int ret;
    struct anemometer_sensor *sensor;
    
    if (count > 255)
        count = 255;
    
    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[count] = '\0';
    
    /* Parse command */
    if (sscanf(kbuf, "%15s", cmd) != 1) {
        kfree(kbuf);
        return -EINVAL;
    }
    
    if (!strcmp(cmd, "add")) {
        /* Parse: add name=<name> gpio=<gpio> [slope=<slope>] [window=<window>] */
        char *p, *token;
        char name_val[32] = "";
        u32 gpio_val = 0;
        u32 window_val = ANEMOMETER_DEFAULT_WINDOW_SIZE;
        
        p = kbuf + 3;  /* Skip "add" */
        while ((token = strsep(&p, " ")) != NULL) {
            if (strncmp(token, "name=", 5) == 0)
                strscpy(name_val, token + 5, sizeof(name_val));
            else if (strncmp(token, "gpio=", 5) == 0)
                kstrtou32(token + 5, 10, &gpio_val);
            else if (strncmp(token, "window=", 7) == 0)
                kstrtou32(token + 7, 10, &window_val);
        }
        
        if (!name_val[0] || gpio_val == 0) {
            kfree(kbuf);
            return -EINVAL;
        }
        
        /* Check if exists */
        if (anemometer_sensor_find(name_val)) {
            kfree(kbuf);
            return -EEXIST;
        }
        
        /* Create sensor */
        sensor = anemometer_sensor_create(name_val);
        if (IS_ERR(sensor)) {
            kfree(kbuf);
            return PTR_ERR(sensor);
        }
        
        sensor->window_size = window_val;
        
        ret = anemometer_sensor_setup_gpio(sensor, gpio_val);
        if (ret) {
            anemometer_sensor_destroy(sensor);
            kfree(kbuf);
            return ret;
        }
        
        ret = anemometer_sensor_start(sensor);
        if (ret) {
            anemometer_sensor_destroy(sensor);
            kfree(kbuf);
            return ret;
        }
        
        pr_info("anemometer: created sensor '%s' via char device\n", name_val);
        
    } else if (!strcmp(cmd, "del")) {
        /* Parse: del <name> */
        if (sscanf(kbuf, "%*s %31s", name) != 1) {
            kfree(kbuf);
            return -EINVAL;
        }
        
        sensor = anemometer_sensor_find(name);
        if (!sensor) {
            kfree(kbuf);
            return -ENOENT;
        }
        
        anemometer_sensor_destroy(sensor);
        pr_info("anemometer: deleted sensor '%s' via char device\n", name);
        
    } else if (!strcmp(cmd, "list")) {
        /* List command just triggers a read */
        
    } else {
        kfree(kbuf);
        return -EINVAL;
    }
    
    kfree(kbuf);
    return count;
}

static const struct file_operations anemometer_chrdev_fops = {
    .owner = THIS_MODULE,
    .open = anemometer_chrdev_open,
    .release = anemometer_chrdev_release,
    .read = anemometer_chrdev_read,
    .write = anemometer_chrdev_write,
    .llseek = noop_llseek,
};

int anemometer_chrdev_init(void)
{
    int ret;
    
    ret = alloc_chrdev_region(&anemometer_drv.devt, 0, 
                               ANEMOMETER_CHRDEV_MINORS, 
                               ANEMOMETER_CHRDEV_NAME);
    if (ret) {
        pr_err("anemometer: failed to allocate char device region: %d\n", ret);
        return ret;
    }
    
    cdev_init(&anemometer_drv.cdev, &anemometer_chrdev_fops);
    anemometer_drv.cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&anemometer_drv.cdev, anemometer_drv.devt,
                   ANEMOMETER_CHRDEV_MINORS);
    if (ret) {
        pr_err("anemometer: failed to add char device: %d\n", ret);
        unregister_chrdev_region(anemometer_drv.devt, ANEMOMETER_CHRDEV_MINORS);
        return ret;
    }
    
    /* Create device node */
    device_create(anemometer_drv.class, NULL, anemometer_drv.devt, NULL,
                  ANEMOMETER_CHRDEV_NAME);
    
    pr_info("anemometer: char device registered at /dev/%s\n", ANEMOMETER_CHRDEV_NAME);
    return 0;
}

void anemometer_chrdev_exit(void)
{
    device_destroy(anemometer_drv.class, anemometer_drv.devt);
    cdev_del(&anemometer_drv.cdev);
    unregister_chrdev_region(anemometer_drv.devt, ANEMOMETER_CHRDEV_MINORS);
}
