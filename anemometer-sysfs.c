#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "anemometer.h"

/* Show raw pulses */
static ssize_t raw_pulses_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u32 count;
    
    if (!sensor)
        return -ENODEV;
    
    count = atomic_read(&sensor->pulse_count);
    return sprintf(buf, "%u\n", count);
}

/* Show frequency in millihertz */
static ssize_t frequency_hz_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u32 freq;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    freq = sensor->frequency_millihz;
    mutex_unlock(&sensor->lock);
    
    /* Print with 3 decimal places */
    return sprintf(buf, "%u.%03u\n", freq / 1000, freq % 1000);
}

/* Show wind speed in m/s */
static ssize_t wind_speed_ms_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    s32 speed;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    speed = sensor->wind_speed_um_s;
    mutex_unlock(&sensor->lock);
    
    /* Print with 3 decimal places */
    return sprintf(buf, "%d.%03d\n", speed / 1000000, abs(speed / 1000) % 1000);
}

/* Show wind speed in km/h */
static ssize_t wind_speed_kmh_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    s32 speed;
    s64 kmh;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    speed = sensor->wind_speed_um_s;
    mutex_unlock(&sensor->lock);
    
    /* Convert um/s to km/h: * 3600 / 1,000,000,000 */
    kmh = div_s64((s64)speed * 36, 10000);

    return sprintf(buf, "%lld.%03lld\n", kmh / 1000, abs64(kmh) % 1000);
}

/* Show total pulses */
static ssize_t pulse_count_total_show(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u64 total;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    total = sensor->total_pulses;
    mutex_unlock(&sensor->lock);
    
    return sprintf(buf, "%llu\n", total);
}

/* Show stale flag */
static ssize_t stale_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    bool stale;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    stale = sensor->stale;
    mutex_unlock(&sensor->lock);
    
    return sprintf(buf, "%d\n", stale);
}

/* Show/set slope */
static ssize_t slope_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    sprintf(buf, "%d %u\n", sensor->slope_num, sensor->slope_den);
    mutex_unlock(&sensor->lock);
    
    return strlen(buf);
}

static ssize_t slope_store(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    s32 num;
    u32 den;
    
    if (!sensor)
        return -ENODEV;
    
    if (sscanf(buf, "%d %u", &num, &den) != 2)
        return -EINVAL;
    
    if (den == 0)
        return -EINVAL;
    
    mutex_lock(&sensor->lock);
    sensor->slope_num = num;
    sensor->slope_den = den;
    mutex_unlock(&sensor->lock);
    
    return count;
}

/* Show/set offset */
static ssize_t offset_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    s32 offset;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    offset = sensor->offset;
    mutex_unlock(&sensor->lock);
    
    return sprintf(buf, "%d\n", offset);
}

static ssize_t offset_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    s32 offset;
    
    if (!sensor)
        return -ENODEV;
    
    if (kstrtos32(buf, 10, &offset))
        return -EINVAL;
    
    mutex_lock(&sensor->lock);
    sensor->offset = offset;
    mutex_unlock(&sensor->lock);
    
    return count;
}

/* Show/set window size */
static ssize_t window_size_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u32 size;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    size = sensor->window_size;
    mutex_unlock(&sensor->lock);
    
    return sprintf(buf, "%u\n", size);
}

static ssize_t window_size_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u32 *new_buffer;
    u32 size;

    if (!sensor)
        return -ENODEV;

    if (kstrtou32(buf, 10, &size))
        return -EINVAL;

    if (size < ANEMOMETER_MIN_WINDOW_SIZE || size > ANEMOMETER_MAX_WINDOW_SIZE)
        return -EINVAL;

    mutex_lock(&sensor->lock);

    /*
     * Note: If worker is currently reading pulse_buffer, it may
     * see inconsistent data briefly. Consider stopping worker
     * before resize for critical applications.
     */

    new_buffer = kcalloc(size, sizeof(u32), GFP_KERNEL);
    if (!new_buffer) {
        mutex_unlock(&sensor->lock);
        return -ENOMEM;
    }

    kfree(sensor->pulse_buffer);
    sensor->pulse_buffer = new_buffer;
    sensor->window_size = size;
    sensor->buffer_head = 0;
    sensor->buffer_count = 0;
    mutex_unlock(&sensor->lock);

    return count;
}

/* Show/set update interval */
static ssize_t update_interval_ms_show(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u32 interval;
    
    if (!sensor)
        return -ENODEV;
    
    mutex_lock(&sensor->lock);
    interval = sensor->update_interval_ms;
    mutex_unlock(&sensor->lock);
    
    return sprintf(buf, "%u\n", interval);
}

static ssize_t update_interval_ms_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
    struct anemometer_sensor *sensor = dev_get_drvdata(dev);
    u32 interval;
    
    if (!sensor)
        return -ENODEV;
    
    if (kstrtou32(buf, 10, &interval))
        return -EINVAL;
    
    if (interval < ANEMOMETER_MIN_UPDATE_INTERVAL || 
        interval > ANEMOMETER_MAX_UPDATE_INTERVAL)
        return -EINVAL;
    
    mutex_lock(&sensor->lock);
    sensor->update_interval_ms = interval;
    mutex_unlock(&sensor->lock);
    
    return count;
}

/* Define attributes */
static DEVICE_ATTR_RO(raw_pulses);
static DEVICE_ATTR_RO(frequency_hz);
static DEVICE_ATTR_RO(wind_speed_ms);
static DEVICE_ATTR_RO(wind_speed_kmh);
static DEVICE_ATTR_RO(pulse_count_total);
static DEVICE_ATTR_RO(stale);
static DEVICE_ATTR_RW(slope);
static DEVICE_ATTR_RW(offset);
static DEVICE_ATTR_RW(window_size);
static DEVICE_ATTR_RW(update_interval_ms);

static struct attribute *anemometer_attrs[] = {
    &dev_attr_raw_pulses.attr,
    &dev_attr_frequency_hz.attr,
    &dev_attr_wind_speed_ms.attr,
    &dev_attr_wind_speed_kmh.attr,
    &dev_attr_pulse_count_total.attr,
    &dev_attr_stale.attr,
    &dev_attr_slope.attr,
    &dev_attr_offset.attr,
    &dev_attr_window_size.attr,
    &dev_attr_update_interval_ms.attr,
    NULL,
};

static struct attribute_group anemometer_attr_group = {
    .attrs = anemometer_attrs,
};

int anemometer_sysfs_register(struct anemometer_sensor *sensor)
{
    int ret;
    
    sensor->dev = device_create(anemometer_drv.class, NULL, 0, sensor,
                                "%s", sensor->name);
    if (IS_ERR(sensor->dev)) {
        ret = PTR_ERR(sensor->dev);
        pr_err("anemometer: failed to create device '%s': %d\n", 
               sensor->name, ret);
        return ret;
    }
    
    ret = sysfs_create_group(&sensor->dev->kobj, &anemometer_attr_group);
    if (ret) {
        pr_err("anemometer: failed to create sysfs group for '%s': %d\n",
               sensor->name, ret);
        device_destroy(anemometer_drv.class, sensor->dev->devt);
        return ret;
    }
    
    return 0;
}

void anemometer_sysfs_unregister(struct anemometer_sensor *sensor)
{
    if (sensor->dev) {
        sysfs_remove_group(&sensor->dev->kobj, &anemometer_attr_group);
        device_destroy(anemometer_drv.class, sensor->dev->devt);
        sensor->dev = NULL;
    }
}
