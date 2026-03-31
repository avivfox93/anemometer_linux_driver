#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "anemometer.h"

static int __init anemometer_init(void);
static void __exit anemometer_exit(void);

struct anemometer_drv anemometer_drv;

irqreturn_t anemometer_irq_handler(int irq, void *dev_id)
{
    struct anemometer_sensor *sensor = dev_id;
    atomic_inc(&sensor->pulse_count);
    sensor->last_pulse_time = ktime_get_seconds();
    return IRQ_HANDLED;
}

static int anemometer_worker(void *data)
{
    struct anemometer_sensor *sensor = data;
    u32 pulses, sum, freq;
    s64 speed;
    int i;
    
    while (!kthread_should_stop() && sensor->running) {
        msleep(sensor->update_interval_ms);
        
        mutex_lock(&sensor->lock);
        
        /* Get and reset pulse count */
        pulses = atomic_xchg(&sensor->pulse_count, 0);
        sensor->total_pulses += pulses;
        
        /* Add to circular buffer */
        sensor->pulse_buffer[sensor->buffer_head] = pulses;
        sensor->buffer_head = (sensor->buffer_head + 1) % sensor->window_size;
        if (sensor->buffer_count < sensor->window_size)
            sensor->buffer_count++;
        
        /* Calculate frequency */
        sum = 0;
        for (i = 0; i < sensor->buffer_count; i++)
            sum += sensor->pulse_buffer[i];
        
        if (sensor->buffer_count > 0)
            freq = (sum * 1000) / (sensor->buffer_count * sensor->update_interval_ms);
        else
            freq = 0;
        
        /* Check for impossible frequency */
        if (freq > ANEMOMETER_MAX_FREQ_HZ * 1000) {
            pr_warn("anemometer %s: frequency %u mHz exceeds max\n", 
                    sensor->name, freq);
            freq = 0;
        }
        
        /* Check for stale data */
        if (ktime_get_seconds() - sensor->last_pulse_time > ANEMOMETER_STALE_TIMEOUT_SEC) {
            sensor->stale = true;
        } else {
            sensor->stale = false;
        }
        
        sensor->frequency_millihz = freq;
        
        /* Apply calibration: speed = freq * slope + offset */
        speed = (s64)freq * sensor->slope_num / sensor->slope_den;
        speed += sensor->offset;  /* offset already in um/s */
        sensor->wind_speed_um_s = (s32)speed;
        
        mutex_unlock(&sensor->lock);
    }
    
    return 0;
}

struct anemometer_sensor *anemometer_sensor_create(const char *name)
{
    struct anemometer_sensor *sensor;
    
    sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
    if (!sensor)
        return ERR_PTR(-ENOMEM);
    
    strscpy(sensor->name, name, sizeof(sensor->name));
    sensor->window_size = ANEMOMETER_DEFAULT_WINDOW_SIZE;
    sensor->update_interval_ms = ANEMOMETER_DEFAULT_UPDATE_INTERVAL;
    sensor->slope_num = ANEMOMETER_DEFAULT_SLOPE_NUM;
    sensor->slope_den = ANEMOMETER_DEFAULT_SLOPE_DEN;
    sensor->offset = ANEMOMETER_DEFAULT_OFFSET;
    atomic_set(&sensor->pulse_count, 0);
    mutex_init(&sensor->lock);
    INIT_LIST_HEAD(&sensor->list);
    sensor->last_pulse_time = ktime_get_seconds();
    
    sensor->pulse_buffer = kcalloc(sensor->window_size, sizeof(u32), GFP_KERNEL);
    if (!sensor->pulse_buffer) {
        kfree(sensor);
        return ERR_PTR(-ENOMEM);
    }
    
    mutex_lock(&anemometer_drv.sensors_lock);
    list_add(&sensor->list, &anemometer_drv.sensors);
    anemometer_drv.sensor_count++;
    mutex_unlock(&anemometer_drv.sensors_lock);
    
    return sensor;
}

void anemometer_sensor_destroy(struct anemometer_sensor *sensor)
{
    if (!sensor)
        return;
    
    if (sensor->running) {
        sensor->running = false;
        if (sensor->worker) {
            kthread_stop(sensor->worker);
            sensor->worker = NULL;
        }
    }
    
    if (sensor->irq > 0)
        free_irq(sensor->irq, sensor);
    
    if (sensor->gpio)
        gpiod_put(sensor->gpio);
    
    anemometer_sysfs_unregister(sensor);
    
    mutex_lock(&anemometer_drv.sensors_lock);
    list_del(&sensor->list);
    anemometer_drv.sensor_count--;
    mutex_unlock(&anemometer_drv.sensors_lock);
    
    kfree(sensor->pulse_buffer);
    kfree(sensor);
}

struct anemometer_sensor *anemometer_sensor_find(const char *name)
{
    struct anemometer_sensor *sensor;
    
    mutex_lock(&anemometer_drv.sensors_lock);
    list_for_each_entry(sensor, &anemometer_drv.sensors, list) {
        if (!strcmp(sensor->name, name)) {
            mutex_unlock(&anemometer_drv.sensors_lock);
            return sensor;
        }
    }
    mutex_unlock(&anemometer_drv.sensors_lock);
    
    return NULL;
}

int anemometer_sensor_setup_gpio(struct anemometer_sensor *sensor, u32 gpio_num)
{
    int ret;
    
    sensor->gpio = gpio_to_desc(gpio_num);
    if (!sensor->gpio) {
        pr_err("anemometer: invalid GPIO %u\n", gpio_num);
        return -EINVAL;
    }
    
    ret = gpiod_direction_input(sensor->gpio);
    if (ret) {
        pr_err("anemometer: failed to set GPIO %u as input: %d\n", gpio_num, ret);
        return ret;
    }
    
    sensor->irq = gpiod_to_irq(sensor->gpio);
    if (sensor->irq < 0) {
        pr_err("anemometer: failed to get IRQ for GPIO %u: %d\n", gpio_num, sensor->irq);
        return sensor->irq;
    }
    
    ret = request_irq(sensor->irq, anemometer_irq_handler, 
                      IRQF_TRIGGER_RISING, "anemometer", sensor);
    if (ret) {
        pr_err("anemometer: failed to request IRQ %d: %d\n", sensor->irq, ret);
        return ret;
    }
    
    return 0;
}

int anemometer_sensor_start(struct anemometer_sensor *sensor)
{
    int ret;
    
    ret = anemometer_sysfs_register(sensor);
    if (ret)
        return ret;
    
    sensor->running = true;
    sensor->worker = kthread_run(anemometer_worker, sensor, "anemometer-%s", sensor->name);
    if (IS_ERR(sensor->worker)) {
        ret = PTR_ERR(sensor->worker);
        sensor->worker = NULL;
        sensor->running = false;
        anemometer_sysfs_unregister(sensor);
        return ret;
    }
    
    return 0;
}

static int __init anemometer_init(void)
{
    int ret;
    
    pr_info("anemometer: initializing driver\n");
    
    mutex_init(&anemometer_drv.sensors_lock);
    INIT_LIST_HEAD(&anemometer_drv.sensors);
    anemometer_drv.sensor_count = 0;
    
    anemometer_drv.class = class_create(THIS_MODULE, ANEMOMETER_CLASS_NAME);
    if (IS_ERR(anemometer_drv.class)) {
        ret = PTR_ERR(anemometer_drv.class);
        pr_err("anemometer: failed to create class: %d\n", ret);
        return ret;
    }
    
    ret = anemometer_chrdev_init();
    if (ret) {
        pr_err("anemometer: failed to init char device: %d\n", ret);
        goto err_class;
    }
    
    ret = anemometer_configfs_init();
    if (ret) {
        pr_warn("anemometer: ConfigFS not available\n");
    }

    ret = anemometer_dt_init();
    if (ret) {
        pr_warn("anemometer: device tree support not available\n");
    }

    pr_info("anemometer: driver loaded\n");
    return 0;
    
err_class:
    class_destroy(anemometer_drv.class);
    return ret;
}

static void __exit anemometer_exit(void)
{
    struct anemometer_sensor *sensor, *tmp;
    
    pr_info("anemometer: unloading driver\n");
    
    mutex_lock(&anemometer_drv.sensors_lock);
    list_for_each_entry_safe(sensor, tmp, &anemometer_drv.sensors, list) {
        anemometer_sensor_destroy(sensor);
    }
    mutex_unlock(&anemometer_drv.sensors_lock);

    anemometer_dt_exit();
    anemometer_configfs_exit();
    anemometer_chrdev_exit();
    class_destroy(anemometer_drv.class);
    
    pr_info("anemometer: driver unloaded\n");
}

module_init(anemometer_init);
module_exit(anemometer_exit);

MODULE_AUTHOR("Anemometer Driver");
MODULE_DESCRIPTION("Generic anemometer wind sensor driver");
MODULE_LICENSE("GPL");
