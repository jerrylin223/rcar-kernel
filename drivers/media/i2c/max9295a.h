#ifndef __MAX9295A_H__
#define __MAX9295A_H__

/* Register 0x04 */
#define MAX9295A_SEREN			BIT(7)
#define MAX9295A_CLINKEN		BIT(6)
#define MAX9295A_PRBSEN			BIT(5)
#define MAX9295A_SLEEP			BIT(4)
#define MAX9295A_INTTYPE_I2C		(0 << 2)
#define MAX9295A_INTTYPE_UART		(1 << 2)
#define MAX9295A_INTTYPE_NONE		(2 << 2)
#define MAX9295A_REVCCEN		BIT(1)
#define MAX9295A_FWDCCEN		BIT(0)
/* Register 0x07 */
#define MAX9295A_DBL			BIT(7)
#define MAX9295A_DRS			BIT(6)
#define MAX9295A_BWS			BIT(5)
#define MAX9295A_ES			BIT(4)
#define MAX9295A_HVEN			BIT(2)
#define MAX9295A_EDC_1BIT_PARITY	(0 << 0)
#define MAX9295A_EDC_6BIT_CRC		(1 << 0)
#define MAX9295A_EDC_6BIT_HAMMING	(2 << 0)
/* Register 0x08 */
#define MAX9295A_INVVS			BIT(7)
#define MAX9295A_INVHS			BIT(6)
#define MAX9295A_REV_LOGAIN		BIT(3)
#define MAX9295A_REV_HIVTH		BIT(0)
/* Register 0x09 */
#define MAX9295A_ID_REG			0x09
/* Register 0x0d */
#define MAX9295A_I2CLOCACK		BIT(7)
#define MAX9295A_I2CSLVSH_1046NS_469NS	(3 << 5)
#define MAX9295A_I2CSLVSH_938NS_352NS	(2 << 5)
#define MAX9295A_I2CSLVSH_469NS_234NS	(1 << 5)
#define MAX9295A_I2CSLVSH_352NS_117NS	(0 << 5)
#define MAX9295A_I2CMSTBT_837KBPS	(7 << 2)
#define MAX9295A_I2CMSTBT_533KBPS	(6 << 2)
#define MAX9295A_I2CMSTBT_339KBPS	(5 << 2)
#define MAX9295A_I2CMSTBT_173KBPS	(4 << 2)
#define MAX9295A_I2CMSTBT_105KBPS	(3 << 2)
#define MAX9295A_I2CMSTBT_84KBPS	(2 << 2)
#define MAX9295A_I2CMSTBT_28KBPS	(1 << 2)
#define MAX9295A_I2CMSTBT_8KBPS		(0 << 2)
#define MAX9295A_I2CSLVTO_NONE		(3 << 0)
#define MAX9295A_I2CSLVTO_1024US	(2 << 0)
#define MAX9295A_I2CSLVTO_256US		(1 << 0)
#define MAX9295A_I2CSLVTO_64US		(0 << 0)
/* Register 0x0f */
#define MAX9295A_GPIO5OUT		BIT(5)
#define MAX9295A_GPIO4OUT		BIT(4)
#define MAX9295A_GPIO3OUT		BIT(3)
#define MAX9295A_GPIO2OUT		BIT(2)
#define MAX9295A_GPIO1OUT		BIT(1)
#define MAX9295A_SETGPO			BIT(0)
/* Register 0x15 */
#define MAX9295A_PCLKDET		BIT(0)

#define MAX9295_REG2			0x02
#define MAX9295_REG7			0x07
#define MAX9295_CTRL0			0x10
#define MAX9295_I2C2			0x42
#define MAX9295_I2C3			0x43
#define MAX9295_I2C4			0x44
#define MAX9295_I2C5			0x45
#define MAX9295_I2C6			0x46

#define MAX9295_CROSS(n)		(0x1b0 + n)

#define MAX9295_GPIO_A(n)		(0x2be + (3 * n))
#define MAX9295_GPIO_B(n)		(0x2bf + (3 * n))
#define MAX9295_GPIO_C(n)		(0x2c0 + (3 * n))

#define MAX9295_VIDEO_TX_BASE(n)	(0x100 + (0x8 * n))
#define MAX9295_VIDEO_TX0(n)		(MAX9295_VIDEO_TX_BASE(n) + 0)
#define MAX9295_VIDEO_TX1(n)		(MAX9295_VIDEO_TX_BASE(n) + 1)

#define MAX9295_FRONTTOP_0		0x308
#define MAX9295_FRONTTOP_9		0x311
#define MAX9295_FRONTTOP_12		0x314
#define MAX9295_FRONTTOP_13		0x315

#define MAX9295_MIPI_RX0		0x330
#define MAX9295_MIPI_RX1		0x331
#define MAX9295_MIPI_RX2		0x332
#define MAX9295_MIPI_RX3		0x333

struct max9295a_reg {
	u16	reg;
	u8	val;
};

static const struct max9295a_reg configuretable_ar0231[] = {
	{0x0002, 0x03}, /* VideoTX Disable and write 3 to reserved bits */
	{0x0100, 0x60}, /* VIDEO_TX) - Line CRC enabled.  Encoding ON. Read back 62. */
	{0x0101, 0x0A}, /* VIDEO_TX) - BPP Setting 10 bits. */

	// Map GPIO8 for FV_OUT:  MAX96712 <== Camera-ISP(AP0202)-MAX9295A output
	{0x02D6, 0x63}, /* GPIO_A - GPIO Tx */
	{0x02D7, 0x2B}, /* GPIO_B - GPIO_TX_ID=11 */
	{0x02D8, 0x0B}, /* GPIO_C(dummy) */

	// MAX96712 MFP2 / MAX9295A MFP7.
	// Map GPIO7 for FR_SYNC: MAX96712 ==> Camera-ISP(AP0202)-MAX9295A input
	{0x02D3, 0x84}, /* GPIO_A - GPIO Rx */
	{0x02D4, 0x2C}, /* GPIO_B(dummy) */
	{0x02D5, 0x0C}, /* GPIO_C - for GPIO7 ... GPIO_RX_ID=12 */
	
	{0x0007, 0xC7}, /* Configure serializer for parallel sensor input */
	{0x0332, 0xEE}, /* PHY lane mapping */
	{0x0333, 0xE4}, /* PHY lane mapping */
	{0x0314, 0x2B}, /* Select designated datatype to route to Video Pipeline X */
	{0x0316, 0x22}, /* Select designated datatype to route to Video Pipeline Y */
	{0x0318, 0x22}, /* Select designated datatype to route to Video Pipeline Z */
	{0x031A, 0x22}, /* Select designated datatype to route to Video Pipeline U */
	{0x031C, 0x2A}, /* Soft BPP Pipe X */
	{0x0002, 0x13}, /* Video transmit Pipe X enable */
	{0x03F1, 0x89}  /* Output RCLK to Sensor */
};

#endif
