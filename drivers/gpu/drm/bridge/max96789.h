#define DEBUG_COLOR_PATTERN			0

#define MAX96789_NUM_GMSL			2

#define HI_NIBBLE(b) (((b) >> 4) & 0x0F)
#define LO_NIBBLE(b) ((b) & 0x0F)

#define MAX96789_PWR0  				0x08
#define MAX96789_PWR4  				0x0C
#define   WAKE_EN_B				BIT(5)
#define   WAKE_EN_A				BIT(4)
#define MAX96789_REG1				0x01
#define   TX_RATE_MASK				GENMASK(3,2)
#define   TX_RATE_SHIFT				2
#define   TX_3GBPS				1
#define   TX_6GBPS				2
#define MAX96789_REG2				0x02
#define   VID_TX_EN_Y				BIT(5)
#define   VID_TX_EN_X				BIT(4)
#define MAX96789_REG3				0x03
#define MAX96789_REG4				0x04
#define   GMSL2_B				BIT(7)
#define   GMSL2_A				BIT(6)
#define   LINK_EN_B				BIT(5)
#define   LINK_EN_A				BIT(4)
#define MAX96789_REG5				0x05
#define MAX96789_REG6				0x06
#define MAX96789_REG13				0x0D
#define MAX96789_REG15				0x0F
#define   SPLTR_CPBL_N				BIT(3)

#define MAX96789_CTRL0 				0x10
#define   RESET_LINK				BIT(6)
#define   RESET_ONESHOT				BIT(5)
#define   AUTO_LINK				BIT(4)
#define   SLEEP					BIT(3)
#define   LINK_CFG_MASK				GENMASK(1,0)
#define   LINK_CFG_DUAL				0
#define   LINK_CFG_SINGLE_A			1
#define   LINK_CFG_SINGLE_B			2
#define   LINK_CFG_SPLITTER			3
#define MAX96789_CTRL1 				0x11
#define   CXTP_B				BIT(1)
#define   CXTP_A				BIT(0)
#define MAX96789_CTRL2				0x12
#define MAX96789_CTRL3				0x13
#define   LOCKED				BIT(3)
#define MAX96789_INTR7				0x1F
#define   LOCK_B				BIT(4)
#define   LOCK_A				BIT(3)

#define MAX96789_GPIO_BASE(n)			(0x2BE + n)
#define MAX96789_GPIO_A(n) 			(MAX96789_GPIO_BASE(0) + (3 * n))
#define MAX96789_GPIO_B(n) 			(MAX96789_GPIO_BASE(1) + (3 * n))
#define MAX96789_GPIO_C(n) 			(MAX96789_GPIO_BASE(2) + (3 * n))

#define MAX96789_I2C_0				0x40
#define 	I2C_0_SLV_SH			GENMASK(5,4)
#define MAX96789_I2C_1				0x41
#define		I2C_1_MST_BT			GENMASK(6,4)
#define MAX96789_I2C_2				0x42
#define MAX96789_I2C_3				0x43
#define MAX96789_I2C_4				0x44
#define MAX96789_I2C_5				0x45

#define MAX96789_FRONTTOP_0			0x308
#define   ENABLE_LINE_INFO			BIT(6)
#define   START_PORTB				BIT(5)
#define   START_PORTA				BIT(4)
#define   CLK_SELU				BIT(3)
#define   CLK_SELZ				BIT(2)
#define   CLK_SELY				BIT(1)
#define   CLK_SELX				BIT(0)
#define MAX96789_FRONTTOP_9			0x311
#define   START_PORTBU				BIT(7)
#define   START_PORTBZ				BIT(6)
#define   START_PORTBY				BIT(5)
#define   START_PORTBX				BIT(4)
#define   START_PORTAU				BIT(3)
#define   START_PORTAZ				BIT(2)
#define   START_PORTAY				BIT(1)
#define   START_PORTAX				BIT(0)
#define MAX96789_FRONTTOP_20			0x31C
#define MAX96789_FRONTTOP_21			0x31D
#define MAX96789_FRONTTOP_25			0x321
#define MAX96789_FRONTTOP_26			0x322
#define MAX96789_FRONTTOP_29			0x325
#define MAX96789_FRONTTOP_30			0x326

#define MAX96789_MIPI_RX0			0x330
#define   PHY_CONFIG				GENMASK(2,0)
#define   PHY_CFG_BOTH				6
#define   PHY_CFG_ONLYB				5
#define   PHY_CFG_ONLYA				4
#define MAX96789_MIPI_RX1			0x331
#define   CTRL1_NUM_LANES			GENMASK(5,4)
#define   CTRL0_NUM_LANES			GENMASK(1,0)
#define MAX96789_MIPI_RX2			0x332
#define   PHY1_LANE_MAP				GENMASK(7,4)
#define   PHY0_LANE_MAP				GENMASK(3,0)
#define MAX96789_MIPI_RX3			0x333
#define MAX96789_MIPI_RX4			0x334
#define MAX96789_MIPI_RX5			0x335
#define MAX96789_MIPI_RX8			0x338

// Controller 0
#define MAX96789_MIPI_DSI0			0x380
#define MAX96789_MIPI_DSI1			0x381
#define MAX96789_MIPI_DSI2			0x382
#define MAX96789_MIPI_DSI5			0x385
#define MAX96789_MIPI_DSI6			0x386
#define MAX96789_MIPI_DSI7			0x387
#define MAX96789_MIPI_DSI8			0x388
#define MAX96789_MIPI_DSI9			0x389
#define MAX96789_MIPI_DSI10			0x38A
#define MAX96789_MIPI_DSI11			0x38B
#define MAX96789_MIPI_DSI12			0x38C
#define MAX96789_MIPI_DSI13			0x38D
#define MAX96789_MIPI_DSI14			0x38E
#define MAX96789_MIPI_DSI15			0x38F
#define MAX96789_MIPI_DSI32			0x3A0
#define MAX96789_MIPI_DSI36			0x3A4
#define MAX96789_MIPI_DSI37			0x3A5
#define MAX96789_MIPI_DSI38			0x3A6
#define MAX96789_MIPI_DSI39			0x3A7
#define MAX96789_MIPI_DSI40			0x3A8
#define MAX96789_MIPI_DSI41			0x3A9
#define MAX96789_MIPI_DSI42			0x3AA
#define MAX96789_MIPI_DSI43			0x3AB
#define MAX96789_MIPI_DSI44			0x3AC
#define MAX96789_MIPI_DSI45			0x3AD
#define MAX96789_MIPI_DSI46			0x3AE

#define MAX96789_TX_BASE(n)			(0x50 + n * 4)
#define MAX96789_TX0(n)				(MAX96789_TX_BASE(n) + 0)
#define MAX96789_TX1(n)				(MAX96789_TX_BASE(n) + 1)
#define MAX96789_TX3(n)				(MAX96789_TX_BASE(n) + 3)

#define MAX96789_VIDEO_TX_BASE(n)		(0x100 + n * 8)
#define MAX96789_VIDEO_TX0(n)			(MAX96789_VIDEO_TX_BASE(n) + 0)
#define MAX96789_VIDEO_TX1(n)			(MAX96789_VIDEO_TX_BASE(n) + 1)
#define MAX96789_VIDEO_TX2(n)			(MAX96789_VIDEO_TX_BASE(n) + 2)
#define MAX96789_VIDEO_TX3(n)			(MAX96789_VIDEO_TX_BASE(n) + 6)
#define   PCLKDET_VTX				BIT(5)

#define MAX96789_VTX_BASE(n)			(0x1B0 + 0x43 * n)
#define MAX96789_VTX_X(n)			(MAX96789_VTX_BASE(0) + 0x18 + n)
#define MAX96789_VTX_Y(n)			(MAX96789_VTX_BASE(1) + 0x18 + n)
#define MAX96789_VTX_Z(n)			(MAX96789_VTX_BASE(2) + 0x18 + n)
#define MAX96789_VTX_U(n)			(MAX96789_VTX_BASE(3) + 0x18 + n)

#define MIPI_DT_RGB888				0x24

#define MAX96789_HS_VS(n)			(0x55D + n)
#define MAX96789_HS_VS_X			MAX96789_HS_VS(0)
#define MAX96789_HS_VS_Y			MAX96789_HS_VS(1)
#define MAX96789_HS_VS_Z			MAX96789_HS_VS(2)
#define MAX96789_HS_VS_U			MAX96789_HS_VS(3)
#define   DE_DET_X				BIT(6)
#define   VS_DET_X				BIT(5)
#define   HS_DET_X				BIT(4)

#define MAX96776_ID				0x8C
#define MAX96778_ID				0x8D

#define REG8_NUM_RETRIES			1 /* number of read/write retries */
#define REG16_NUM_RETRIES			10 /* number of read/write retries */

static const struct regmap_config max96789_i2c_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x1f00,
};

enum max96789_dsi_port_id {
	DSI_PORT_A,
	DSI_PORT_B,
	DSI_MAX_PORTS,
};

enum max96789_gmsl_link_id {
	GMSL_LINK_A,
	GMSL_LINK_B,
	GMSL_MAX_LINKS,
};

enum max96789_gmsl_link_type {
	TYPE_GMSL1,
	TYPE_GMSL2,
};

enum max96789_gmsl_dev_type {
	TYPE_NO_DEV,
	TYPE_DEV_UNKNOWN,
	TYPE_DEV_FIXED,
};

enum max96789_gmsl_link_rate {
	GMSL2_RATE_3G,
	GMSL2_RATE_6G,
};

enum max96789_video_pipe_id {
	VIDEO_PIPE_X,
	VIDEO_PIPE_Y,
	VIDEO_PIPE_Z,
	VIDEO_PIPE_U,
	VIDEO_MAX_PIPES,
};

enum max96789_gmsl_cab_sel {
	CABLE_TWIST_PAIR,
	CABLE_COAX,
};

struct max96789_priv {
	struct device 		*dev;
	struct i2c_client	*client;
	struct regmap 		*regmap;
	struct gpio_desc 	*gpiod_pwdn;
	struct device_node	*host_node;
	
	struct drm_connector	*connector;
	struct drm_bridge 	bridge;
	struct drm_bridge 	*next_bridge[GMSL_MAX_LINKS];

	unsigned int dsi_id;
	int data_lanes;
	
	/* gmsl param */
	unsigned int gmsl_links_used;
	enum max96789_gmsl_dev_type	gmsl_link_mask[GMSL_MAX_LINKS];
	enum max96789_gmsl_link_type	gmsl_link_types[GMSL_MAX_LINKS];
	enum max96789_gmsl_link_rate 	gmsl_link_rate;
	bool gmsl2_dual_link;
	enum max96789_gmsl_cab_sel	gmsl_cab_sel[GMSL_MAX_LINKS];
};
/* -----------------------------------------------------------------------------
 * I2C IO
 */
static int max96789_write_n(struct max96789_priv *priv, int reg, int val_count, int val)
{
	int ret;
	int i;
	
	u8 values[3];
	for (i = 0; i < val_count; i++)
	{
		values[i] = (val >> ((val_count - i - 1) * 8)) & 0xff;
		ret = regmap_write(priv->regmap, reg, values[i]);
		if (ret)
		{
			dev_dbg(&priv->client->dev, "write register 0x%04x failed (%d)\n", reg, ret);
			return ret;
		}
		reg += 1;
	}
	return 0;
}
