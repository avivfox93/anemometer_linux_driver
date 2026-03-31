#include <linux/module.h>
#include <linux/configfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "anemometer.h"

#if IS_ENABLED(CONFIG_CONFIGFS_FS)

struct anemometer_config_item {
    struct config_item item;
    struct anemometer_sensor *sensor;
    u32 gpio_num;
    bool enabled;
};

static inline struct anemometer_config_item *to_anemometer_item(struct config_item *item)
{
    return container_of(item, struct anemometer_config_item, item);
}

static ssize_t anemometer_config_gpio_show(struct config_item *item, char *page)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    return sprintf(page, "%u\n", ci->gpio_num);
}

static ssize_t anemometer_config_gpio_store(struct config_item *item,
                                            const char *page, size_t count)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    u32 gpio;
    int ret;
    
    if (kstrtou32(page, 10, &gpio))
        return -EINVAL;
    
    if (ci->enabled)
        return -EBUSY;  /* Can't change GPIO while enabled */
    
    ci->gpio_num = gpio;
    return count;
}

CONFIGFS_ATTR(anemometer_config_, gpio);

static ssize_t anemometer_config_slope_num_show(struct config_item *item, char *page)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    if (ci->sensor)
        return sprintf(page, "%d\n", ci->sensor->slope_num);
    return sprintf(page, "%d\n", ANEMOMETER_DEFAULT_SLOPE_NUM);
}

static ssize_t anemometer_config_slope_num_store(struct config_item *item,
                                                  const char *page, size_t count)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    s32 val;
    
    if (kstrtos32(page, 10, &val))
        return -EINVAL;
    
    if (ci->sensor) {
        mutex_lock(&ci->sensor->lock);
        ci->sensor->slope_num = val;
        mutex_unlock(&ci->sensor->lock);
    }
    return count;
}

CONFIGFS_ATTR(anemometer_config_, slope_num);

static ssize_t anemometer_config_slope_den_show(struct config_item *item, char *page)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    if (ci->sensor)
        return sprintf(page, "%u\n", ci->sensor->slope_den);
    return sprintf(page, "%u\n", ANEMOMETER_DEFAULT_SLOPE_DEN);
}

static ssize_t anemometer_config_slope_den_store(struct config_item *item,
                                                  const char *page, size_t count)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    u32 val;
    
    if (kstrtou32(page, 10, &val))
        return -EINVAL;
    
    if (val == 0)
        return -EINVAL;
    
    if (ci->sensor) {
        mutex_lock(&ci->sensor->lock);
        ci->sensor->slope_den = val;
        mutex_unlock(&ci->sensor->lock);
    }
    return count;
}

CONFIGFS_ATTR(anemometer_config_, slope_den);

static ssize_t anemometer_config_offset_show(struct config_item *item, char *page)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    if (ci->sensor)
        return sprintf(page, "%d\n", ci->sensor->offset);
    return sprintf(page, "%d\n", ANEMOMETER_DEFAULT_OFFSET);
}

static ssize_t anemometer_config_offset_store(struct config_item *item,
                                               const char *page, size_t count)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    s32 val;
    
    if (kstrtos32(page, 10, &val))
        return -EINVAL;
    
    if (ci->sensor) {
        mutex_lock(&ci->sensor->lock);
        ci->sensor->offset = val;
        mutex_unlock(&ci->sensor->lock);
    }
    return count;
}

CONFIGFS_ATTR(anemometer_config_, offset);

static ssize_t anemometer_config_window_size_show(struct config_item *item, char *page)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    if (ci->sensor)
        return sprintf(page, "%u\n", ci->sensor->window_size);
    return sprintf(page, "%u\n", ANEMOMETER_DEFAULT_WINDOW_SIZE);
}

static ssize_t anemometer_config_window_size_store(struct config_item *item,
                                                    const char *page, size_t count)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    u32 val;
    
    if (kstrtou32(page, 10, &val))
        return -EINVAL;
    
    if (val < ANEMOMETER_MIN_WINDOW_SIZE || val > ANEMOMETER_MAX_WINDOW_SIZE)
        return -EINVAL;
    
    if (ci->sensor) {
        /* Reuse sysfs store function */
        struct device dev;
        dev_set_drvdata(&dev, ci->sensor);
        return window_size_store(&dev, NULL, page, count);
    }
    return count;
}

CONFIGFS_ATTR(anemometer_config_, window_size);

static ssize_t anemometer_config_enabled_show(struct config_item *item, char *page)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    return sprintf(page, "%d\n", ci->enabled);
}

static ssize_t anemometer_config_enabled_store(struct config_item *item,
                                                const char *page, size_t count)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    int enable;
    int ret;
    
    if (kstrtoint(page, 10, &enable))
        return -EINVAL;
    
    if (enable && !ci->enabled) {
        /* Enable sensor */
        ci->sensor = anemometer_sensor_create(config_item_name(item));
        if (IS_ERR(ci->sensor))
            return PTR_ERR(ci->sensor);
        
        ret = anemometer_sensor_setup_gpio(ci->sensor, ci->gpio_num);
        if (ret) {
            anemometer_sensor_destroy(ci->sensor);
            ci->sensor = NULL;
            return ret;
        }
        
        ret = anemometer_sensor_start(ci->sensor);
        if (ret) {
            anemometer_sensor_destroy(ci->sensor);
            ci->sensor = NULL;
            return ret;
        }
        
        ci->enabled = true;
    } else if (!enable && ci->enabled) {
        /* Disable sensor */
        if (ci->sensor) {
            anemometer_sensor_destroy(ci->sensor);
            ci->sensor = NULL;
        }
        ci->enabled = false;
    }
    
    return count;
}

CONFIGFS_ATTR(anemometer_config_, enabled);

static struct configfs_attribute *anemometer_config_attrs[] = {
    &anemometer_config_attr_gpio,
    &anemometer_config_attr_slope_num,
    &anemometer_config_attr_slope_den,
    &anemometer_config_attr_offset,
    &anemometer_config_attr_window_size,
    &anemometer_config_attr_enabled,
    NULL,
};

static void anemometer_config_item_release(struct config_item *item)
{
    struct anemometer_config_item *ci = to_anemometer_item(item);
    
    if (ci->enabled && ci->sensor)
        anemometer_sensor_destroy(ci->sensor);
    
    kfree(ci);
}

static struct configfs_item_operations anemometer_config_item_ops = {
    .release = anemometer_config_item_release,
};

static struct config_item_type anemometer_config_type = {
    .ct_item_ops = &anemometer_config_item_ops,
    .ct_attrs = anemometer_config_attrs,
    .ct_owner = THIS_MODULE,
};

static struct config_item *anemometer_config_make_item(struct config_group *group,
                                                        const char *name)
{
    struct anemometer_config_item *ci;
    
    ci = kzalloc(sizeof(*ci), GFP_KERNEL);
    if (!ci)
        return ERR_PTR(-ENOMEM);
    
    config_item_init_type_name(&ci->item, name, &anemometer_config_type);
    ci->gpio_num = 0;
    ci->enabled = false;
    ci->sensor = NULL;
    
    return &ci->item;
}

static struct configfs_group_operations anemometer_config_group_ops = {
    .make_item = anemometer_config_make_item,
};

static struct config_item_type anemometer_subsys_type = {
    .ct_group_ops = &anemometer_config_group_ops,
    .ct_owner = THIS_MODULE,
};

static struct configfs_subsystem anemometer_configfs_subsys = {
    .su_group = {
        .cg_item = {
            .ci_namebuf = "anemometer",
            .ci_type = &anemometer_subsys_type,
        },
    },
};

int anemometer_configfs_init(void)
{
    int ret;
    
    config_group_init(&anemometer_configfs_subsys.su_group);
    mutex_init(&anemometer_configfs_subsys.su_mutex);
    
    ret = configfs_register_subsystem(&anemometer_configfs_subsys);
    if (ret) {
        pr_err("anemometer: failed to register configfs subsystem: %d\n", ret);
        return ret;
    }
    
    pr_info("anemometer: configfs interface registered at /sys/kernel/config/anemometer/\n");
    return 0;
}

void anemometer_configfs_exit(void)
{
    configfs_unregister_subsystem(&anemometer_configfs_subsys);
}

#else /* !CONFIG_CONFIGFS_FS */

int anemometer_configfs_init(void)
{
    pr_info("anemometer: ConfigFS support not compiled in\n");
    return 0;
}

void anemometer_configfs_exit(void)
{
}

#endif /* CONFIG_CONFIGFS_FS */
