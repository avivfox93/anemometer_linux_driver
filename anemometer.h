#ifndef _ANEMOMETER_H
#define _ANEMOMETER_H

#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#define ANEMOMETER_NAME "anemometer"
#define ANEMOMETER_CLASS_NAME "anemometer"

/* Default configuration values */
#define ANEMOMETER_DEFAULT_WINDOW_SIZE      5
#define ANEMOMETER_DEFAULT_UPDATE_INTERVAL  1000  /* ms */
#define ANEMOMETER_DEFAULT_SLOPE_NUM        100   /* 0.1 * 1000 */
#define ANEMOMETER_DEFAULT_SLOPE_DEN        1000
#define ANEMOMETER_DEFAULT_OFFSET           0
#define ANEMOMETER_MIN_WINDOW_SIZE          1
#define ANEMOMETER_MAX_WINDOW_SIZE          60
#define ANEMOMETER_MIN_UPDATE_INTERVAL      100   /* ms */
#define ANEMOMETER_MAX_UPDATE_INTERVAL      10000 /* ms */
#define ANEMOMETER_MAX_FREQ_HZ              200
#define ANEMOMETER_STALE_TIMEOUT_SEC        120  /* 2 * max window */

struct anemometer_sensor {
    char name[32];
    struct gpio_desc *gpio;
    int irq;
    
    /* Configuration */
    u32 window_size;
    u32 update_interval_ms;
    s32 slope_num;
    u32 slope_den;
    s32 offset;
    
    /* Runtime data */
    atomic_t pulse_count;
    u32 *pulse_buffer;
    u32 buffer_head;
    u32 buffer_count;
    u64 last_pulse_time;
    
    /* Calculated values */
    u32 frequency_millihz;
    s32 wind_speed_um_s;
    u64 total_pulses;
    bool stale;
    
    /* Threading */
    struct task_struct *worker;
    bool running;
    struct mutex lock;
    
    /* sysfs */
    struct device *dev;
    struct list_head list;
};

/* Global driver state */
struct anemometer_drv {
    struct class *class;
    struct list_head sensors;
    struct mutex sensors_lock;
    int sensor_count;
    
#if IS_ENABLED(CONFIG_CONFIGFS_FS)
    struct configfs_subsystem configfs_subsys;
#endif
    
    struct cdev cdev;
    dev_t devt;
};

extern struct anemometer_drv anemometer_drv;

/* Function prototypes */
extern struct anemometer_sensor *anemometer_sensor_create(const char *name);
extern int anemometer_sensor_setup_gpio(struct anemometer_sensor *sensor, u32 gpio_num);
extern int anemometer_sensor_start(struct anemometer_sensor *sensor);
extern int anemometer_sensor_create_dt(struct platform_device *pdev);
extern int anemometer_sensor_create_configfs(const char *name);
extern int anemometer_sensor_create_chrdev(const char *name, u32 gpio);
extern void anemometer_sensor_destroy(struct anemometer_sensor *sensor);
extern struct anemometer_sensor *anemometer_sensor_find(const char *name);

int anemometer_sysfs_register(struct anemometer_sensor *sensor);
void anemometer_sysfs_unregister(struct anemometer_sensor *sensor);

int anemometer_configfs_init(void);
void anemometer_configfs_exit(void);

int anemometer_chrdev_init(void);
void anemometer_chrdev_exit(void);

#endif /* _ANEMOMETER_H */
