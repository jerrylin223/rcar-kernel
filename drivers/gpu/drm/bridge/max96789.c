// SPDX-License-Identifier: GPL-2.0
/*
 * Core driver for Analog Devices MAX96789 serializer
 *
 * Support:
 * 	GMSL single-link mode is well supported currently
 * 	GMSL splitter mode has to be further validated,
 *	see max96789_bridge_attach() for more detailed
 *	GMSL dual-link mode hasn't been tested yet
 *
 * Copyright (C) 2022 Renesas Electronics Corporation
 * CopyRight (C) 2024 Retronix Tech Inc.
 *
 * Contact:
 * 	Felix Hsu <felixhsu@retronix.com.tw>
 *	KC yang <kcyang@retronix.com.tw>
 */

#include <linux/bits.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge_connector.h>
#include "max96789.h"

static unsigned int SER_PATTERN_SEL = 0x02;		/* 0x02: gradient, 0x01: chessboard, 0x00: OFF */
module_param_named(ser_patgen_select, SER_PATTERN_SEL, int, 0644);

#define bridge_to_max96789_priv(b) \
	container_of(b, struct max96789_priv, bridge)

#define connector_to_max96789_priv(c) \
	container_of(c, struct max96789_priv, connector);

/* -----------------------------------------------------------------------------
 * DEBUG
 */
 
static void DEBUG_INFO(struct max96789_priv *priv){
	unsigned int val;

	regmap_read(priv->regmap, MAX96789_PWR0, &val);
	dev_dbg(priv->dev, "[%s (%d)]: vdd info = %d\n",
		__func__, __LINE__, val);
	
	regmap_read(priv->regmap, MAX96789_REG15, &val);
	dev_dbg(priv->dev, "[%s (%d)]: link_status = %d\n",
		__func__, __LINE__, val);
	
	regmap_read(priv->regmap, MAX96789_VTX_X(1), &val);
	if (val & PCLKDET_VTX)
		dev_dbg(priv->dev, "[%s (%d)]: PCLK detected\n",
			 __func__, __LINE__);
	else
		dev_dbg(priv->dev, "[%s (%d)]: PCLK not detected, pclk_det = %d\n",
			 __func__, __LINE__, val);	

	regmap_read(priv->regmap, MAX96789_HS_VS_X, &val);
	if ((val & DE_DET_X) && (val & VS_DET_X) && (val & HS_DET_X))
		dev_dbg(priv->dev, "[%s (%d)]: HS VS DE detected\n",
			__func__, __LINE__);
	else
		dev_dbg(priv->dev, "[%s (%d)]: HS VS DE are not detected hs_vs_det = %d\n",
			 __func__, __LINE__, val);
	
	regmap_read(priv->regmap, MAX96789_MIPI_DSI32, &val);
	dev_dbg(priv->dev, "[%s (%d)]: dsi_contr_0_status = %d\n",
		__func__, __LINE__, val);
}
 
 
/* -----------------------------------------------------------------------------
 * test pattern
 */
static int max96789_patgen(struct max96789_priv *priv, int pat_flags)
{
	int ret = 0;
	u32 xres, yres, hfp, hbp, hsa, vfp, vbp, vsa;
	u32 vtotal, htotal, vs_high, vs_low, hs_high, hs_low, de_high, de_low, de_cnt, v2h, v2d;

	xres = 1280;
	yres = 768;
	hbp	 = 6;
	hfp	 = 200;
	hsa	 = 14;
	vbp	 = 20;
	vfp	 = 20;
	vsa	 = 16;

	vtotal 	= vfp + vsa + vbp + yres;
	htotal 	= xres + hfp + hbp + hsa;
	vs_high = vsa * htotal;
	vs_low 	= (vfp + yres + vbp) * htotal;
	hs_high = hsa;
	hs_low 	= xres + hfp + hbp;
	de_high = xres;
	de_low 	= hfp + hsa + hbp;
	de_cnt 	= yres;
	v2h 	= (vsa + vbp) * htotal + hfp;		/* Set HS Delay */
	v2d 	= v2h + hsa + hbp;					/* Set DE Delay */
	
	max96789_write_n(priv, MAX96789_VTX_X(2) , 3, 0);			/* VS delay */
	max96789_write_n(priv, MAX96789_VTX_X(11), 3, v2h);			/* HS delay */
	max96789_write_n(priv, MAX96789_VTX_X(20), 3, v2d);			/* DE delay */
	max96789_write_n(priv, MAX96789_VTX_X(5) , 3, vs_high); 
	max96789_write_n(priv, MAX96789_VTX_X(8) , 3, vs_low);
	max96789_write_n(priv, MAX96789_VTX_X(14), 2, hs_high);
	max96789_write_n(priv, MAX96789_VTX_X(16), 2, hs_low);
	max96789_write_n(priv, MAX96789_VTX_X(18), 2, vtotal);	
	max96789_write_n(priv, MAX96789_VTX_X(23), 2, de_high);		
	max96789_write_n(priv, MAX96789_VTX_X(25), 2, de_low);		
	max96789_write_n(priv, MAX96789_VTX_X(27), 2, de_cnt);		
	
	/* Generate VS, HS and DE in free-running mode. */
	regmap_write(priv->regmap, MAX96789_VTX_X(0), 0xFB);
	
	/* pclk detect, select VS trigger edge */
	regmap_write(priv->regmap, MAX96789_VTX_X(1), 0x01);

	/* Set gradient increment for gradient pattern. */
	regmap_write(priv->regmap, MAX96789_VTX_X(30), 0x03);
	
	/* Set color for chessboard pattern. */
	max96789_write_n(priv, MAX96789_VTX_X(31), 3, 0xFF0000);
	max96789_write_n(priv, MAX96789_VTX_X(34), 3, 0x0000FF);
	max96789_write_n(priv, MAX96789_VTX_X(37), 3, 0x505050);
	
	/* Select pattern : 0x02 is gradient, 0x01 is chessboard, 
	 * 0x00 is pattern generator disabled - use video from the serializer input */
	regmap_write(priv->regmap, MAX96789_VTX_X(29), pat_flags == 1 ? 0x02 : 0x01);

	return ret;
}

/*
 * sysfs
 */
static int dual_set = 0;

static ssize_t max96789_dual_view_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", dual_set);
}

static ssize_t max96789_dual_view_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 0, &dual_set);
	if (ret)
		return ret;

	if (dual_set > 1 || dual_set < 0)
		return -EINVAL;

	return count;
}

static DEVICE_ATTR(dual_view, 0644, max96789_dual_view_show, max96789_dual_view_store);

/* see MST_BT */
static const u32 max96789_i2c_clk_lut[] = {
	9920,
	33200,
	99200,
	123000,
	203000,
	397000,
	625000,
	980000,
};

static void max96789_set_link_i2c(struct max96789_priv *priv)
{
	struct i2c_adapter *adap = priv->client->adapter;
	struct i2c_timings t;
	const u32 *clk_lut;
	size_t clk_lut_size;
	unsigned int mst_bt, slv_sh;
	int i;

	i2c_parse_fw_timings(&adap->dev, &t, false);
	
	/* 
	 * 	i|MST_BT	bit_rate	SLV_SH
	 * 	======================================== Fast-plus 1Mhz
	 * 	7|111		980000		00
	 *	6|110		625000		00
	 *	======================================== Fast 400KHz
	 *	5|101		397000		01
	 *	4|100		203000		01
	 *	3|011		123000		01
	 *	======================================== Standard 100KHz
	 *	2|010		99200		10
	 *	1|001		33200		10
	 *	0|000		9920		10
	 *					2-(i/3)
	 */

	clk_lut = max96789_i2c_clk_lut;
	clk_lut_size = ARRAY_SIZE(max96789_i2c_clk_lut);

	for(i = 0; i < clk_lut_size; i++)
		if(clk_lut[i] > t.bus_freq_hz)
			break;

	if(i == 0) {
		dev_warn(priv->dev, "i2c-bus clk-freq too low, use 9.92Kbps as bit-rate\n");
	} else
		i--;

	mst_bt = i << 4;
	slv_sh = 2 - (i / 3);

	regmap_update_bits(priv->regmap, MAX96789_I2C_0,
		       	   I2C_1_MST_BT, mst_bt);
	regmap_update_bits(priv->regmap, MAX96789_I2C_1,
			   I2C_0_SLV_SH, slv_sh);
}

static int max96789_parse_gmsl_link_cfg(struct max96789_priv *priv)
{
	struct device *dev = priv->dev;
	unsigned int link_type[GMSL_MAX_LINKS], cab_sel[GMSL_MAX_LINKS];
	unsigned int link_rate;
	struct device_node *node;
	int ret, port, val;

	/* decide gmsl_link_type */
	ret = of_property_read_u32_array(dev->of_node, "maxim,gmsl-link-type", 
					 link_type, GMSL_MAX_LINKS);	

	if(ret) {
		dev_dbg(dev, "gmsl-link-type are not defined or invalid, use cfg pin default\n");
		regmap_read(priv->regmap, MAX96789_REG4, &val);
		priv->gmsl_link_types[GMSL_LINK_A] = !!(val & GMSL2_A);
		priv->gmsl_link_types[GMSL_LINK_B] = !!(val & GMSL2_B);
	}else {
		priv->gmsl_link_types[GMSL_LINK_A] = !!(link_type[GMSL_LINK_A] & 0x3);
		priv->gmsl_link_types[GMSL_LINK_B] = !!(link_type[GMSL_LINK_B] & 0x3);	
	}

	/* decide gmsl cable_sel */
	ret = of_property_read_u32_array(dev->of_node, "maxim,gmsl-cable-sel",
					 cab_sel, GMSL_MAX_LINKS);
	if(ret) {
		dev_dbg(dev, "gmsl-cable-sel not defined or invalid, use cfg pin default\n");
		regmap_read(priv->regmap, MAX96789_CTRL1, &val);
		priv->gmsl_cab_sel[GMSL_LINK_A] = !!(val & CXTP_A);
		priv->gmsl_cab_sel[GMSL_LINK_B] = !!(val & CXTP_B);
	}else {
		priv->gmsl_cab_sel[GMSL_LINK_A] = cab_sel[GMSL_LINK_A] & 0x1;
		priv->gmsl_cab_sel[GMSL_LINK_B] = cab_sel[GMSL_LINK_B] & 0x1;
	}

	/* decide gmsl_link_rate */
	ret = of_property_read_u32(dev->of_node, "maxim,gmsl-link-rate",
				   &link_rate);
	if(ret || link_rate != 3 || link_rate != 6) {
		dev_dbg(dev, "gmsl_link_rate not defined or invalid, use cfg pin default\n");
		regmap_read(priv->regmap, MAX96789_REG1, &val);
		priv->gmsl_link_rate = (val & TX_RATE_MASK) >> TX_RATE_SHIFT;
	}else
		priv->gmsl_link_rate = (link_rate & 0x7) / 3;
	

	/* decide link cfg */
	if(of_get_property(dev->of_node, "maxim,gmsl2-dual-link", NULL))
		priv->gmsl2_dual_link = true;
	else
		priv->gmsl2_dual_link = false;

	/* 
	 * decide how many gmsl endpoint dev linked
	 * port 0 is for dsi, while 1/2 link to GMSL 
	 */
	for(port = 1; port < GMSL_MAX_LINKS + 1; port++) {
		node = of_graph_get_remote_node(dev->of_node, port, 0);
		if(!of_device_is_available(node)) {
			dev_dbg(dev, "skipping disabled port\n");
			continue;
		}

		priv->gmsl_link_mask[port - 1] = true;
		priv->gmsl_links_used++;
	}
	
	if(!priv->gmsl_links_used) {
		dev_err(priv->dev, "%s: no gmsl linked to this device\n", __func__);
		return -ENODEV;
	}
	
	return 0;
}

static void max96789_set_gmsl_link_cfg(struct max96789_priv *priv)
{
	int ret;
	unsigned int mask, val;	
	
	/* Link type, Link number */
	mask = GMSL2_A | GMSL2_B | LINK_EN_A | LINK_EN_B;
	if(priv->gmsl_links_used == 2 || priv->gmsl2_dual_link)
		regmap_update_bits(priv->regmap, MAX96789_REG4, mask, mask);
	else if(priv->gmsl_link_mask[GMSL_LINK_A])
		regmap_update_bits(priv->regmap, MAX96789_REG4, mask, GMSL2_A | LINK_EN_A);
	else if(priv->gmsl_link_mask[GMSL_LINK_B])
		regmap_update_bits(priv->regmap, MAX96789_REG4, mask, GMSL2_B | LINK_EN_B);

	/* Cable type */
	mask = CXTP_A | CXTP_B;
	val  = priv->gmsl_cab_sel[GMSL_LINK_A] ? CXTP_A : 0;
	val |= priv->gmsl_cab_sel[GMSL_LINK_B] ? CXTP_B : 0;
	regmap_update_bits(priv->regmap, MAX96789_CTRL1, mask, val);

	/* Link speed */
	val = priv->gmsl_link_rate << TX_RATE_SHIFT;
	regmap_update_bits(priv->regmap, MAX96789_REG1, TX_RATE_MASK, val);

	/* Link cfg + Reset oneshot */
	mask = LINK_CFG_MASK | AUTO_LINK | RESET_ONESHOT;
	if(priv->gmsl_links_used == 2)
		regmap_update_bits(priv->regmap,
				   MAX96789_CTRL0, mask,
				   LINK_CFG_SPLITTER | RESET_ONESHOT);
	else if(priv->gmsl2_dual_link)
		regmap_update_bits(priv->regmap,
				   MAX96789_CTRL0, mask,
				   LINK_CFG_DUAL | RESET_ONESHOT);
	else if(priv->gmsl_link_mask[GMSL_LINK_A])
		regmap_update_bits(priv->regmap,
				   MAX96789_CTRL0, mask,
				   LINK_CFG_SINGLE_A | RESET_ONESHOT);
	else if(priv->gmsl_link_mask[GMSL_LINK_B])
		regmap_update_bits(priv->regmap,
				   MAX96789_CTRL0, mask,
				   LINK_CFG_SINGLE_B | RESET_ONESHOT);

	ret = regmap_read_poll_timeout(priv->regmap, MAX96789_INTR7, val,
				       (val & LOCK_A) && (val & LOCK_B), 500, 300000);
	if(ret)
		dev_err(priv->dev, "%s: GMSL links not locked 0x1F=%d\n", __func__, val);
}

static int max96789_dev_init(struct max96789_priv *priv)
{
	int err;
	unsigned int val;

	/*
	 * According to datasheet:
	 * 	The following register writes must be made to ensure
	 * 	proper serializer operation. Without these writes,
	 * 	the operation of the device as specified in the 
	 * 	datasheet cannot be guaranteed.
	 *
	 * Purpose: Increase regulator voltage to the clock
	 * 	    system to ensure robust operation
	 */
	err = regmap_update_bits(priv->regmap, 0x302, 0x07, 0x10);
	if(err)
		return dev_err_probe(priv->dev, err,
				     "%s: Cannot increase voltage to clock system\n",
				      __func__);

	/* ensure gmsl LOCKED asserted before setting cfg */
	err = regmap_read_poll_timeout(priv->regmap, MAX96789_CTRL3, val,
				       val & LOCKED, 500, 300000);
	if(err)
		return dev_err_probe(priv->dev, err,
				     "%s: no GMSL link locked, CTRL3 = 0x%02x\n",
				      __func__, val);

	max96789_set_gmsl_link_cfg(priv);

	max96789_set_link_i2c(priv);

	return 0;
}

static void max96789_video_timing(struct max96789_priv *priv)
{
	struct drm_display_mode *mode = 
		&priv->bridge.encoder->crtc->state->adjusted_mode;
	
	/* HSYNC_LB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI5,
		     (mode->hsync_end - mode->hsync_start) & 0xFF);
	/* VSYNC_LB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI6,
		     (mode->vsync_end - mode->vsync_start) & 0xFF);
	/* VSYNC_HN / HSYNC_HN */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI7,
		     (((mode->vsync_end - mode->vsync_start) & 0xF00) >> 4) |
		     ((mode->hsync_end - mode->hsync_start) >> 8));

	/* VFP_LB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI37,
		     (mode->vsync_start - mode->vdisplay) & 0xFF);
	/* VBP_HB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI39,
		     ((mode->vtotal - mode->vsync_end) & 0xFF0) >> 4);
	/* VBP_LN / VFP_HN */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI38,
		     ((mode->vtotal - mode->vsync_end) & 0x00F) |
		     (((mode->vsync_start - mode->vdisplay) & 0xF00) >> 4));

	/* VRES_LB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI40,
		     mode->vdisplay & 0xFF);
	/* VRES_HN */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI41,
		     (mode->vdisplay & 0xF00) >> 8);

	/* HFP_LB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI42,
		     (mode->hsync_start - mode->hdisplay) & 0xFF);
	/* HBP_LN / HFP_HN */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI43,
		     ((mode->htotal - mode->hsync_end) & 0x00F) |
		     ((mode->hsync_start - mode->hdisplay) & 0xF00) >> 4);
	/* HBP_HB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI44,
		     ((mode->htotal - mode->hsync_end) & 0xFF0) >> 4);

	/* HRES_LB */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI45,
		     mode->hdisplay & 0xFF);
	/* HRES_H5b */
	regmap_write(priv->regmap, MAX96789_MIPI_DSI46,
		     (mode->hdisplay & 0x1F00) >> 8);
}

static void max96789_dsi_cfg_for_split(struct max96789_priv *priv)
{
	int mask, val;

	/* 0x0308: FRONTTOP 0 */
	mask = (CLK_SELX | CLK_SELY | CLK_SELZ | CLK_SELU |
		START_PORTA | START_PORTB | ENABLE_LINE_INFO);

	val  = priv->dsi_id == DSI_PORT_A ? (CLK_SELZ | CLK_SELU)
					  : (CLK_SELX | CLK_SELY);
	val |= priv->dsi_id == DSI_PORT_A ? START_PORTA : START_PORTB;
	val |= ENABLE_LINE_INFO;

	regmap_update_bits(priv->regmap, MAX96789_FRONTTOP_0, mask, val);

	/* 0x0311: FRONTTOP 9 */
	val = priv->dsi_id == DSI_PORT_A ? (START_PORTAX | START_PORTAY) 
					 : (START_PORTBX | START_PORTBY);
	regmap_update_bits(priv->regmap, MAX96789_FRONTTOP_9, val, val);

	/* 0x031C/0x031D/0x031E/0x031F: FRONTTOP_20/21/22/23 soft_dt en */
	regmap_write(priv->regmap, MAX96789_FRONTTOP_20, 0x98);
	regmap_write(priv->regmap, MAX96789_FRONTTOP_21, 0x98);

	/* 0x0321/0x0322/0x0323/0x0324: FRONTTOP_25/26/27/28 soft_dt*/
	regmap_write(priv->regmap, MAX96789_FRONTTOP_25, 0x24);
	regmap_write(priv->regmap, MAX96789_FRONTTOP_26, 0x24);

	/* 0x0053/0x0057/0x005B/0x005F: TX3 CFGV VIDEO_X/Y/Z/U */
	regmap_write(priv->regmap, MAX96789_TX3(0), 0x10);
	regmap_write(priv->regmap, MAX96789_TX3(1), 0x20);

	max96789_video_timing(priv);

	/* 0x0002: REG2 */
	mask = VID_TX_EN_X | VID_TX_EN_Y | 0x3;
	regmap_update_bits(priv->regmap, MAX96789_REG2, mask, mask);

}

/* -----------------------------------------------------------------------------
 * DRM Bridge Operations
 */
static int max96789_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	int ret;
	struct max96789_priv *priv = bridge_to_max96789_priv(bridge);
	struct device *dev = priv->dev;
	struct drm_bridge *next_bridge[GMSL_MAX_LINKS];
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	const struct mipi_dsi_device_info info = { .type = "max96789_bridge",
						   .channel = 0,
						   .node = NULL,
						 };
	unsigned int link;

	dev_info(dev, "%s: entered drm bridge attach\n", __func__);

	for(link = GMSL_LINK_A; link < GMSL_MAX_LINKS; link++){
		if(!priv->gmsl_link_mask[link])
			continue;
	
		next_bridge[link] = devm_drm_of_get_bridge(dev, dev->of_node, link + 1, 0);

		if(IS_ERR(next_bridge[link]))
			return dev_err_probe(dev, PTR_ERR(next_bridge[link]),
					     "%s: empty bridge node fetched\n", __func__);

		priv->next_bridge[link] = next_bridge[link];
	}

	host = of_find_mipi_dsi_host_by_node(priv->host_node);
	if (!host) {
		dev_err(dev, "%s: failed to find dsi host\n", __func__);
		return -ENODEV;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) 
		return dev_err_probe(dev, PTR_ERR(dsi),
				     "%s: failed to create dsi device\n", __func__);
	

	/* lane can be parsed from DT */
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "%s: failed to attach dsi to host\n", __func__);

	/* -------NOTICE---------
	 * Tricky use case:
	 * 	Test for attaching both deserializer into encoder's bridge list
	 *
	 * Drm bridge is a daisy-chain architecture, while Serdes treats the
	 * downstream device more as a tree. It won't be suitable for
	 * this linear drm bridge structure or may have some side effect
	 * to check in a further validation.
	 */	
	for(link = GMSL_LINK_A; link < GMSL_MAX_LINKS ; link++) {
		if(!priv->next_bridge[link])
			continue;
		ret = drm_bridge_attach(bridge->encoder,
					priv->next_bridge[link], &priv->bridge,
					flags | DRM_BRIDGE_ATTACH_NO_CONNECTOR);

		if(ret < 0) {
			dev_err(dev,"%s: Bridge attach failed\n", __func__);
			goto mipi_dsi_unregister;
		}
	}

	/*-------------------------------------------------------------*/

	if(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)
		return 0;
	
	priv->connector = drm_bridge_connector_init(priv->bridge.dev, priv->bridge.encoder);
	
	if (IS_ERR(priv->connector)) {
		ret = PTR_ERR(priv->connector);
		dev_err(dev, "%s: Failed to initialize connector with drm\n", __func__);
		goto mipi_dsi_unregister;
	}	

	drm_connector_attach_encoder(priv->connector, priv->bridge.encoder);

	return 0;

mipi_dsi_unregister:
	mipi_dsi_detach(dsi);
	mipi_dsi_device_unregister(dsi);
	return ret;
}

static void max96789_atomic_bridge_enable(struct drm_bridge *bridge,
					  struct drm_bridge_state *old_bridge_state)
{
	struct max96789_priv *priv = bridge_to_max96789_priv(bridge);
	enum max96789_video_pipe_id vpip = VIDEO_PIPE_X;
	int mask, val, shift;

	dev_info(priv->dev, "%s: entered enable func\n", __func__);
	gpiod_set_value_cansleep(priv->gpiod_pwdn, 1);

	regmap_write(priv->regmap, 0x1A, 0x08);
	
	if (DEBUG_COLOR_PATTERN == 1)
		max96789_patgen(priv, SER_PATTERN_SEL);

	DEBUG_INFO(priv);

	/* 0x0332: MIPI_RX2 */
	val = priv->dsi_id == DSI_PORT_A ? 0x4E : 0xE4;
	regmap_write(priv->regmap, MAX96789_MIPI_RX2, val);

	/* 0x0331: MIPI_RX1 */
	mask  = priv->dsi_id == DSI_PORT_A ? CTRL0_NUM_LANES : CTRL1_NUM_LANES;
	shift = priv->dsi_id == DSI_PORT_A ? 0 : 4;
	val = (priv->data_lanes - 1) << shift;
	regmap_update_bits(priv->regmap, MAX96789_MIPI_RX1, mask, val);

	/* 0x0330: MIPI_RX0 */
	val = priv->dsi_id == DSI_PORT_A ? PHY_CFG_ONLYA: PHY_CFG_ONLYB;
	regmap_update_bits(priv->regmap, MAX96789_MIPI_RX0, PHY_CONFIG, val);
	
	if(priv->gmsl_links_used ==2 && !priv->gmsl2_dual_link) {
		max96789_dsi_cfg_for_split(priv);
		return;
	}
	/*-------------------------------------------------------------*/
	/* 0x0308: FRONTTOP 0 */
	mask = (CLK_SELX | CLK_SELY | CLK_SELZ | CLK_SELU |
		START_PORTA | START_PORTB | ENABLE_LINE_INFO);

	val  = priv->dsi_id == DSI_PORT_A ? (CLK_SELY |CLK_SELZ | CLK_SELU)
					  : CLK_SELX;
	val |= priv->dsi_id == DSI_PORT_A ? START_PORTA : START_PORTB;
	val |= ENABLE_LINE_INFO;

	regmap_update_bits(priv->regmap, MAX96789_FRONTTOP_0, mask, val);

	/* 0x0311: FRONTTOP 9 */
	val = priv->dsi_id == DSI_PORT_A ? START_PORTAX : START_PORTBX;
	regmap_update_bits(priv->regmap, MAX96789_FRONTTOP_9, val, val);

	/* 0x031C/0x031D/0x031E/0x031F: FRONTTOP_20/21/22/23 soft_dt en */
	regmap_write(priv->regmap, MAX96789_FRONTTOP_20, 0x98);

	/* 0x0321/0x0322/0x0323/0x0324: FRONTTOP_25/26/27/28 soft_dt*/
	regmap_write(priv->regmap, MAX96789_FRONTTOP_25, 0x24);

	/* 0x0053/0x0057/0x005B/0x005F: TX3 CFGV VIDEO_X/Y/Z/U */
	regmap_write(priv->regmap, MAX96789_TX3(vpip), 0x10);

	max96789_video_timing(priv);

	/* 0x0002: REG2 */
	mask = VID_TX_EN_X | 0x3;
	regmap_update_bits(priv->regmap, MAX96789_REG2, mask, mask);
}

static void max96789_atomic_bridge_disable(struct drm_bridge *bridge,
					   struct drm_bridge_state *old_bridge_state)
{
	struct max96789_priv *priv = bridge_to_max96789_priv(bridge);

	gpiod_set_value_cansleep(priv->gpiod_pwdn, 0);
}

static int max96789_bridge_get_modes(struct drm_bridge *bridge,
				     struct drm_connector *connector)
{
	int count;
	count = drm_add_modes_noedid(connector, 8192, 8192);
	drm_set_preferred_mode(connector, 1280, 768);
	return count;
}

static const struct drm_bridge_funcs max96789_bridge_funcs = {
	.attach = max96789_bridge_attach,
	.atomic_enable = max96789_atomic_bridge_enable,
	.atomic_disable = max96789_atomic_bridge_disable,

	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state =drm_atomic_helper_bridge_duplicate_state ,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,

	.get_modes = max96789_bridge_get_modes,
};

static int max96789_parse_dt(struct max96789_priv *priv)
{
	struct device *dev = priv->dev;
	struct device_node *dsi_node;
	int data_lanes, ep, ret;

	/* 
	 * get dsi node 
	 * max96789 is capable to use both dsi port
	 * Driver only use either dsi port as input at this stage
	 */
	for(ep = DSI_PORT_A; ep < DSI_MAX_PORTS; ep++) {
		dsi_node = of_graph_get_endpoint_by_regs(dev->of_node, 0, ep);
		if(dsi_node) {
			priv->host_node = of_graph_get_remote_node(priv->dev->of_node, 0, ep);
			priv->dsi_id = ep;
			break;
		}
	}

	if(ep == DSI_MAX_PORTS) {
		dev_err(dev, "no dsi port links to max96789 in DT\n");
		return -ENXIO;
	}

	/* kernel 5.10 doesn't have drm_of_get_data_lanes_count() to use */
	data_lanes = of_property_count_u32_elems(dsi_node, "data-lanes");

	if(data_lanes < 0 || data_lanes < 1 || data_lanes > 4) {
		dev_err(dev, "invalid data-lanes property for dsi ep %d\n",
			priv->dsi_id);
		return -EINVAL;
	}

	priv->data_lanes = data_lanes;

	ret = max96789_parse_gmsl_link_cfg(priv);
	
	if(ret)
		return ret;

	return 0;
}

static int max96789_bridge_probe(struct i2c_client *client)
{
	struct max96789_priv *priv;
	struct device *dev = &client->dev;
	int ret;

	dev_info(dev, "%s: entered probe func\n", __func__);

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if(!priv)
		return -ENOMEM;

	priv->dev = dev;

	priv->regmap = devm_regmap_init_i2c(client, &max96789_i2c_regmap);
	if(IS_ERR(priv->regmap))
		return dev_err_probe(priv->dev, PTR_ERR(priv->regmap),
			       	     "failed to init i2c regmap\n");

	priv->gpiod_pwdn = devm_gpiod_get_optional(&client->dev, "enable",
						   GPIOD_OUT_HIGH);
	if(IS_ERR(priv->gpiod_pwdn))
		return dev_err_probe(priv->dev, PTR_ERR(priv->gpiod_pwdn),
				     "failed to request pwdn gpio\n");

	gpiod_set_consumer_name(priv->gpiod_pwdn, "max96789-pwdn");

	if(priv->gpiod_pwdn)
		usleep_range(4000, 5000);

	priv->client = client;

	i2c_set_clientdata(client, priv);

	ret = max96789_parse_dt(priv);
	if(ret < 0)
		return ret;

	priv->bridge.driver_private = priv;
	priv->bridge.funcs = &max96789_bridge_funcs;
	priv->bridge.of_node = priv->dev->of_node;
	priv->bridge.type = DRM_MODE_CONNECTOR_VIRTUAL;
	priv->bridge.ops = DRM_BRIDGE_OP_MODES;
		
	drm_bridge_add(&priv->bridge);

	ret = device_create_file(dev, &dev_attr_dual_view);
	if (ret) {
		dev_err(dev, "failed to create dual view controll sysfs file %d\n", ret);
		return ret;
	}
	
	return max96789_dev_init(priv);
}

static int max96789_bridge_remove(struct i2c_client *client)
{	
	struct max96789_priv *priv = i2c_get_clientdata(client);

	regmap_exit(priv->regmap);

	drm_bridge_remove(&priv->bridge);

	return 0;
}

static const struct of_device_id max96789_bridge_match_table[] = {
	{.compatible = "maxim,max96789"},
	{},
};
MODULE_DEVICE_TABLE(of, max96789_bridge_match_table);

static struct i2c_driver max96789_bridge_driver = {
	.driver = {
		.name = "maxim-max96789",
		.of_match_table = max96789_bridge_match_table,
	},
	.probe_new = max96789_bridge_probe,
	.remove = max96789_bridge_remove,
};
module_i2c_driver(max96789_bridge_driver);

MODULE_AUTHOR("Felix Hsu <felixhsu@retronix.com.tw>");
MODULE_DESCRIPTION("Max96789 MIPI-DSI to GMSL2 maxim serdes bridge driver");
MODULE_LICENSE("GPL v2");
