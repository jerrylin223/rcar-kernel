/*
 * The camera is connected to a Maxim MAX9295A GMSL2 serializer.
 */
#include <linux/delay.h>
#include <linux/fwnode.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "max9295a.h"

#define CAMERA_WIDTH		1920
#define CAMERA_HEIGHT		1020
#define CAMERA_FORMAT		MEDIA_BUS_FMT_Y10_1X10
#define MAX9295A_NUM_PADS   1

struct max9295a_priv {
	struct i2c_client        *client;
	struct v4l2_subdev	      sd;
	struct media_pad	      pads;
	struct v4l2_ctrl_handler  ctrls;
	struct fwnode_handle     *fwnode;
};

static inline struct max9295a_priv *sd_to_max9295a(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9295a_priv, sd);
}


static int __max9295a_write(struct max9295a_priv *priv, u16 reg, u8 val)
{
	u8 buf[3] = { reg >> 8, reg & 0xff, val };
	int ret;

	ret = i2c_master_send(priv->client, buf, 3);
	return ret < 0 ? ret : 0;
}

static int max9295a_set_regs(struct max9295a_priv *priv,
			                 const struct max9295a_reg *regs,
			                 unsigned int nr_regs)
{
	unsigned int i;
	int ret;

	for (i = 0; i < nr_regs; i++) {
		ret = __max9295a_write(priv, regs[i].reg, regs[i].val);
		msleep(5);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int max9295a_sensor_set_regs(struct max9295a_priv *priv)
{
	int ret;

	__max9295a_write(priv, 0x0010, 0x21);	/* SW reset */
	msleep(200);

	/* Program the camera sensor initial configuration. */
	ret = max9295a_set_regs(priv, configuretable_ar0231,
				            ARRAY_SIZE(configuretable_ar0231));
	msleep(200);
	
	return ret;
}

static int max9295a_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9295a_priv *priv = sd_to_max9295a(sd);
	struct device *dev = &priv->client->dev;
	int ret;
	
	ret = max9295a_sensor_set_regs(priv);
	if (ret) {
		dev_err(dev, "Failed to max9295a set register\n");
		return -EINVAL;
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdev ops
 */
static int max9295a_enum_mbus_code(struct v4l2_subdev *sd,
				                   struct v4l2_subdev_pad_config *cfg,
				                   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index > 0) {
		return -EINVAL;
	}

	code->code = CAMERA_FORMAT;

	return 0;
}

static int max9295a_get_fmt(struct v4l2_subdev *sd,
			                struct v4l2_subdev_pad_config *cfg,
			                struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;

	if (format->pad) {
		return -EINVAL;
	}

	fmt->width		  = CAMERA_WIDTH;
	fmt->height		  = CAMERA_HEIGHT;
	fmt->code		  = CAMERA_FORMAT;
	fmt->colorspace	  = V4L2_COLORSPACE_RAW;
	fmt->field		  = V4L2_FIELD_NONE;
	fmt->ycbcr_enc	  = V4L2_YCBCR_ENC_601;
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func	  = V4L2_XFER_FUNC_NONE;

	return 0;
}

static struct v4l2_subdev_video_ops max9295a_video_ops = {
	.s_stream	= max9295a_s_stream,
};

static const struct v4l2_subdev_pad_ops max9295a_subdev_pad_ops = {
	.enum_mbus_code = max9295a_enum_mbus_code,
	.get_fmt    = max9295a_get_fmt,
	.set_fmt    = max9295a_get_fmt,
};

static struct v4l2_subdev_ops max9295a_subdev_ops = {
	.video      = &max9295a_video_ops,
	.pad        = &max9295a_subdev_pad_ops,
};

static int max9295a_initialize(struct max9295a_priv *priv)
{
	/* Implement as needed */
	return 0;
}

static const struct of_device_id max9295a_dt_ids[] = {
	{ .compatible = "maxim,max9295a" },
	{},
};
MODULE_DEVICE_TABLE(of, max9295a_dt_ids);

static int max9295a_v4l2_subdev_init(struct max9295a_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct i2c_client *client = priv->client;
	struct device_node *np = dev->of_node;
	struct fwnode_handle *ep;
	unsigned int i, mbps;
	int ret, bpp = 10;

	/* Skip non-max9295a devices. */
	if (!np || !of_match_node(max9295a_dt_ids, np)) {
		return 0;
	}

	client = to_i2c_client(dev);
	priv = i2c_get_clientdata(client);

	v4l2_i2c_subdev_init(&priv->sd, client, &max9295a_subdev_ops);
	priv->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/* Optional Ctrl Handler */
	v4l2_ctrl_handler_init(&priv->ctrls, 0);
	priv->sd.ctrl_handler = &priv->ctrls;

	/* 1920x1020@30Hz, RAW10 bpp = 10 */
	mbps = 74230000 * bpp;	
	v4l2_ctrl_new_std(&priv->ctrls, NULL, V4L2_CID_PIXEL_RATE,
			          1, INT_MAX, 1, mbps);
	priv->sd.ctrl_handler = &priv->ctrls;

	/* Pads (one source pad) */
	priv->pads.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&priv->sd.entity, MAX9295A_NUM_PADS, &priv->pads);

	if (ret) {
		return ret;
	}

	/* Find endpoint (fwnode) */
	priv->fwnode = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!priv->fwnode) {
		dev_warn(dev, "No endpoint found.\n");
	}
	
	priv->sd.fwnode = priv->fwnode;

	/* Register as a V4L2 subdevice for async notifier */
	ret = v4l2_async_register_subdev(&priv->sd);
	if (ret) {
		dev_err(dev, "Failed to register subdev\n");
		goto error_put_node;
	}

	return 0;

error_put_node:
	fwnode_handle_put(ep);
	return ret;
}

/* -----------------------------------------------------------------------------
 * Probe/Remove
 */
static int max9295a_probe(struct i2c_client *client)
{
	struct max9295a_priv *priv;
	struct device_node *np = client->dev.of_node;
	int ret;
	int addrs[1];

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		return -ENOMEM;
	}

	priv->client = client;

	of_property_read_u32_array(np, "reg", addrs, ARRAY_SIZE(addrs));

	i2c_set_clientdata(client, priv);

	ret = max9295a_v4l2_subdev_init(priv);
	if (ret < 0) {
		goto error_free;
	}

	max9295a_initialize(priv);
	return 0;

error_free:
	return ret;
}

static int max9295a_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct max9295a_priv *priv = sd_to_max9295a(sd);

	media_entity_cleanup(&priv->sd.entity);
	fwnode_handle_put(priv->sd.fwnode);
	v4l2_async_unregister_subdev(&priv->sd);
	return 0;
}

static const struct of_device_id max9295a_ids[] = {
	{ .compatible = "maxim,max9295a", },
	{ }
};
MODULE_DEVICE_TABLE(of, max9295a_ids);

static struct i2c_driver max9295a_i2c_driver = {
	.driver	= {
		.name	= "max9295a",
		.of_match_table = max9295a_ids,
	},
	.probe_new	= max9295a_probe,
	.remove		= max9295a_remove,
};
module_i2c_driver(max9295a_i2c_driver);

MODULE_ALIAS("MAX9295A");
MODULE_DESCRIPTION("GMSL2 MAX9295A driver");
MODULE_LICENSE("GPL");
