// SPDX-License-Identifier: GPL-2.0
/*
 * gc2093 sensor driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 * V0.0X01.0X01 Add HDR support.
 * V0.0X01.0X02 update sensor driver
 * 1. fix linear mode ae flicker issue.
 * 2. add hdr mode exposure limit issue.
 * 3. fix hdr mode highlighting pink issue.
 * 4. add some debug info.
 */
//#define DEBUG
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <linux/rk-preisp.h>

#include <media/v4l2-async.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-tb-setup.h"

#define DRIVER_VERSION		KERNEL_VERSION(0, 0x01, 0x02)
#define GC2093_CLK_24M_NAME		"gc2093_clk_24m"
#define GC2093_CLK_24M_MEDIA_BUS_FMT	MEDIA_BUS_FMT_SRGGB10_1X10

#define MIPI_FREQ_297M		297000000
#define MIPI_FREQ_396M		396000000

/* 27M or 24M */
//#define MCLK_27M

#ifdef MCLK_27M
#define GC2093_XVCLK_FREQ	27000000
#else
#define GC2093_XVCLK_FREQ	24000000
#endif

#define GC2093_CLK_24M_REG_CHIP_ID_H	0x03F0
#define GC2093_CLK_24M_REG_CHIP_ID_L	0x03F1

#define GC2093_CLK_24M_REG_EXP_SHORT_H	0x0001
#define GC2093_CLK_24M_REG_EXP_SHORT_L	0x0002
#define GC2093_CLK_24M_REG_EXP_LONG_H	0x0003
#define GC2093_CLK_24M_REG_EXP_LONG_L	0x0004

#define GC2093_CLK_24M_REG_VB_H		0x0007
#define GC2093_CLK_24M_REG_VB_L		0x0008

#define GC2093_CLK_24M_REG_VTS_H	0x0041
#define GC2093_CLK_24M_REG_VTS_L	0x0042

#define GC2093_CLK_24M_MIRROR_FLIP_REG	0x0017
#define MIRROR_MASK		BIT(0)
#define FLIP_MASK		BIT(1)

#define GC2093_CLK_24M_REG_CTRL_MODE	0x003E
#define GC2093_CLK_24M_MODE_SW_STANDBY	0x11
#define GC2093_CLK_24M_MODE_STREAMING	0x91

#define GC2093_CLK_24M_CHIP_ID		0x2093

#define GC2093_CLK_24M_VTS_MAX		0x3FFF
#define GC2093_CLK_24M_HTS_MAX		0xFFF

#define GC2093_CLK_24M_EXPOSURE_MAX	0x3FFF
#define GC2093_CLK_24M_EXPOSURE_MIN	1
#define GC2093_CLK_24M_EXPOSURE_STEP	1

#define GC2093_CLK_24M_GAIN_MIN		0x40
#define GC2093_CLK_24M_GAIN_MAX		0x2000
#define GC2093_CLK_24M_GAIN_STEP	1
#define GC2093_CLK_24M_GAIN_DEFAULT	64
#define REG_NULL		0xFFFF

#define GC2093_CLK_24M_LANES		2

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

static const char * const gc2093_clk_24m_supply_names[] = {
	"dovdd",    /* Digital I/O power */
	"avdd",     /* Analog power */
	"dvdd",     /* Digital power */
};

#define GC2093_CLK_24M_NUM_SUPPLIES ARRAY_SIZE(gc2093_clk_24m_supply_names)

#define to_gc2093_clk_24m(sd) container_of(sd, struct gc2093_clk_24m, subdev)

enum {
	LINK_FREQ_297M_INDEX,
	LINK_FREQ_396M_INDEX,
};

struct gain_reg_config {
	u32 value;
	u16 analog_gain;
	u16 col_gain;
	u16 analog_sw;
	u16 ram_width;
};

struct gc2093_clk_24m_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 link_freq_index;
	const struct reg_sequence *reg_list;
	u32 reg_num;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct gc2093_clk_24m {
	struct device	*dev;
	struct clk	*xvclk;
	struct regmap	*regmap;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct regulator_bulk_data supplies[GC2093_CLK_24M_NUM_SUPPLIES];

	struct v4l2_subdev  subdev;
	struct media_pad    pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl    *exposure;
	struct v4l2_ctrl    *anal_gain;
	struct v4l2_ctrl    *hblank;
	struct v4l2_ctrl    *vblank;
	struct v4l2_ctrl    *h_flip;
	struct v4l2_ctrl    *v_flip;
	struct v4l2_ctrl    *link_freq;
	struct v4l2_ctrl    *pixel_rate;

	struct mutex        lock;
	bool		    streaming;
	bool		    power_on;
	const struct gc2093_clk_24m_mode *cur_mode;

	u32		module_index;
	const char      *module_facing;
	const char      *module_name;
	const char      *len_name;

	struct v4l2_fract	cur_fps;
	u32			cur_vts;

	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

static const struct regmap_config gc2093_clk_24m_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x04f0,
};

static const s64 link_freq_menu_items[] = {
	MIPI_FREQ_297M,
	MIPI_FREQ_396M,
};

/*
 * window size=1920*1080 mipi@2lane
 * mclk=27M mipi_clk=594Mbps
 * pixel_line_total=2200 line_frame_total=1125
 * row_time=29.62us frame_rate=30fps
 */
static const struct reg_sequence gc2093_clk_24m_1080p_liner_settings[] = {
#ifdef MCLK_27M
	/* System */
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0a},
	{0x03f7, 0x01},
	{0x03f8, 0x2c},
	{0x03f9, 0x10},
	{0x03fc, 0x8e},
	/* Cisctl & Analog */
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xb7},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x00},
	{0x0004, 0x02},
	{0x0005, 0x04},
	{0x0006, 0x4c},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0x65},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x38},
	{0x004a, 0x01},
	{0x004b, 0x28},
	{0x0055, 0x38},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/* Gain */
	{0x00b6, 0xc0},
	{0x00b0, 0x60},
	/* Isp */
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010f, 0x00},
	{0x0158, 0x00},
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x00},
	{0x0122, 0x10},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/* Blk */
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x07},
	{0x014b, 0x80},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	/* Window */
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	/* MIPI */
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0xce},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x003e, 0x91},
#else
	/****system****/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x11},
	{0x03f8, 0x30},
	{0x03f9, 0x42},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0003, 0x04},
	{0x0004, 0x65},
	{0x0005, 0x05},
	{0x0006, 0x8e},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0x65},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x40},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x30},
	{0x004a, 0x01},
	{0x004b, 0x28},
	{0x0055, 0x30},
	{0x0066, 0x3f},
	{0x0068, 0x3f},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x68},
	{0x00b3, 0x00},
	{0x00b8, 0x01},
	{0x00b9, 0x00},
	{0x00b1, 0x01},
	{0x00b2, 0x00},
	/*isp*/
	{0x0101, 0x0c},
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},
	{0x010f, 0x00},
	{0x0158, 0x00},
	{0x0428, 0x86},
	{0x0429, 0x86},
	{0x042a, 0x86},
	{0x042b, 0x68},
	{0x042c, 0x68},
	{0x042d, 0x68},
	{0x042e, 0x68},
	{0x042f, 0x68},
	{0x0430, 0x4f},
	{0x0431, 0x68},
	{0x0432, 0x67},
	{0x0433, 0x66},
	{0x0434, 0x66},
	{0x0435, 0x66},
	{0x0436, 0x66},
	{0x0437, 0x66},
	{0x0438, 0x62},
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x04},
	{0x0122, 0x65},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x07},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0xb6},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x10},
	{0x003e, 0x91},
#endif
};

/*
 * window size=1920*1080 mipi@2lane
 * mclk=27M mipi_clk=792Mbps
 * pixel_line_total=2640 line_frame_total=1250
 * row_time=13.33us frame_rate=60fps
 */
static const struct reg_sequence gc2093_clk_24m_1080p_hdr_settings[] = {
#ifdef MCLK_27M
	/* System */
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x01},
	{0x03f8, 0x58},
	{0x03f9, 0x40},
	{0x03fc, 0x8e},
	/* Cisctl & Analog */
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x02},
	{0x0005, 0x02},
	{0x0006, 0x94},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x04},
	{0x0042, 0xe2},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x20},
	{0x004a, 0x01},
	{0x004b, 0x20},
	{0x0055, 0x30},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/* Gain */
	{0x00b6, 0xc0},
	{0x00b0, 0x60},
	/* Isp */
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x01},
	{0x0158, 0x00},
	{0x0183, 0x01},
	{0x0187, 0x50},
	/* Dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x00},
	{0x0122, 0x10},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/* Blk */
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x0454, 0x78},
	{0x0455, 0x78},
	{0x0456, 0x78},
	{0x0457, 0x78},
	{0x04e0, 0x18},
	/* Window */
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	/* MIPI */
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0xb6},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x12},
	{0x003e, 0x91},
	/* HDR En */
	{0x0027, 0x71},
	{0x0215, 0x92},
	{0x024d, 0x01},
#else
	/****system****/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0B},
	{0x03f7, 0x01},
	{0x03f8, 0x63},
	{0x03f9, 0x40},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x02},
	{0x0005, 0x02},
	{0x0006, 0x94},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x05}, //30fps: 0x4e2;   25FPS: 0x5dc:  20FPS: 0x753 
	{0x0042, 0xdc},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x28},
	{0x004a, 0x01},
	{0x004b, 0x20},
	{0x0055, 0x28},
	{0x0066, 0x3f},
	{0x0068, 0x3f},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x68},
	/*isp*/
	{0x0101, 0x0c},
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x01},
	{0x010f, 0x00},
	{0x0158, 0x00},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x01},
	{0x0121, 0x04},
	{0x0122, 0xd8},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x0f},
	{0x014b, 0x00},
	{0x0155, 0x07},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x0454, 0x78},
	{0x0455, 0x78},
	{0x0456, 0x78},
	{0x0457, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0xb6}, //try 0xce or 0x8e
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x10},
	{0x003e, 0x91},
	/****HDR EN****/
	{0x0027, 0x71},
	{0x0215, 0x92},
	{0x024d, 0x01},
#endif
};


static const struct gc2093_clk_24m_mode supported_modes[] = {
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x460,
		.hts_def = 0x898,
		.vts_def = 0x465,
		.link_freq_index = LINK_FREQ_297M_INDEX,
		.reg_list = gc2093_clk_24m_1080p_liner_settings,
		.reg_num = ARRAY_SIZE(gc2093_clk_24m_1080p_liner_settings),
		.hdr_mode = NO_HDR,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x460,
		.hts_def = 0xa50,
		.vts_def = 0x5dc,//30fps: 0x4e2;   25FPS: 0x5dc:  20FPS: 0x753 
		.link_freq_index = LINK_FREQ_396M_INDEX,
		.reg_list = gc2093_clk_24m_1080p_hdr_settings,
		.reg_num = ARRAY_SIZE(gc2093_clk_24m_1080p_hdr_settings),
		.hdr_mode = HDR_X2,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD1] = V4L2_MBUS_CSI2_CHANNEL_0,//L->csi wr0
		.vc[PAD2] = V4L2_MBUS_CSI2_CHANNEL_1,
		.vc[PAD3] = V4L2_MBUS_CSI2_CHANNEL_1,//M->csi wr2
	},
};

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
/* * 2, to match suitable isp freq */
static u64 to_pixel_rate(u32 index)
{
	u64 pixel_rate = link_freq_menu_items[index] * 2 * GC2093_CLK_24M_LANES * 2;

	do_div(pixel_rate, 10);

	return pixel_rate;
}

static inline int gc2093_clk_24m_read_reg(struct gc2093_clk_24m *gc2093_clk_24m, u16 addr, u8 *value)
{
	unsigned int val;
	int ret;

	ret = regmap_read(gc2093_clk_24m->regmap, addr, &val);
	if (ret) {
		dev_err(gc2093_clk_24m->dev, "i2c read failed at addr: %x\n", addr);
		return ret;
	}

	*value = val & 0xff;

	return 0;
}

static inline int gc2093_clk_24m_write_reg(struct gc2093_clk_24m *gc2093_clk_24m, u16 addr, u8 value)
{
	int ret;

	ret = regmap_write(gc2093_clk_24m->regmap, addr, value);
	if (ret) {
		dev_err(gc2093_clk_24m->dev, "i2c write failed at addr: %x\n", addr);
		return ret;
	}

	return ret;
}

static const struct gain_reg_config gain_reg_configs[] = {
	{  64, 0x0000, 0x0100, 0x6807, 0x00f8},
	{  75, 0x0010, 0x010c, 0x6807, 0x00f8},
	{  90, 0x0020, 0x011b, 0x6c08, 0x00f9},
	{ 105, 0x0030, 0x012c, 0x6c0a, 0x00fa},
	{ 122, 0x0040, 0x013f, 0x7c0b, 0x00fb},
	{ 142, 0x0050, 0x0216, 0x7c0d, 0x00fe},
	{ 167, 0x0060, 0x0235, 0x7c0e, 0x00ff},
	{ 193, 0x0070, 0x0316, 0x7c10, 0x0801},
	{ 223, 0x0080, 0x0402, 0x7c12, 0x0802},
	{ 257, 0x0090, 0x0431, 0x7c13, 0x0803},
	{ 299, 0x00a0, 0x0532, 0x7c15, 0x0805},
	{ 346, 0x00b0, 0x0635, 0x7c17, 0x0807},
	{ 397, 0x00c0, 0x0804, 0x7c18, 0x0808},
	{ 444, 0x005a, 0x0919, 0x7c17, 0x0807},
	{ 523, 0x0083, 0x0b0f, 0x7c17, 0x0807},
	{ 607, 0x0093, 0x0d12, 0x7c19, 0x0809},
	{ 700, 0x0084, 0x1000, 0x7c1b, 0x080c},
	{ 817, 0x0094, 0x123a, 0x7c1e, 0x080f},
	{1131, 0x005d, 0x1a02, 0x7c23, 0x0814},
	{1142, 0x009b, 0x1b20, 0x7c25, 0x0816},
	{1334, 0x008c, 0x200f, 0x7c27, 0x0818},
	{1568, 0x009c, 0x2607, 0x7c2a, 0x081b},
	{2195, 0x00b6, 0x3621, 0x7c32, 0x0823},
	{2637, 0x00ad, 0x373a, 0x7c36, 0x0827},
	{3121, 0x00bd, 0x3d02, 0x7c3a, 0x082b},
};

static int gc2093_clk_24m_set_gain(struct gc2093_clk_24m *gc2093_clk_24m, u32 gain)
{
	int ret, i = 0;
	u16 pre_gain = 0;

	for (i = 0; i < ARRAY_SIZE(gain_reg_configs) - 1; i++)
		if ((gain_reg_configs[i].value <= gain) && (gain < gain_reg_configs[i+1].value))
			break;

	ret = gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00b4, gain_reg_configs[i].analog_gain >> 8);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00b3, gain_reg_configs[i].analog_gain & 0xff);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00b8, gain_reg_configs[i].col_gain >> 8);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00b9, gain_reg_configs[i].col_gain & 0xff);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00ce, gain_reg_configs[i].analog_sw >> 8);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00c2, gain_reg_configs[i].analog_sw & 0xff);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00cf, gain_reg_configs[i].ram_width >> 8);
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00d9, gain_reg_configs[i].ram_width & 0xff);

	pre_gain = 64 * gain / gain_reg_configs[i].value;

	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00b1, (pre_gain >> 6));
	ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x00b2, ((pre_gain & 0x3f) << 2));

	return ret;
}

static void gc2093_clk_24m_modify_fps_info(struct gc2093_clk_24m *gc2093_clk_24m)
{
	const struct gc2093_clk_24m_mode *mode = gc2093_clk_24m->cur_mode;

	gc2093_clk_24m->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      gc2093_clk_24m->cur_vts;
}

static int gc2093_clk_24m_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc2093_clk_24m *gc2093_clk_24m = container_of(ctrl->handler,
					     struct gc2093_clk_24m, ctrl_handler);
	s64 max;
	int ret = 0;
	u32 vts = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = gc2093_clk_24m->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(gc2093_clk_24m->exposure,
					 gc2093_clk_24m->exposure->minimum, max,
					 gc2093_clk_24m->exposure->step,
					 gc2093_clk_24m->exposure->default_value);
		break;
	}
	if (!pm_runtime_get_if_in_use(gc2093_clk_24m->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(gc2093_clk_24m->dev, "set exposure value 0x%x\n", ctrl->val);
		if (gc2093_clk_24m->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		dev_dbg(gc2093_clk_24m->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_EXP_LONG_H,
				       (ctrl->val >> 8) & 0x3f);
		ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_EXP_LONG_L,
					ctrl->val & 0xff);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(gc2093_clk_24m->dev, "set gain value 0x%x, mode: %d\n",
				ctrl->val, gc2093_clk_24m->cur_mode->hdr_mode);
		if (gc2093_clk_24m->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		dev_dbg(gc2093_clk_24m->dev, "set gain value 0x%x\n", ctrl->val);
		gc2093_clk_24m_set_gain(gc2093_clk_24m, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(gc2093_clk_24m->dev, "set blank value 0x%x\n", ctrl->val);
		vts = gc2093_clk_24m->cur_mode->height + ctrl->val;
		gc2093_clk_24m->cur_vts = vts;
		ret = gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_VTS_H,
				       (vts >> 8) & 0x3f);
		ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_VTS_L,
					vts & 0xff);
		if (!ret)
			gc2093_clk_24m->cur_vts = ctrl->val + gc2093_clk_24m->cur_mode->height;
		if (gc2093_clk_24m->cur_vts != gc2093_clk_24m->cur_mode->vts_def)
			gc2093_clk_24m_modify_fps_info(gc2093_clk_24m);
		dev_dbg(gc2093_clk_24m->dev, " set blank value 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(gc2093_clk_24m->dev, "set hflip 0x%x\n", ctrl->val);
		regmap_update_bits(gc2093_clk_24m->regmap, GC2093_CLK_24M_MIRROR_FLIP_REG,
				   MIRROR_MASK, ctrl->val ? MIRROR_MASK : 0);
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(gc2093_clk_24m->dev, "set vflip 0x%x\n", ctrl->val);
		regmap_update_bits(gc2093_clk_24m->regmap, GC2093_CLK_24M_MIRROR_FLIP_REG,
				   FLIP_MASK,  ctrl->val ? FLIP_MASK : 0);
		break;
	default:
		dev_warn(gc2093_clk_24m->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

ctrl_end:
	pm_runtime_put(gc2093_clk_24m->dev);
	return ret;
}

static const struct v4l2_ctrl_ops gc2093_clk_24m_ctrl_ops = {
	.s_ctrl = gc2093_clk_24m_set_ctrl,
};

static int gc2093_clk_24m_get_regulators(struct gc2093_clk_24m *gc2093_clk_24m)
{
	unsigned int i;

	for (i = 0; i < GC2093_CLK_24M_NUM_SUPPLIES; i++)
		gc2093_clk_24m->supplies[i].supply = gc2093_clk_24m_supply_names[i];

	return devm_regulator_bulk_get(gc2093_clk_24m->dev,
				       GC2093_CLK_24M_NUM_SUPPLIES,
				       gc2093_clk_24m->supplies);
}

static int gc2093_clk_24m_initialize_controls(struct gc2093_clk_24m *gc2093_clk_24m)
{
	const struct gc2093_clk_24m_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &gc2093_clk_24m->ctrl_handler;
	mode = gc2093_clk_24m->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &gc2093_clk_24m->lock;

	gc2093_clk_24m->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
						   ARRAY_SIZE(link_freq_menu_items) - 1, 0,
						   link_freq_menu_items);

	gc2093_clk_24m->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
					       0, to_pixel_rate(LINK_FREQ_396M_INDEX),
					       1, to_pixel_rate(LINK_FREQ_297M_INDEX));

	h_blank = mode->hts_def - mode->width;
	gc2093_clk_24m->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (gc2093_clk_24m->hblank)
		gc2093_clk_24m->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	gc2093_clk_24m->cur_fps = mode->max_fps;
	vblank_def = mode->vts_def - mode->height;
	gc2093_clk_24m->cur_vts = mode->vts_def;
	gc2093_clk_24m->vblank = v4l2_ctrl_new_std(handler, &gc2093_clk_24m_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   GC2093_CLK_24M_VTS_MAX - mode->height,
					   1, vblank_def);

	exposure_max = mode->vts_def - 4;
	gc2093_clk_24m->exposure = v4l2_ctrl_new_std(handler, &gc2093_clk_24m_ctrl_ops,
					     V4L2_CID_EXPOSURE, GC2093_CLK_24M_EXPOSURE_MIN,
					     exposure_max, GC2093_CLK_24M_EXPOSURE_STEP,
					     mode->exp_def);

	gc2093_clk_24m->anal_gain = v4l2_ctrl_new_std(handler, &gc2093_clk_24m_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN, GC2093_CLK_24M_GAIN_MIN,
					      GC2093_CLK_24M_GAIN_MAX, GC2093_CLK_24M_GAIN_STEP,
					      GC2093_CLK_24M_GAIN_DEFAULT);

	gc2093_clk_24m->h_flip = v4l2_ctrl_new_std(handler, &gc2093_clk_24m_ctrl_ops,
					   V4L2_CID_HFLIP, 0, 1, 1, 0);

	gc2093_clk_24m->v_flip = v4l2_ctrl_new_std(handler, &gc2093_clk_24m_ctrl_ops,
					   V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(gc2093_clk_24m->dev, "Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc2093_clk_24m->subdev.ctrl_handler = handler;
	gc2093_clk_24m->has_init_exp = false;
	gc2093_clk_24m->cur_vts = mode->vts_def;
	gc2093_clk_24m->cur_fps = mode->max_fps;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);
	return ret;
}

static int __gc2093_clk_24m_power_on(struct gc2093_clk_24m *gc2093_clk_24m)
{
	int ret;
	struct device *dev = gc2093_clk_24m->dev;

	ret = clk_set_rate(gc2093_clk_24m->xvclk, GC2093_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");

	if (clk_get_rate(gc2093_clk_24m->xvclk) != GC2093_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");

	ret = clk_prepare_enable(gc2093_clk_24m->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (gc2093_clk_24m->is_thunderboot)
		return 0;

	ret = regulator_bulk_enable(GC2093_CLK_24M_NUM_SUPPLIES, gc2093_clk_24m->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(gc2093_clk_24m->reset_gpio))
		gpiod_direction_output(gc2093_clk_24m->reset_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR(gc2093_clk_24m->pwdn_gpio))
		gpiod_direction_output(gc2093_clk_24m->pwdn_gpio, 1);
	if (!IS_ERR(gc2093_clk_24m->reset_gpio))
		gpiod_direction_output(gc2093_clk_24m->reset_gpio, 0);

	usleep_range(10000, 20000);

	return 0;

disable_clk:
	clk_disable_unprepare(gc2093_clk_24m->xvclk);
	return ret;
}

static void __gc2093_clk_24m_power_off(struct gc2093_clk_24m *gc2093_clk_24m)
{
	clk_disable_unprepare(gc2093_clk_24m->xvclk);
	if (gc2093_clk_24m->is_thunderboot) {
		if (gc2093_clk_24m->is_first_streamoff) {
			gc2093_clk_24m->is_thunderboot = false;
			gc2093_clk_24m->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(gc2093_clk_24m->reset_gpio))
		gpiod_direction_output(gc2093_clk_24m->reset_gpio, 1);
	if (!IS_ERR(gc2093_clk_24m->pwdn_gpio))
		gpiod_direction_output(gc2093_clk_24m->pwdn_gpio, 0);

	regulator_bulk_disable(GC2093_CLK_24M_NUM_SUPPLIES, gc2093_clk_24m->supplies);
}

static int gc2093_clk_24m_check_sensor_id(struct gc2093_clk_24m *gc2093_clk_24m)
{
	struct device *dev = gc2093_clk_24m->dev;
	u8 id_h = 0, id_l = 0;
	u16 id = 0;
	int ret = 0;

	if (gc2093_clk_24m->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = gc2093_clk_24m_read_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_CHIP_ID_H, &id_h);
	ret |= gc2093_clk_24m_read_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_CHIP_ID_L, &id_l);
	if (ret) {
		dev_err(gc2093_clk_24m->dev, "Failed to read sensor id, (%d)\n", ret);
		return ret;
	}

	id = id_h << 8 | id_l;
	if (id != GC2093_CLK_24M_CHIP_ID) {
		dev_err(gc2093_clk_24m->dev, "sensor id: %04X mismatched\n", id);
		return -ENODEV;
	}

	dev_info(gc2093_clk_24m->dev, "Detected GC2093_CLK_24M sensor\n");
	return 0;
}

static void gc2093_clk_24m_get_module_inf(struct gc2093_clk_24m *gc2093_clk_24m,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.lens, gc2093_clk_24m->len_name, sizeof(inf->base.lens));
	strlcpy(inf->base.sensor, GC2093_CLK_24M_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, gc2093_clk_24m->module_name, sizeof(inf->base.module));
}

static int gc2093_clk_24m_get_channel_info(struct gc2093_clk_24m *gc2093_clk_24m,
				   struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = gc2093_clk_24m->cur_mode->vc[ch_info->index];
	ch_info->width = gc2093_clk_24m->cur_mode->width;
	ch_info->height = gc2093_clk_24m->cur_mode->height;
	ch_info->bus_fmt = GC2093_CLK_24M_MEDIA_BUS_FMT;
	return 0;
}

static long gc2093_clk_24m_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	struct preisp_hdrae_exp_s *hdrae_exp = arg;
	struct rkmodule_hdr_cfg *hdr_cfg;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 i, h, w;
	u32 stream = 0;
	u8 vb_h = 0, vb_l = 0;
	u16 vb = 0, cur_vts = 0, short_exp = 0, middle_exp = 0;
	u64 delay_us = 0;
	u32 fps = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		if (!gc2093_clk_24m->has_init_exp && !gc2093_clk_24m->streaming) {
			gc2093_clk_24m->init_hdrae_exp = *hdrae_exp;
			gc2093_clk_24m->has_init_exp = true;
			dev_info(gc2093_clk_24m->dev, "don't streaming, record hdrae\n");
			break;
		}

		ret = gc2093_clk_24m_set_gain(gc2093_clk_24m, hdrae_exp->short_gain_reg);
		if (ret) {
			dev_err(gc2093_clk_24m->dev, "Failed to set gain!)\n");
			return ret;
		}

		dev_dbg(gc2093_clk_24m->dev, "%s exp_reg middle: 0x%x, short: 0x%x, gain 0x%x\n",
			__func__, hdrae_exp->middle_exp_reg,
			hdrae_exp->short_exp_reg, hdrae_exp->short_gain_reg);
		// Optimize blooming effect
		if (hdrae_exp->middle_exp_reg < 0x30 || hdrae_exp->short_exp_reg < 4)
			gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x0032, 0xfd);
		else
			gc2093_clk_24m_write_reg(gc2093_clk_24m, 0x0032, 0xf8);

		/* hdr exp limit
		 * 1. max short_exp_reg  < VB
		 * 2. short_exp_reg + middle_exp_reg < framelength
		 */
		/* 30FPS sample */
//		if (hdrae_exp->middle_exp_reg > 1100)
//			hdrae_exp->middle_exp_reg = 1100;
//
//		if (hdrae_exp->short_exp_reg > 68)
//			hdrae_exp->short_exp_reg = 68;

		ret = gc2093_clk_24m_read_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_VB_H, &vb_h);
		ret |= gc2093_clk_24m_read_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_VB_L, &vb_l);
		if (ret) {
			dev_err(gc2093_clk_24m->dev, "Failed to read vb data)\n");
			return ret;
		}
		vb = vb_h << 8 | vb_l;

		/* max short exposure limit to 3 ms */
		if (hdrae_exp->short_exp_reg <= (vb - 8)) {
			short_exp = hdrae_exp->short_exp_reg;
		} else {
			short_exp = vb - 8;
			dev_err(gc2093_clk_24m->dev, "short exposure should be less than %d\n",
				vb - 8);
		}
		cur_vts = gc2093_clk_24m->cur_vts;
		dev_dbg(gc2093_clk_24m->dev, "%s cur_vts: 0x%x\n", __func__, cur_vts);

		if (short_exp + hdrae_exp->middle_exp_reg > cur_vts) {
			middle_exp = cur_vts - short_exp;
			dev_err(gc2093_clk_24m->dev, "total exposure should be less than %d\n",
				cur_vts);
		} else {
			middle_exp = hdrae_exp->middle_exp_reg;
		}
		dev_dbg(gc2093_clk_24m->dev, "%s cal exp_reg middle: 0x%x, short: 0x%x\n",
			__func__, middle_exp, short_exp);
		ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_EXP_LONG_H,
					(middle_exp >> 8) & 0x3f);
		ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_EXP_LONG_L,
					middle_exp & 0xff);
		ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_EXP_SHORT_H,
					(short_exp >> 8) & 0x3f);
		ret |= gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_EXP_SHORT_L,
					short_exp & 0xff);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		hdr_cfg->esp.mode = HDR_NORMAL_VC;
		hdr_cfg->hdr_mode = gc2093_clk_24m->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		w = gc2093_clk_24m->cur_mode->width;
		h = gc2093_clk_24m->cur_mode->height;
		for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
			if (w == supported_modes[i].width &&
			h == supported_modes[i].height &&
			supported_modes[i].hdr_mode == hdr_cfg->hdr_mode) {
				gc2093_clk_24m->cur_mode = &supported_modes[i];
				break;
			}
			dev_err(gc2093_clk_24m->dev, "i:%d,w:%d, h:%d, hdr:%d\n",
					i, supported_modes[i].width, supported_modes[i].height,
					supported_modes[i].hdr_mode);
		}
		if (i == ARRAY_SIZE(supported_modes)) {
			dev_err(gc2093_clk_24m->dev, "not find hdr mode:%d %dx%d config\n",
				hdr_cfg->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = gc2093_clk_24m->cur_mode->hts_def - gc2093_clk_24m->cur_mode->width;
			h = gc2093_clk_24m->cur_mode->vts_def - gc2093_clk_24m->cur_mode->height;
			__v4l2_ctrl_modify_range(gc2093_clk_24m->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(gc2093_clk_24m->vblank, h,
						 GC2093_CLK_24M_VTS_MAX - gc2093_clk_24m->cur_mode->height,
						 1, h);
			gc2093_clk_24m->cur_vts = gc2093_clk_24m->cur_mode->vts_def;
			gc2093_clk_24m->cur_fps = gc2093_clk_24m->cur_mode->max_fps;
			dev_info(gc2093_clk_24m->dev, "sensor mode: %d\n",
				 gc2093_clk_24m->cur_mode->hdr_mode);
		}
		break;
	case RKMODULE_GET_MODULE_INFO:
		gc2093_clk_24m_get_module_inf(gc2093_clk_24m, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream) {
			ret = gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_CTRL_MODE,
				GC2093_CLK_24M_MODE_STREAMING);
		} else {
			ret = gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_CTRL_MODE,
				GC2093_CLK_24M_MODE_SW_STANDBY);
			fps = gc2093_clk_24m->cur_mode->max_fps.denominator /
				  gc2093_clk_24m->cur_mode->max_fps.numerator;
			delay_us = 1000000 / (gc2093_clk_24m->cur_mode->vts_def * fps / gc2093_clk_24m->cur_vts);
			usleep_range(delay_us, delay_us + 2000);
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = gc2093_clk_24m_get_channel_info(gc2093_clk_24m, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static int __gc2093_clk_24m_start_stream(struct gc2093_clk_24m *gc2093_clk_24m)
{
	int ret;

	if (!gc2093_clk_24m->is_thunderboot) {
		ret = regmap_multi_reg_write(gc2093_clk_24m->regmap,
						gc2093_clk_24m->cur_mode->reg_list,
						gc2093_clk_24m->cur_mode->reg_num);
		if (ret)
			return ret;

		/* Apply customized control from user */
		mutex_unlock(&gc2093_clk_24m->lock);
		v4l2_ctrl_handler_setup(&gc2093_clk_24m->ctrl_handler);
		mutex_lock(&gc2093_clk_24m->lock);

		if (gc2093_clk_24m->has_init_exp && gc2093_clk_24m->cur_mode->hdr_mode != NO_HDR) {
			ret = gc2093_clk_24m_ioctl(&gc2093_clk_24m->subdev, PREISP_CMD_SET_HDRAE_EXP,
					&gc2093_clk_24m->init_hdrae_exp);
			if (ret) {
				dev_err(gc2093_clk_24m->dev, "init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	dev_info(gc2093_clk_24m->dev,
		 "%dx%d@%d, mode %d, vts 0x%x\n",
		 gc2093_clk_24m->cur_mode->width,
		 gc2093_clk_24m->cur_mode->height,
		 gc2093_clk_24m->cur_fps.denominator / gc2093_clk_24m->cur_fps.numerator,
		 gc2093_clk_24m->cur_mode->hdr_mode,
		 gc2093_clk_24m->cur_vts);
	dev_info(gc2093_clk_24m->dev, "is_tb:%d\n", gc2093_clk_24m->is_thunderboot);
	return gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_CTRL_MODE,
							GC2093_CLK_24M_MODE_STREAMING);
}

static int __gc2093_clk_24m_stop_stream(struct gc2093_clk_24m *gc2093_clk_24m)
{
	gc2093_clk_24m->has_init_exp = false;
	if (gc2093_clk_24m->is_thunderboot) {
		gc2093_clk_24m->is_first_streamoff = true;
		pm_runtime_put(gc2093_clk_24m->dev);
	}
	return gc2093_clk_24m_write_reg(gc2093_clk_24m, GC2093_CLK_24M_REG_CTRL_MODE,
				GC2093_CLK_24M_MODE_SW_STANDBY);
}

#ifdef CONFIG_COMPAT
static long gc2093_clk_24m_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc2093_clk_24m_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc2093_clk_24m_ioctl(sd, cmd, hdr);
		if (!ret) {
			ret = copy_to_user(up, hdr, sizeof(*hdr));
			if (ret)
				ret = -EFAULT;
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdr, up, sizeof(*hdr));
		if (!ret)
			ret = gc2093_clk_24m_ioctl(sd, cmd, hdr);
		else
			ret = -EFAULT;
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdrae, up, sizeof(*hdrae));
		if (!ret)
			ret = gc2093_clk_24m_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc2093_clk_24m_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc2093_clk_24m_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif

static int gc2093_clk_24m_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	int ret = 0;
	unsigned int fps;
	unsigned int delay_us;

	fps = DIV_ROUND_CLOSEST(gc2093_clk_24m->cur_mode->max_fps.denominator,
					gc2093_clk_24m->cur_mode->max_fps.numerator);

	dev_info(gc2093_clk_24m->dev,
		 "%dx%d@%d, mode %d, vts 0x%x\n",
		 gc2093_clk_24m->cur_mode->width,
		 gc2093_clk_24m->cur_mode->height,
		 gc2093_clk_24m->cur_fps.denominator / gc2093_clk_24m->cur_fps.numerator,
		 gc2093_clk_24m->cur_mode->hdr_mode,
		 gc2093_clk_24m->cur_vts);

	dev_info(gc2093_clk_24m->dev,
		 "stream:%d, on:%d\n",
		 gc2093_clk_24m->streaming, on);
	mutex_lock(&gc2093_clk_24m->lock);
	on = !!on;
	if (on == gc2093_clk_24m->streaming)
		goto unlock_and_return;

	if (on) {
		if (gc2093_clk_24m->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			gc2093_clk_24m->is_thunderboot = false;
			__gc2093_clk_24m_power_on(gc2093_clk_24m);
		}
		ret = pm_runtime_get_sync(gc2093_clk_24m->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(gc2093_clk_24m->dev);
			goto unlock_and_return;
		}

		ret = __gc2093_clk_24m_start_stream(gc2093_clk_24m);
		if (ret) {
			dev_err(gc2093_clk_24m->dev, "Failed to start gc2093_clk_24m stream\n");
			pm_runtime_put(gc2093_clk_24m->dev);
			goto unlock_and_return;
		}
	} else {
		__gc2093_clk_24m_stop_stream(gc2093_clk_24m);
		/* delay to enable oneframe complete */
		delay_us = 1000 * 1000 / fps;
		usleep_range(delay_us, delay_us+10);
		dev_info(gc2093_clk_24m->dev, "%s: on: %d, sleep(%dus)\n",
				__func__, on, delay_us);

		pm_runtime_put(gc2093_clk_24m->dev);
	}

	gc2093_clk_24m->streaming = on;

unlock_and_return:
	mutex_unlock(&gc2093_clk_24m->lock);
	return 0;
}

static int gc2093_clk_24m_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	const struct gc2093_clk_24m_mode *mode = gc2093_clk_24m->cur_mode;

	if (gc2093_clk_24m->streaming)
		fi->interval = gc2093_clk_24m->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static int gc2093_clk_24m_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	u32 val = 1 << (GC2093_CLK_24M_LANES - 1) | V4L2_MBUS_CSI2_CHANNEL_0 |
		  V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = (gc2093_clk_24m->cur_mode->hdr_mode == NO_HDR) ?
			val : (val | V4L2_MBUS_CSI2_CHANNEL_1);

	return 0;
}

static int gc2093_clk_24m_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = GC2093_CLK_24M_MEDIA_BUS_FMT;
	return 0;
}

static int gc2093_clk_24m_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != GC2093_CLK_24M_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;
	return 0;
}

static int gc2093_clk_24m_enum_frame_interval(struct v4l2_subdev *sd,
						  struct v4l2_subdev_pad_config *cfg,
						  struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = GC2093_CLK_24M_MEDIA_BUS_FMT;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static int gc2093_clk_24m_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	const struct gc2093_clk_24m_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc2093_clk_24m->lock);

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);

	fmt->format.code = GC2093_CLK_24M_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc2093_clk_24m->lock);
		return -ENOTTY;
#endif
	} else {
		gc2093_clk_24m->cur_mode = mode;
		__v4l2_ctrl_s_ctrl(gc2093_clk_24m->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(gc2093_clk_24m->pixel_rate,
					 to_pixel_rate(mode->link_freq_index));
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc2093_clk_24m->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc2093_clk_24m->vblank, vblank_def,
					 GC2093_CLK_24M_VTS_MAX - mode->height,
					 1, vblank_def);
		gc2093_clk_24m->cur_vts = mode->vts_def;
		gc2093_clk_24m->cur_fps = mode->max_fps;
	}

	mutex_unlock(&gc2093_clk_24m->lock);
	return 0;
}

static int gc2093_clk_24m_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	const struct gc2093_clk_24m_mode *mode = gc2093_clk_24m->cur_mode;

	mutex_lock(&gc2093_clk_24m->lock);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
#else
		mutex_unlock(&gc2093_clk_24m->lock);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = GC2093_CLK_24M_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;

		/* format info: width/height/data type/virctual channel */
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];

	}
	mutex_unlock(&gc2093_clk_24m->lock);
	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc2093_clk_24m_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
	const struct gc2093_clk_24m_mode *def_mode = &supported_modes[0];

	mutex_lock(&gc2093_clk_24m->lock);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = GC2093_CLK_24M_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;
	mutex_unlock(&gc2093_clk_24m->lock);

	return 0;
}
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc2093_clk_24m_internal_ops = {
	.open = gc2093_clk_24m_open,
};
#endif

static int gc2093_clk_24m_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);
	int ret = 0;

	mutex_lock(&gc2093_clk_24m->lock);

	if (gc2093_clk_24m->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(gc2093_clk_24m->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(gc2093_clk_24m->dev);
			goto unlock_and_return;
		}
		gc2093_clk_24m->power_on = true;
	} else {
		pm_runtime_put(gc2093_clk_24m->dev);
		gc2093_clk_24m->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc2093_clk_24m->lock);

	return ret;
}

static const struct v4l2_subdev_core_ops gc2093_clk_24m_core_ops = {
	.s_power = gc2093_clk_24m_s_power,
	.ioctl = gc2093_clk_24m_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc2093_clk_24m_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc2093_clk_24m_video_ops = {
	.s_stream = gc2093_clk_24m_s_stream,
	.g_frame_interval = gc2093_clk_24m_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc2093_clk_24m_pad_ops = {
	.enum_mbus_code = gc2093_clk_24m_enum_mbus_code,
	.enum_frame_size = gc2093_clk_24m_enum_frame_sizes,
	.enum_frame_interval = gc2093_clk_24m_enum_frame_interval,
	.get_fmt = gc2093_clk_24m_get_fmt,
	.set_fmt = gc2093_clk_24m_set_fmt,
	.get_mbus_config = gc2093_clk_24m_g_mbus_config,
};

static const struct v4l2_subdev_ops gc2093_clk_24m_subdev_ops = {
	.core   = &gc2093_clk_24m_core_ops,
	.video  = &gc2093_clk_24m_video_ops,
	.pad    = &gc2093_clk_24m_pad_ops,
};

static int __maybe_unused gc2093_clk_24m_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);

	__gc2093_clk_24m_power_on(gc2093_clk_24m);
	return 0;
}

static int __maybe_unused gc2093_clk_24m_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);

	__gc2093_clk_24m_power_off(gc2093_clk_24m);
	return 0;
}

static const struct dev_pm_ops gc2093_clk_24m_pm_ops = {
	SET_RUNTIME_PM_OPS(gc2093_clk_24m_runtime_suspend,
			   gc2093_clk_24m_runtime_resume, NULL)
};


#ifdef CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP
static void find_terminal_resolution(struct gc2093_clk_24m *gc2093_clk_24m)
{
	int i = 0;
	const struct gc2093_clk_24m_mode *mode = NULL;
	const struct gc2093_clk_24m_mode *fit_mode = NULL;
	u32 cur_fps = 0;
	u32 dst_fps = 0;
	u32 tmp_fps = 0;
	u32 rk_cam_hdr = get_rk_cam_hdr();
	u32 rk_cam_w = get_rk_cam_w();
	u32 rk_cam_h = get_rk_cam_h();
	u32 rk_cam_fps = get_rk_cam_fps();

	if (rk_cam_w == 0 || rk_cam_h == 0 ||
	    rk_cam_fps == 0)
		goto err_find_res;

	dev_info(gc2093_clk_24m->dev, "find resolution width: %d, height: %d, hdr: %d, fps: %d\n",
		 rk_cam_w, rk_cam_h, rk_cam_hdr, rk_cam_fps);
	dst_fps = rk_cam_fps;
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		mode = &supported_modes[i];
		cur_fps = mode->max_fps.denominator / mode->max_fps.numerator;
		if (mode->width == rk_cam_w && mode->height == rk_cam_h &&
		    mode->hdr_mode == rk_cam_hdr) {
			if (cur_fps == dst_fps) {
				gc2093_clk_24m->cur_mode = mode;
				return;
			}
			if (cur_fps >= dst_fps) {
				if (fit_mode) {
					tmp_fps = fit_mode->max_fps.denominator /
							  fit_mode->max_fps.numerator;
					if (tmp_fps - dst_fps > cur_fps - dst_fps)
						fit_mode = mode;
				} else {
					fit_mode = mode;
				}
			}
		}
	}
	if (fit_mode) {
		gc2093_clk_24m->cur_mode = fit_mode;
		return;
	}
err_find_res:
	dev_err(gc2093_clk_24m->dev, "not match %dx%d@%dfps mode %d\n!",
		rk_cam_w, rk_cam_h, dst_fps, rk_cam_hdr);
	gc2093_clk_24m->cur_mode = &supported_modes[0];
}
#else
static void find_terminal_resolution(struct gc2093_clk_24m *gc2093_clk_24m)
{
	u32 hdr_mode = 0;
	struct device_node *node = gc2093_clk_24m->dev->of_node;
	int i = 0;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			gc2093_clk_24m->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes))
		gc2093_clk_24m->cur_mode = &supported_modes[0];

}
#endif

static int gc2093_clk_24m_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc2093_clk_24m *gc2093_clk_24m;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	gc2093_clk_24m = devm_kzalloc(dev, sizeof(*gc2093_clk_24m), GFP_KERNEL);
	if (!gc2093_clk_24m)
		return -ENOMEM;

	gc2093_clk_24m->dev = dev;
	gc2093_clk_24m->regmap = devm_regmap_init_i2c(client, &gc2093_clk_24m_regmap_config);
	if (IS_ERR(gc2093_clk_24m->regmap)) {
		dev_err(dev, "Failed to initialize I2C\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gc2093_clk_24m->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &gc2093_clk_24m->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &gc2093_clk_24m->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &gc2093_clk_24m->len_name);
	if (ret) {
		dev_err(dev, "Failed to get module information\n");
		return -EINVAL;
	}

	gc2093_clk_24m->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	gc2093_clk_24m->xvclk = devm_clk_get(gc2093_clk_24m->dev, "xvclk");
	if (IS_ERR(gc2093_clk_24m->xvclk)) {
		dev_err(gc2093_clk_24m->dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	find_terminal_resolution(gc2093_clk_24m);

	gc2093_clk_24m->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(gc2093_clk_24m->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc2093_clk_24m->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_ASIS);
	if (IS_ERR(gc2093_clk_24m->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = gc2093_clk_24m_get_regulators(gc2093_clk_24m);
	if (ret) {
		dev_err(dev, "Failed to get regulators\n");
		return ret;
	}

	mutex_init(&gc2093_clk_24m->lock);

	sd = &gc2093_clk_24m->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc2093_clk_24m_subdev_ops);
	ret = gc2093_clk_24m_initialize_controls(gc2093_clk_24m);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc2093_clk_24m_power_on(gc2093_clk_24m);
	if (ret)
		goto err_free_handler;

	ret = gc2093_clk_24m_check_sensor_id(gc2093_clk_24m);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc2093_clk_24m_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#ifdef CONFIG_MEDIA_CONTROLLER
	gc2093_clk_24m->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc2093_clk_24m->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc2093_clk_24m->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc2093_clk_24m->module_index, facing,
		 GC2093_CLK_24M_NAME, dev_name(sd->dev));

	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 async subdev\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (gc2093_clk_24m->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#ifdef CONFIG_MEDIA_CONTROLLER
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__gc2093_clk_24m_power_off(gc2093_clk_24m);
err_free_handler:
	v4l2_ctrl_handler_free(&gc2093_clk_24m->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc2093_clk_24m->lock);

	return ret;
}

static int gc2093_clk_24m_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2093_clk_24m *gc2093_clk_24m = to_gc2093_clk_24m(sd);

	v4l2_async_unregister_subdev(sd);
#ifdef CONFIG_MEDIA_CONTROLLER
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc2093_clk_24m->ctrl_handler);
	mutex_destroy(&gc2093_clk_24m->lock);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc2093_clk_24m_power_off(gc2093_clk_24m);
	pm_runtime_set_suspended(&client->dev);
	return 0;
}

static const struct i2c_device_id gc2093_clk_24m_match_id[] = {
	{ "gc2093_clk_24m", 0 },
	{ },
};

static const struct of_device_id gc2093_clk_24m_of_match[] = {
	{ .compatible = "galaxycore,gc2093_clk_24m" },
	{},
};
MODULE_DEVICE_TABLE(of, gc2093_clk_24m_of_match);

static struct i2c_driver gc2093_clk_24m_i2c_driver = {
	.driver = {
		.name = GC2093_CLK_24M_NAME,
		.pm = &gc2093_clk_24m_pm_ops,
		.of_match_table = of_match_ptr(gc2093_clk_24m_of_match),
	},
	.probe      = &gc2093_clk_24m_probe,
	.remove     = &gc2093_clk_24m_remove,
	.id_table   = gc2093_clk_24m_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc2093_clk_24m_i2c_driver);
}
static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc2093_clk_24m_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP) && !defined(CONFIG_INITCALL_ASYNC)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Galaxycore GC2093_CLK_24M Image Sensor driver");
MODULE_LICENSE("GPL v2");
