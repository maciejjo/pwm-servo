#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/pinctrl/consumer.h>

#define SERVO_PWM_PERIOD  20000000
#define SERVO_MAX_DUTY    2500000
#define SERVO_MIN_DUTY    500000
#define SERVO_DEGREE ((SERVO_MAX_DUTY - SERVO_MIN_DUTY) / 180)

struct pwm_servo_data {
	struct pwm_device *pwm;
	unsigned int angle;
	unsigned int angle_duty;
	unsigned int standby;
};

static ssize_t pwm_servo_show_angle(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct pwm_servo_data *servo =
		platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%u\n", servo->angle);
}

static ssize_t pwm_servo_store_angle(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long tmp;
	int error;
	struct pwm_servo_data *servo =
		platform_get_drvdata(to_platform_device(dev));

	error = kstrtoul(buf, 10, &tmp);

	if (error < 0)
		return error;

	if (tmp < 0 || tmp > 180)
		return -EINVAL;

	error = pwm_config(servo->pwm, SERVO_MIN_DUTY +
			SERVO_DEGREE * tmp, SERVO_PWM_PERIOD);
	if (error < 0)
		return error;

	servo->angle = tmp;
	servo->angle_duty = SERVO_MIN_DUTY + SERVO_DEGREE * tmp;

	return count;
}

static ssize_t pwm_servo_show_standby(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct pwm_servo_data *servo =
		platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%u\n", servo->standby);
}

static ssize_t pwm_servo_store_standby(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long tmp;
	int error;
	struct pwm_servo_data *servo =
		platform_get_drvdata(to_platform_device(dev));

	error = kstrtoul(buf, 10, &tmp);

	if (error < 0)
		return error;

	if (tmp != 0 && tmp != 1)
		return -EINVAL;

	if (tmp == 1 && servo->standby != 1)
		pwm_disable(servo->pwm);
	else if (tmp == 0 && servo->standby != 0) {
		error = pwm_enable(servo->pwm);
		if (error)
			return error;
	}

	servo->standby = tmp;

	return count;
}

static DEVICE_ATTR(angle, S_IWUSR | S_IRUGO, pwm_servo_show_angle,
		pwm_servo_store_angle);
static DEVICE_ATTR(standby, S_IWUSR | S_IRUGO, pwm_servo_show_standby,
		pwm_servo_store_standby);

static struct attribute *pwm_servo_attributes[] = {
	&dev_attr_angle.attr,
	&dev_attr_standby.attr,
	NULL,
};

static const struct attribute_group pwm_servo_group = {
	.attrs = pwm_servo_attributes,
};

static int pwm_servo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pinctrl *pinctrl;
	struct device_node *node = dev->of_node;
	struct pwm_servo_data *servo;
	int error = 0;

	if (node == NULL) {
		dev_err(dev, "Non DT platforms not supported\n");
		return -EINVAL;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl))
		dev_warn(&pdev->dev, "Unable to select pin group\n");

	servo = devm_kzalloc(&pdev->dev, sizeof(*servo), GFP_KERNEL);
	if (!servo)
		return -ENOMEM;

	servo->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(servo->pwm)) {
		dev_err(&pdev->dev, "devm_pwm_get() failed\n");
		return error;
	}

	servo->angle = 0;
	servo->angle_duty = SERVO_MIN_DUTY;
	servo->standby = 1;

	error = pwm_config(servo->pwm, servo->angle_duty, SERVO_PWM_PERIOD);
	if (error) {
		dev_err(&pdev->dev, "pwm_config() failed\n");
		return error;
	}

	error = pwm_set_polarity(servo->pwm, PWM_POLARITY_NORMAL);
	if (error) {
		dev_err(&pdev->dev, "pwm_set_polarity() failed\n");
		return error;
	}

	platform_set_drvdata(pdev, servo);

	error = sysfs_create_group(&pdev->dev.kobj, &pwm_servo_group);
	if (error) {
		dev_err(&pdev->dev, "sysfs_create_group() failed (%d)\n",
				error);
		return error;
	}

	return 0;
}

static int pwm_servo_remove(struct platform_device *pdev)
{
	struct pwm_servo_data *servo = platform_get_drvdata(pdev);

	pwm_disable(servo->pwm);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pwm_servo_match[] = {
	{ .compatible = "pwm_servo", },
	{ },
};
MODULE_DEVICE_TABLE(of, pwm_servo_match);
#endif

static struct platform_driver pwm_servo_driver = {
	.probe	= pwm_servo_probe,
	.remove = pwm_servo_remove,
	.driver = {
		.name	= "pwm-servo",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(pwm_servo_match),
#endif
	},
};
module_platform_driver(pwm_servo_driver);

MODULE_AUTHOR("Adam Olek, Maciej Sobkowski <maciejjo@maciejjo.pl>");
MODULE_DESCRIPTION("PWM Servo driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-servo");
