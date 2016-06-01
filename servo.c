#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pwm.h>

#define DRVNAME "SERVO"

#define ANGLE_PWM	4 /* P9_16 */
#define PWM_PERIOD	20000000
#define MAX_DUTY	2500000
#define MIN_DUTY	500000
/* it isn't joke
 * pwm = MIN_DUTY + (MAX_DUTY - MIN_DUTY) * (angle / 180)
 * pwm = MIN_DUTY + ((MAX_DUTY - MIN_DUTY) / 180) * angle
 * MAX_DUTY - MIN_DUTY) / 180- this is constant
 * (2500000 - 500000) / 180 = 2000000 / 180 = 11111,(1) what was rounded to 11111 */
#define ANGLE_CONST	11111

static unsigned int angle = 0;
static unsigned int angle_duty = MIN_DUTY;
static struct class *servo_class;
static struct pwm_device *angle_pwm;

/* show and store functions declarations */
static ssize_t angle_show(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t angle_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);

/* attributes */
static struct class_attribute angle_attr = __ATTR(angle, 0660, angle_show, angle_store);

static ssize_t angle_show(struct class *cls, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%u", angle);
}

static ssize_t angle_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int tmp;
	sscanf(buf, "%u", &tmp);
	if(tmp < 0 || tmp > 180)return -EINVAL;
	angle = tmp;
	angle_duty = MIN_DUTY + ANGLE_CONST * angle;
	pwm_config(angle_pwm, angle_duty, PWM_PERIOD);
	return count;
}

static int __init servo_init(void)
{
	/* create entried in sysfs */
	servo_class = class_create(THIS_MODULE, "servo");
	if(servo_class == NULL){
		printk(KERN_ERR "%s: Cannot create entry in sysfs", DRVNAME);
		return -1;
	}
	
	if(class_create_file(servo_class, &angle_attr) != 0){
		printk(KERN_ERR "%s: Cannot create sysfs attribute\n", DRVNAME);
		goto err1;
	}
	
	angle_pwm = pwm_request(ANGLE_PWM, "angle_pwm");
	if(angle_pwm == NULL)
	{
		printk(KERN_ERR "%s: Cannot use PWM output P8_13\n", DRVNAME);
		goto err2;
	}
	pwm_config(angle_pwm, angle_duty, PWM_PERIOD);
	pwm_set_polarity(angle_pwm, PWM_POLARITY_NORMAL);
	pwm_enable(angle_pwm);
	return 0;
	
	err2:
	class_remove_file(servo_class, &angle_attr);
	err1:
	class_destroy(servo_class);
	return -1;
}

static void __exit servo_exit(void)
{
	pwm_config(angle_pwm, 0, 0);
	pwm_disable(angle_pwm);
	pwm_free(angle_pwm);

	class_remove_file(servo_class, &angle_attr);
	class_destroy(servo_class);
}

module_init(servo_init);
module_exit(servo_exit);

MODULE_AUTHOR("Adam Olek");
MODULE_DESCRIPTION("Servo driver");
MODULE_LICENSE("GPL");
