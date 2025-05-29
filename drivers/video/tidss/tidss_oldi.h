/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2024 - Texas Instruments Incorporated
 *
 * Swamil Jain <s-jain1@ti.com>
 */

#ifndef __TIDSS_OLDI_H__
#define __TIDSS_OLDI_H__
 
// #include "tidss_drv.h"
#include <dm/ofnode.h>
#include <dm/of_access.h>
#include <media_bus_format.h>

#include "tidss_drv.h"

/* OLDI PORTS */
#define OLDI_INPUT_PORT    0
#define OLDI_OURPUT_PORT   1

/* Control MMR Registers */

/* Register offsets */
#define OLDI_PD_CTRL            0x100
#define OLDI_LB_CTRL            0x104

/* Power control bits */
#define OLDI_PWRDOWN_TX(n)	BIT(n)

/* LVDS Bandgap reference Enable/Disable */
#define OLDI_PWRDN_BG		BIT(8)

enum tidss_oldi_link_type {
   OLDI_MODE_UNSUPPORTED,
   OLDI_MODE_SINGLE_LINK,
   OLDI_MODE_CLONE_SINGLE_LINK,
   OLDI_MODE_DUAL_LINK,
   OLDI_MODE_SECONDARY,
};
 
struct oldi_bus_format {
   u32 bus_fmt;
   u32 data_width;
   enum oldi_mode_reg_val oldi_mode_reg_val;
   u32 input_bus_fmt;
};

struct tidss_oldi {
	struct udevice *dev;

	enum tidss_oldi_link_type link_type;
	const struct oldi_bus_format *bus_format;
	u32 oldi_instance;
	u32 companion_instance;
	u32 parent_vp;

	struct clk *serial;
	struct regmap *io_ctrl;
};

int tidss_oldi_init(struct udevice *dev);

#endif /* __TIDSS_OLDI_H__ */
 