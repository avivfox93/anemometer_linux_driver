#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include "anemometer.h"

static int anemometer_dt_parse_sensor(struct device_node *np)
{
    struct anemometer_sensor *sensor;
    const char *name;
    u32 gpio_prop;
    u32 val;
    int ret;
    struct gpio_desc *gpio;
    enum of_gpio_flags flags;
    
    /* Get label */
    ret = of_property_read_string(np, "label", &name);
    if (ret) {
        pr_err("anemometer: sensor missing label property\n");
        return ret;
    }
    
    /* Check if sensor already exists */
    if (anemometer_sensor_find(name)) {
        pr_err("anemometer: sensor '%s' already exists\n", name);
        return -EEXIST;
    }
    
    /* Create sensor */
    sensor = anemometer_sensor_create(name);
    if (IS_ERR(sensor))
        return PTR_ERR(sensor);
    
    /* Parse GPIO */
    gpio = gpiod_get_from_of_node(np, NULL, 0, GPIOD_IN, name);
    if (IS_ERR(gpio)) {
        pr_err("anemometer: failed to get GPIO for '%s': %ld\n", 
               name, PTR_ERR(gpio));
        ret = PTR_ERR(gpio);
        goto err_free;
    }
    sensor->gpio = gpio;
    
    /* Get IRQ */
    sensor->irq = gpiod_to_irq(sensor->gpio);
    if (sensor->irq < 0) {
        pr_err("anemometer: failed to get IRQ for '%s': %d\n", name, sensor->irq);
        ret = sensor->irq;
        goto err_gpio;
    }
    
    /* Parse optional properties */
    if (!of_property_read_u32(np, "window-size", &val)) {
        if (val >= ANEMOMETER_MIN_WINDOW_SIZE && val <= ANEMOMETER_MAX_WINDOW_SIZE)
            sensor->window_size = val;
        else
            pr_warn("anemometer: '%s' invalid window-size %u, using default\n", name, val);
    }
    
    if (!of_property_read_u32(np, "update-interval", &val)) {
        if (val >= ANEMOMETER_MIN_UPDATE_INTERVAL && val <= ANEMOMETER_MAX_UPDATE_INTERVAL)
            sensor->update_interval_ms = val;
        else
            pr_warn("anemometer: '%s' invalid update-interval %u, using default\n", name, val);
    }
    
    if (!of_property_read_u32(np, "slope", &val))
        sensor->slope_num = val;
    
    if (!of_property_read_u32(np, "slope-div", &val))
        sensor->slope_den = val;
    
    if (!of_property_read_u32(np, "offset", &val))
        sensor->offset = val;
    
    /* Request IRQ */
    ret = request_irq(sensor->irq, anemometer_irq_handler,
                       IRQF_TRIGGER_RISING, "anemometer", sensor);
    if (ret) {
        pr_err("anemometer: failed to request IRQ for '%s': %d\n", name, ret);
        goto err_gpio;
    }
    
    /* Start sensor */
    ret = anemometer_sensor_start(sensor);
    if (ret) {
        pr_err("anemometer: failed to start '%s': %d\n", name, ret);
        goto err_irq;
    }
    
    pr_info("anemometer: created sensor '%s' from device tree\n", name);
    return 0;
    
err_irq:
    free_irq(sensor->irq, sensor);
err_gpio:
    gpiod_put(sensor->gpio);
err_free:
    anemometer_sensor_destroy(sensor);
    return ret;
}

static int anemometer_dt_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    struct device_node *child;
    int ret;
    
    if (!np)
        return -ENODEV;
    
    for_each_child_of_node(np, child) {
        ret = anemometer_dt_parse_sensor(child);
        if (ret) {
            pr_err("anemometer: failed to parse sensor: %d\n", ret);
            continue;
        }
    }
    
    return 0;
}

static int anemometer_dt_remove(struct platform_device *pdev)
{
    /* Sensors are cleaned up in anemometer_exit */
    return 0;
}

static const struct of_device_id anemometer_dt_ids[] = {
    { .compatible = "generic,anemometer" },
    { }
};
MODULE_DEVICE_TABLE(of, anemometer_dt_ids);

static struct platform_driver anemometer_platform_driver = {
    .probe = anemometer_dt_probe,
    .remove = anemometer_dt_remove,
    .driver = {
        .name = ANEMOMETER_NAME,
        .of_match_table = anemometer_dt_ids,
    },
};

int anemometer_dt_init(void)
{
    return platform_driver_register(&anemometer_platform_driver);
}

void anemometer_dt_exit(void)
{
    platform_driver_unregister(&anemometer_platform_driver);
}
