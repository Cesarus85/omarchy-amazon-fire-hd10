/* Trona STK8BA53: Linux 6.18 STK8BA50 IIO driver adapted to the
 * 0x87 chip ID and 12-bit register format verified in Amazon GPL driver. */
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sensortek STK8BA53 3-Axis Accelerometer
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * STK8BA53 7-bit I2C address: 0x18.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/types.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define STK8BA53_REG_XOUT			0x02
#define STK8BA53_REG_YOUT			0x04
#define STK8BA53_REG_ZOUT			0x06
#define STK8BA53_REG_RANGE			0x0F
#define STK8BA53_REG_BWSEL			0x10
#define STK8BA53_REG_POWMODE			0x11
#define STK8BA53_REG_SWRST			0x14
#define STK8BA53_REG_INTEN2			0x17
#define STK8BA53_REG_INTMAP2			0x1A

#define STK8BA53_MODE_NORMAL			0
#define STK8BA53_MODE_SUSPEND			1
#define STK8BA53_MODE_POWERBIT			BIT(7)
#define STK8BA53_DATA_SHIFT			4
#define STK8BA53_RESET_CMD			0xB6
#define STK8BA53_SR_1792HZ_IDX			7
#define STK8BA53_DREADY_INT_MASK		0x10
#define STK8BA53_DREADY_INT_MAP			0x81
#define STK8BA53_ALL_CHANNEL_MASK		7
#define STK8BA53_ALL_CHANNEL_SIZE		6

#define STK8BA53_DRIVER_NAME			"stk8ba53"

#define STK8BA53_SCALE_AVAIL			"0.009577 0.019154 0.038307 0.076614"

/*
 * The accelerometer has four measurement ranges:
 * +/-2g; +/-4g; +/-8g; +/-16g
 *
 * Acceleration values are 12-bit, 2's complement.
 * Scales are calculated as following:
 *
 * scale1 = (2 + 2) * 9.81 / (2^12 - 1)   = 0.0384
 * scale2 = (4 + 4) * 9.81 / (2^12 - 1)   = 0.0767
 * etc.
 *
 * Scales are stored in this format:
 * { <register value>, <scale value> }
 *
 * Locally, the range is stored as a table index.
 */
static const struct {
	u8 reg_val;
	u32 scale_val;
} stk8ba53_scale_table[] = {
	{3, 9577}, {5, 19154}, {8, 38307}, {12, 76614}
};

/* Sample rates are stored as { <register value>, <Hz value> } */
static const struct {
	u8 reg_val;
	u16 samp_freq;
} stk8ba53_samp_freq_table[] = {
	{0x08, 14},  {0x09, 25},  {0x0A, 56},  {0x0B, 112},
	{0x0C, 224}, {0x0D, 448}, {0x0E, 896}, {0x0F, 1792}
};

/* Used to map scan mask bits to their corresponding channel register. */
static const int stk8ba53_channel_table[] = {
	STK8BA53_REG_XOUT,
	STK8BA53_REG_YOUT,
	STK8BA53_REG_ZOUT
};

struct stk8ba53_data {
	struct iio_mount_matrix orientation;
	struct i2c_client *client;
	struct mutex lock;
	int range;
	u8 sample_rate_idx;
	struct iio_trigger *dready_trig;
	bool dready_trigger_on;
	/* Ensure timestamp is naturally aligned */
	struct {
		s16 chans[3];
		aligned_s64 timetamp;
	} scan;
};


static const struct iio_mount_matrix *stk8ba53_mount(const struct iio_dev *dev, const struct iio_chan_spec *chan)
{
 struct stk8ba53_data *data = iio_priv(dev);
 return &data->orientation;
}
static const struct iio_chan_spec_ext_info stk8ba53_ext_info[] = {
 IIO_MOUNT_MATRIX(IIO_SHARED_BY_DIR, stk8ba53_mount), {}
};

#define STK8BA53_ACCEL_CHANNEL(index, reg, axis) {			\
	.type = IIO_ACCEL,						\
	.ext_info = stk8ba53_ext_info,\
	.address = reg,							\
	.modified = 1,							\
	.channel2 = IIO_MOD_##axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),		\
				    BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = index,						\
	.scan_type = {							\
		.sign = 's',						\
		.realbits = 12,						\
		.storagebits = 16,					\
		.shift = STK8BA53_DATA_SHIFT,				\
		.endianness = IIO_CPU,					\
	},								\
}

static const struct iio_chan_spec stk8ba53_channels[] = {
	STK8BA53_ACCEL_CHANNEL(0, STK8BA53_REG_XOUT, X),
	STK8BA53_ACCEL_CHANNEL(1, STK8BA53_REG_YOUT, Y),
	STK8BA53_ACCEL_CHANNEL(2, STK8BA53_REG_ZOUT, Z),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static IIO_CONST_ATTR(in_accel_scale_available, STK8BA53_SCALE_AVAIL);

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL("14 25 56 112 224 448 896 1792");

static struct attribute *stk8ba53_attributes[] = {
	&iio_const_attr_in_accel_scale_available.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group stk8ba53_attribute_group = {
	.attrs = stk8ba53_attributes
};

static int stk8ba53_read_accel(struct stk8ba53_data *data, u8 reg)
{
	int ret;
	struct i2c_client *client = data->client;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "register read failed\n");
		return ret;
	}

	return ret;
}

static int stk8ba53_data_rdy_trigger_set_state(struct iio_trigger *trig,
					       bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct stk8ba53_data *data = iio_priv(indio_dev);
	int ret;

	if (state)
		ret = i2c_smbus_write_byte_data(data->client,
			STK8BA53_REG_INTEN2, STK8BA53_DREADY_INT_MASK);
	else
		ret = i2c_smbus_write_byte_data(data->client,
			STK8BA53_REG_INTEN2, 0x00);

	if (ret < 0)
		dev_err(&data->client->dev, "failed to set trigger state\n");
	else
		data->dready_trigger_on = state;

	return ret;
}

static const struct iio_trigger_ops stk8ba53_trigger_ops = {
	.set_trigger_state = stk8ba53_data_rdy_trigger_set_state,
};

static int stk8ba53_set_power(struct stk8ba53_data *data, bool mode)
{
	int ret;
	u8 masked_reg;
	struct i2c_client *client = data->client;

	ret = i2c_smbus_read_byte_data(client, STK8BA53_REG_POWMODE);
	if (ret < 0)
		goto exit_err;

	if (mode)
		masked_reg = ret | STK8BA53_MODE_POWERBIT;
	else
		masked_reg = ret & (~STK8BA53_MODE_POWERBIT);

	ret = i2c_smbus_write_byte_data(client, STK8BA53_REG_POWMODE,
					masked_reg);
	if (ret < 0)
		goto exit_err;

	return ret;

exit_err:
	dev_err(&client->dev, "failed to change sensor mode\n");
	return ret;
}

static int stk8ba53_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int *val, int *val2, long mask)
{
	struct stk8ba53_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (iio_buffer_enabled(indio_dev))
			return -EBUSY;
		mutex_lock(&data->lock);
		ret = stk8ba53_set_power(data, STK8BA53_MODE_NORMAL);
		if (ret < 0) {
			mutex_unlock(&data->lock);
			return -EINVAL;
		}
		/* Allow fresh conversions after leaving suspend (1792 Hz). */
		usleep_range(2000, 3000);
		ret = stk8ba53_read_accel(data, chan->address);
		if (ret < 0) {
			stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
			mutex_unlock(&data->lock);
			return -EINVAL;
		}
		*val = sign_extend32(ret >> chan->scan_type.shift,
				     chan->scan_type.realbits - 1);
		stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
		mutex_unlock(&data->lock);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = stk8ba53_scale_table[data->range].scale_val;
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = stk8ba53_samp_freq_table
				[data->sample_rate_idx].samp_freq;
		*val2 = 0;
		return IIO_VAL_INT;
	}

	return -EINVAL;
}

static int stk8ba53_write_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int val, int val2, long mask)
{
	int ret;
	int i;
	int index = -1;
	struct stk8ba53_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		if (val != 0)
			return -EINVAL;

		for (i = 0; i < ARRAY_SIZE(stk8ba53_scale_table); i++)
			if (val2 == stk8ba53_scale_table[i].scale_val) {
				index = i;
				break;
			}
		if (index < 0)
			return -EINVAL;

		ret = i2c_smbus_write_byte_data(data->client,
				STK8BA53_REG_RANGE,
				stk8ba53_scale_table[index].reg_val);
		if (ret < 0)
			dev_err(&data->client->dev,
					"failed to set measurement range\n");
		else
			data->range = index;

		return ret;
	case IIO_CHAN_INFO_SAMP_FREQ:
		for (i = 0; i < ARRAY_SIZE(stk8ba53_samp_freq_table); i++)
			if (val == stk8ba53_samp_freq_table[i].samp_freq) {
				index = i;
				break;
			}
		if (index < 0)
			return -EINVAL;

		ret = i2c_smbus_write_byte_data(data->client,
				STK8BA53_REG_BWSEL,
				stk8ba53_samp_freq_table[index].reg_val);
		if (ret < 0)
			dev_err(&data->client->dev,
					"failed to set sampling rate\n");
		else
			data->sample_rate_idx = index;

		return ret;
	}

	return -EINVAL;
}

static const struct iio_info stk8ba53_info = {
	.read_raw		= stk8ba53_read_raw,
	.write_raw		= stk8ba53_write_raw,
	.attrs			= &stk8ba53_attribute_group,
};

static irqreturn_t stk8ba53_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct stk8ba53_data *data = iio_priv(indio_dev);
	int bit, ret, i = 0;

	mutex_lock(&data->lock);
	/*
	 * Do a bulk read if all channels are requested,
	 * from 0x02 (XOUT1) to 0x07 (ZOUT2)
	 */
	if (*(indio_dev->active_scan_mask) == STK8BA53_ALL_CHANNEL_MASK) {
		ret = i2c_smbus_read_i2c_block_data(data->client,
						    STK8BA53_REG_XOUT,
						    STK8BA53_ALL_CHANNEL_SIZE,
						    (u8 *)data->scan.chans);
		if (ret < STK8BA53_ALL_CHANNEL_SIZE) {
			dev_err(&data->client->dev, "register read failed\n");
			goto err;
		}
	} else {
		iio_for_each_active_channel(indio_dev, bit) {
			ret = stk8ba53_read_accel(data,
						  stk8ba53_channel_table[bit]);
			if (ret < 0)
				goto err;

			data->scan.chans[i++] = ret;
		}
	}
	iio_push_to_buffers_with_ts(indio_dev, &data->scan, sizeof(data->scan),
				    pf->timestamp);
err:
	mutex_unlock(&data->lock);
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static irqreturn_t stk8ba53_data_rdy_trig_poll(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct stk8ba53_data *data = iio_priv(indio_dev);

	if (data->dready_trigger_on)
		iio_trigger_poll(data->dready_trig);

	return IRQ_HANDLED;
}

static int stk8ba53_buffer_preenable(struct iio_dev *indio_dev)
{
	struct stk8ba53_data *data = iio_priv(indio_dev);

	return stk8ba53_set_power(data, STK8BA53_MODE_NORMAL);
}

static int stk8ba53_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct stk8ba53_data *data = iio_priv(indio_dev);

	return stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
}

static const struct iio_buffer_setup_ops stk8ba53_buffer_setup_ops = {
	.preenable   = stk8ba53_buffer_preenable,
	.postdisable = stk8ba53_buffer_postdisable,
};

static int stk8ba53_probe(struct i2c_client *client)
{
	int ret;
	struct iio_dev *indio_dev;
	struct stk8ba53_data *data;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	ret = iio_read_mount_matrix(&client->dev, &data->orientation);
	if (ret)
		return ret;
	i2c_set_clientdata(client, indio_dev);
	mutex_init(&data->lock);

	indio_dev->info = &stk8ba53_info;
	indio_dev->name = STK8BA53_DRIVER_NAME;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = stk8ba53_channels;
	indio_dev->num_channels = ARRAY_SIZE(stk8ba53_channels);

	ret = i2c_smbus_read_byte_data(client, 0x00);
	if (ret < 0)
		return ret;
	if (ret != 0x87)
		return -ENODEV;

	/* Reset all registers on startup */
	ret = i2c_smbus_write_byte_data(client,
			STK8BA53_REG_SWRST, STK8BA53_RESET_CMD);
	if (ret < 0) {
		dev_err(&client->dev, "failed to reset sensor\n");
		goto err_power_off;
	}

	usleep_range(10000, 12000);
	ret = i2c_smbus_write_byte_data(client, STK8BA53_REG_RANGE, 3);
	if (ret < 0)
		goto err_power_off;
	ret = i2c_smbus_write_byte_data(client, STK8BA53_REG_BWSEL, 0x0f);
	if (ret < 0)
		goto err_power_off;

	/* The default range is +/-2g */
	data->range = 0;

	/* The default sampling rate is 1792 Hz (maximum) */
	data->sample_rate_idx = STK8BA53_SR_1792HZ_IDX;

	/* Set up interrupts */
	ret = i2c_smbus_write_byte_data(client,
			STK8BA53_REG_INTEN2, client->irq > 0 ? STK8BA53_DREADY_INT_MASK : 0);
	if (ret < 0) {
		dev_err(&client->dev, "failed to set up interrupts\n");
		goto err_power_off;
	}
	ret = i2c_smbus_write_byte_data(client,
			STK8BA53_REG_INTMAP2, STK8BA53_DREADY_INT_MAP);
	if (ret < 0) {
		dev_err(&client->dev, "failed to set up interrupts\n");
		goto err_power_off;
	}

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						stk8ba53_data_rdy_trig_poll,
						NULL,
						IRQF_TRIGGER_RISING |
						IRQF_ONESHOT,
						"stk8ba53_event",
						indio_dev);
		if (ret < 0) {
			dev_err(&client->dev, "request irq %d failed\n",
				client->irq);
			goto err_power_off;
		}

		data->dready_trig = devm_iio_trigger_alloc(&client->dev,
							   "%s-dev%d",
							   indio_dev->name,
							   iio_device_id(indio_dev));
		if (!data->dready_trig) {
			ret = -ENOMEM;
			goto err_power_off;
		}

		data->dready_trig->ops = &stk8ba53_trigger_ops;
		iio_trigger_set_drvdata(data->dready_trig, indio_dev);
		ret = iio_trigger_register(data->dready_trig);
		if (ret) {
			dev_err(&client->dev, "iio trigger register failed\n");
			goto err_power_off;
		}
	}

	ret = iio_triggered_buffer_setup(indio_dev,
					 iio_pollfunc_store_time,
					 stk8ba53_trigger_handler,
					 &stk8ba53_buffer_setup_ops);
	if (ret < 0) {
		dev_err(&client->dev, "iio triggered buffer setup failed\n");
		goto err_trigger_unregister;
	}

	ret = iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&client->dev, "device_register failed\n");
		goto err_buffer_cleanup;
	}

	stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
	return ret;

err_buffer_cleanup:
	iio_triggered_buffer_cleanup(indio_dev);
err_trigger_unregister:
	if (data->dready_trig)
		iio_trigger_unregister(data->dready_trig);
err_power_off:
	stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
	return ret;
}

static void stk8ba53_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct stk8ba53_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);

	if (data->dready_trig)
		iio_trigger_unregister(data->dready_trig);

	stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
}

static int stk8ba53_suspend(struct device *dev)
{
	struct stk8ba53_data *data;

	data = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	return stk8ba53_set_power(data, STK8BA53_MODE_SUSPEND);
}

static int stk8ba53_resume(struct device *dev)
{
	struct stk8ba53_data *data;

	data = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	return stk8ba53_set_power(data, iio_buffer_enabled(i2c_get_clientdata(to_i2c_client(dev))) ?
		STK8BA53_MODE_NORMAL : STK8BA53_MODE_SUSPEND);
}

static DEFINE_SIMPLE_DEV_PM_OPS(stk8ba53_pm_ops, stk8ba53_suspend,
				stk8ba53_resume);

static const struct i2c_device_id stk8ba53_i2c_id[] = {
	{ "stk8ba53" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, stk8ba53_i2c_id);

static const struct acpi_device_id stk8ba53_acpi_id[] = {
	{"STK8BA53", 0},
	{ }
};

MODULE_DEVICE_TABLE(acpi, stk8ba53_acpi_id);

static const struct of_device_id stk8ba53_of_match[] = {
 { .compatible = "sensortek,stk8ba53" }, {}
};
MODULE_DEVICE_TABLE(of, stk8ba53_of_match);

static struct i2c_driver stk8ba53_driver = {
	.driver = {
		.name = "stk8ba53",
		.of_match_table = stk8ba53_of_match,
		.pm = pm_sleep_ptr(&stk8ba53_pm_ops),
		.acpi_match_table = stk8ba53_acpi_id,
	},
	.probe =        stk8ba53_probe,
	.remove =           stk8ba53_remove,
	.id_table =         stk8ba53_i2c_id,
};

module_i2c_driver(stk8ba53_driver);

MODULE_AUTHOR("Tiberiu Breana <tiberiu.a.breana@intel.com>");
MODULE_DESCRIPTION("STK8BA53 3-Axis Accelerometer driver");
MODULE_LICENSE("GPL v2");
