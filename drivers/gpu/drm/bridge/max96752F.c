// SPDX-License-Identifier: GPL-2.0
/*
 * Core driver for Analog Devices MAX96752F deserializer
 *  
 * Supported:
 * 	- LVDS format adjust
 *	- RGB format adjust
 *
 * Copyright (C) Retronix Tech Inc.
 *
 * Contact:
 *	Felix Hsu <felixhsu@retronix.com.tw>
 */

#include <linux/bits.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

/* Device relative register */
#define REG_REG2		0x02
#define REG_REG2_LOCK_CFG	BIT(7)
#define REG_REG2_VID_EN		BIT(6)
/* OLDI register */
#define REG_OLDI1		0x1CE
#define REG_OLDI1_FMT		BIT(6)
#define REG_OLDI1_LANE		BIT(5)
#define REG_OLDI1_SPL_EN	BIT(3)
/* GPIO register */
#define REG_GPIO4_A		0x20C

struct max96752F {
	struct drm_bridge	bridge;
	struct device		*dev;
	struct regmap		*regmap;
	struct drm_bridge	*next_bridge;
	struct gpio_desc	*pwdn;

	bool			panel_switch;
};

static const struct regmap_config max96752F_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x1F00,
};

static struct max96752F *bridge_to_max96752F(struct drm_bridge *bridge)
{
	return container_of(bridge, struct max96752F, bridge);
}

static int max96752F_bridge_attach(struct drm_bridge *bridge,
			    enum drm_bridge_attach_flags flags)
{
	struct max96752F *priv = bridge_to_max96752F(bridge);

	if(priv->next_bridge)
		dev_dbg(priv->dev, "%s: max96752F find panel bridge\n", __func__);

	return drm_bridge_attach(bridge->encoder, priv->next_bridge,
				 &priv->bridge, flags);
}

static void max96752F_atomic_bridge_enable(struct drm_bridge *bridge,
				           struct drm_bridge_state *old_bridge_state)
{
	struct max96752F *priv = bridge_to_max96752F(bridge);
	struct drm_atomic_state *state = old_bridge_state->base.state;
	const struct drm_bridge_state *bridge_state;
	const struct drm_crtc_state *crtc_state;
	const struct drm_display_mode *mode;
	struct drm_connector *connector;
	struct drm_crtc *crtc;
	bool lvds_format_24bpp;
	bool lvds_format_jeida;
	int ret;

	dev_info(priv->dev, "%s: entered drm bridge enable\n", __func__);

	gpiod_set_value_cansleep(priv->pwdn, 1);
	msleep(45);

	bridge_state = drm_atomic_get_new_bridge_state(state, bridge);

	switch (bridge_state->output_bus_cfg.format) {
	case MEDIA_BUS_FMT_RGB666_1X7X3_SPWG:
		lvds_format_24bpp = false;
		lvds_format_jeida = true;
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA:
		lvds_format_24bpp = true;
		lvds_format_jeida = true;
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_SPWG:
		lvds_format_24bpp = true;
		lvds_format_jeida = false;
		break;
	default:
		lvds_format_24bpp = true;
		lvds_format_jeida = false;
		dev_warn(priv->dev,
			 "Unsupported LVDS bus format 0x%04x, please check output bridge driver. Falling back to SPWG24.\n",
			 bridge_state->output_bus_cfg.format);
		break;
	}
	
	connector = drm_atomic_get_new_connector_for_encoder(state,
							     bridge->encoder);
	crtc = drm_atomic_get_new_connector_state(state, connector)->crtc;
	crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
	mode = &crtc_state->adjusted_mode;

	/* turn off LVDS output before configuration */
	ret = regmap_update_bits(priv->regmap, REG_REG2, REG_REG2_VID_EN, 0x00);

	if(ret)
		dev_dbg(priv->dev, "%s: REG2_VID_EN write 0 failed\n", __func__);

	/* write GPIO4 if needed */
	if(priv->panel_switch)
		ret = regmap_write(priv->regmap, REG_GPIO4_A, 0x92);

	if(ret)
		dev_dbg(priv->dev, "%s: GPIO4 write 0x92 failed\n", __func__);

	/* RGB666 use 3 lane */
	if(!lvds_format_24bpp)
		ret = regmap_update_bits(priv->regmap, REG_OLDI1, REG_OLDI1_LANE, 0x20);

	if(ret)
		dev_dbg(priv->dev, "%s: OLDI1_LANE write 1 failed\n", __func__);

	/* choose format vesa/jeida */
	if(!lvds_format_jeida)
		ret = regmap_update_bits(priv->regmap, REG_OLDI1, REG_OLDI1_FMT, 0x40);

	if(ret)
		dev_dbg(priv->dev, "%s: OLDI_FMT write 1 failed\n", __func__);

	/* 
	 * 1920x1080@60 should use LVDS split mode
	 * Threshold isn't clearly pointed out, and should be updated if necessary.
	 */
	if(mode->hdisplay >= 1920 && mode->vdisplay >= 1080)
		ret = regmap_update_bits(priv->regmap, REG_OLDI1, REG_OLDI1_SPL_EN, 0x08);

	if(ret)
		dev_dbg(priv->dev, "%s: OLDI_SPL_EN write 1 failed\n", __func__);

	ret = regmap_update_bits(priv->regmap, REG_REG2, REG_REG2_VID_EN, 0x40);

	if(ret)
		dev_dbg(priv->dev, "%s: REG2_VID_EN write 1 failed\n", __func__);
}

static void max96752F_atomic_bridge_disable(struct drm_bridge *bridge,
				           struct drm_bridge_state *old_bridge_state)
{
	struct max96752F *priv = bridge_to_max96752F(bridge);

	dev_info(priv->dev, "%s: entered disable func\n", __func__);
	gpiod_set_value_cansleep(priv->pwdn, 0);
	usleep_range(1000,1100);
}

static enum drm_mode_status max96752F_bridge_mode_valid(struct drm_bridge *bridge,
		     					const struct drm_display_info *info,
     		     					const struct drm_display_mode *mode)
{
	/* clock range < dual LVDS 160 MHz */
	//if (mode->clock > 160000) {
	//	printk("%s: %d failed from max96752F drm mode valid\n", __func__, mode->clock);
	//	return MODE_CLOCK_HIGH;
	//}

	return MODE_OK;
}

static const struct drm_bridge_funcs max96752F_bridge_funcs = {
        .attach = max96752F_bridge_attach,
        .atomic_enable = max96752F_atomic_bridge_enable,
        .atomic_disable = max96752F_atomic_bridge_disable,
	.mode_valid = max96752F_bridge_mode_valid,

	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
};

static int max96752F_parse_dt(struct max96752F *priv)
{
	struct drm_bridge *bridge;
	struct device *dev = priv->dev;
	
	bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);
	if(IS_ERR(bridge))
		return dev_err_probe(dev, PTR_ERR(bridge), "failed to create panel bridge\n");

	priv->next_bridge = bridge;

	/*
	 * "panel_switch" refers to GPIO4 since in our application
	 * we use this pin to control which panel signal should go
	 *
	 * NOTE: creating limitation is a bad implementation in driver
	 * and shall be replaced in the future. 
	 */	
	if (of_find_property(dev->of_node, "panel_switch", NULL))
		priv->panel_switch = true;
	else
		priv->panel_switch = false;
	
	return 0;
}

static int max96752F_bridge_probe(struct i2c_client *client)
{
	struct max96752F *priv;
	struct device *dev = &client->dev;
	int ret;

	dev_info(dev, "%s: entered probe func\n", __func__);

	priv = devm_kzalloc(dev, sizeof(struct max96752F), GFP_KERNEL);
	if(!priv)
		return -ENOMEM;

	priv->dev = dev;

	priv->pwdn = devm_gpiod_get_optional(priv->dev, "enable",
					     GPIOD_OUT_HIGH);
	if(IS_ERR(priv->pwdn))
		return dev_err_probe(priv->dev, PTR_ERR(priv->pwdn), "failed to request gpio\n");

	ret = max96752F_parse_dt(priv);
	if(ret)
		return ret;

	priv->regmap = devm_regmap_init_i2c(client, &max96752F_regmap_config);
	if(IS_ERR(priv->regmap))
		return dev_err_probe(priv->dev, PTR_ERR(priv->regmap), "failed to init regmap\n");

	dev_set_drvdata(dev, priv);
	i2c_set_clientdata(client, priv);

	priv->bridge.funcs = &max96752F_bridge_funcs;
	priv->bridge.of_node = dev->of_node;

	drm_bridge_add(&priv->bridge);

        return 0;
}

static int max96752F_bridge_remove(struct i2c_client *client)
{
	struct max96752F *priv = i2c_get_clientdata(client);

	drm_bridge_remove(&priv->bridge);

	return 0;
}

static const struct of_device_id max96752F_bridge_match_table[] = { 
        {.compatible = "maxim,max96752F"},
        {},
};
MODULE_DEVICE_TABLE(of, max96752F_bridge_match_table);

static struct i2c_driver max96752F_driver = {
	.probe_new = max96752F_bridge_probe,
	.remove = max96752F_bridge_remove,
	.driver = {
		.name = "maxim-max96752F",
		.of_match_table = max96752F_bridge_match_table,
	},
};
module_i2c_driver(max96752F_driver);

MODULE_AUTHOR("Felix Hsu <felixhsu@retronix.com.tw>");
MODULE_DESCRIPTION("Max96752F GMSL2 to LVDS maxim serdes bridge driver");
MODULE_LICENSE("GPL v2");
