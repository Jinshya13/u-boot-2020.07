/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 - Texas Instruments Incorporated
 *
 * Swamil Jain <s-jain1@ti.com>
 */

#include <dm.h>
#include <dm/ofnode_graph.h>
#include <dm/ofnode.h>
#include <malloc.h>
#include <syscon.h>
#include <clk.h>
#include <regmap.h>
#include <dm/device_compat.h>


#include "tidss_oldi.h"

enum tidss_oldi_pixels {
	OLDI_PIXELS_EVEN = BIT(0),
	OLDI_PIXELS_ODD = BIT(1),
};

/**
 * enum tidss_oldi_dual_link_pixels - Pixel order of an OLDI dual-link connection
 * @TIDSS_OLDI_DUAL_LINK_EVEN_ODD_PIXELS: Even pixels are expected to be generated
 *    from the first port, odd pixels from the second port
 * @TIDSS_OLDI_DUAL_LINK_ODD_EVEN_PIXELS: Odd pixels are expected to be generated
 *    from the first port, even pixels from the second port
 */
enum tidss_oldi_dual_link_pixels {
	TIDSS_OLDI_DUAL_LINK_EVEN_ODD_PIXELS = 0,
	TIDSS_OLDI_DUAL_LINK_ODD_EVEN_PIXELS = 1,
};

static const struct oldi_bus_format oldi_bus_formats[] = {
	{ MEDIA_BUS_FMT_RGB666_1X7X3_SPWG,	18, SPWG_18,	MEDIA_BUS_FMT_RGB666_1X18 },
	{ MEDIA_BUS_FMT_RGB888_1X7X4_SPWG,	24, SPWG_24,	MEDIA_BUS_FMT_RGB888_1X24 },
	{ MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA,	24, JEIDA_24,	MEDIA_BUS_FMT_RGB888_1X24 },
};

static int tidss_oldi_get_port_pixels_type(ofnode port_node)
{
	bool even_pixels =
		ofnode_has_property(port_node, "dual-lvds-even-pixels");
	bool odd_pixels =
		ofnode_has_property(port_node, "dual-lvds-odd-pixels");
	// printf("\nPixel types: %s\n", even_pixels?"even_pixels":(odd_pixels ? "odd" : "none"));
	return (even_pixels ? OLDI_PIXELS_EVEN : 0) |
	       (odd_pixels ? OLDI_PIXELS_ODD : 0);
}

static int tidss_oldi_get_remote_pixels_type(ofnode port_node)
{
	ofnode endpoint = ofnode_null();
	int pixels_type = -EPIPE;

	ofnode_for_each_subnode(endpoint, port_node) {
		ofnode remote_port;
		int current_pt;
		// printf("get_remote_pixels_type: port_name = %s\n, port_parent = %s", ofnode_get_name(port_node), ofnode_get_name(ofnode_get_parent(ofnode_get_parent(port_node))));		
		// printf("endpoint = %s\n", ofnode_get_name(endpoint));
		if (!ofnode_name_eq(endpoint, "endpoint"))
			continue;

		remote_port = ofnode_graph_get_remote_port(endpoint);
		// printf("remote_port = %s, parent node: %s\n", ofnode_get_name(remote_port), ofnode_get_name(ofnode_get_parent(ofnode_get_parent(remote_port))));
		if (!ofnode_valid(remote_port)) {
			return -EPIPE;
		}

		current_pt = tidss_oldi_get_port_pixels_type(remote_port);
		// printf("\ncurrent_pt = %d\n",current_pt);
		if (pixels_type < 0)
			pixels_type = current_pt;

		if (!current_pt || pixels_type != current_pt)
			return -EINVAL;
	}

	return pixels_type;
}


int tidss_oldi_get_dual_link_pixel_order(ofnode port1,
					  ofnode port2)
{
	int remote_p1_pt, remote_p2_pt;
	if (!ofnode_valid(port1) || !ofnode_valid(port2))
	return -EINVAL;
	
	// printf("\ntidss_oldi_get_dual_link_pixel_order: port1: %s, port2: %s\n", ofnode_get_name(port1), ofnode_get_name(port2));
	remote_p1_pt = tidss_oldi_get_remote_pixels_type(port1);
	// printf("remote_p1_pt = %d\n", remote_p1_pt);
	if (remote_p1_pt < 0)
		return remote_p1_pt;

	remote_p2_pt = tidss_oldi_get_remote_pixels_type(port2);
	// printf("remote_p2_pt = %d\n", remote_p2_pt);
	if (remote_p2_pt < 0)
		return remote_p2_pt;

	/*
	 * A valid dual-lVDS bus is found when one remote port is marked with
	 * "dual-lvds-even-pixels", and the other remote port is marked with
	 * "dual-lvds-odd-pixels", bail out if the markers are not right.
	 */
	if (remote_p1_pt + remote_p2_pt != OLDI_PIXELS_EVEN + OLDI_PIXELS_ODD)
		return -EINVAL;

	return remote_p1_pt == OLDI_PIXELS_EVEN ?
		TIDSS_OLDI_DUAL_LINK_EVEN_ODD_PIXELS :
		TIDSS_OLDI_DUAL_LINK_ODD_EVEN_PIXELS;
}

static int get_oldi_mode(ofnode oldi_tx, u32 *companion_instance)
{
	ofnode companion;
	ofnode port0, port1;
	int pixel_order;
    int ret;
    // printf("\nget_oldi_modes_api, companion instance = %d\n", *companion_instance);
	/*
	 * Find if the OLDI is paired with another OLDI for combined OLDI
	 * operation (dual-lvds or clone).
	 */
	companion = ofnode_parse_phandle(oldi_tx, "ti,companion-oldi", 0);
	// printf("\nnode name: %s\n",ofnode_get_name(companion));
	if (!ofnode_valid(companion)) {
		/*
		 * OLDI TXes in Single Link mode do not have companion
		 * OLDI TXes and, Secondary OLDI nodes don't need this
		 * information.
		 */
		*companion_instance = -1;

		if (ofnode_has_property(oldi_tx, "ti,secondary-oldi"))
			return OLDI_MODE_SECONDARY;

		/*
		 * The OLDI TX does not have a companion, nor is it a
		 * secondary OLDI. It will operate independently.
		 */
		return OLDI_MODE_SINGLE_LINK;
	}
	ret = *companion_instance;
    *companion_instance = ofnode_read_u32_default(companion, "reg", *companion_instance);
    if (*companion_instance == ret){
		// printf("\ncompanion instance = %d\n", *companion_instance);
		return OLDI_MODE_UNSUPPORTED;
	}
    
	/*
    * We need to work out if the sink is expecting us to function in
    * dual-link mode. We do this by looking at the DT port nodes we are
    * connected to, if they are marked as expecting even pixels and
    * odd pixels than we need to enable vertical stripe output.
    */
    port0 = ofnode_graph_get_port_by_id(oldi_tx, 1);
	// printf("\nport name: %s, port parent name: %s\n",ofnode_get_name(port0), ofnode_get_name(ofnode_get_parent(ofnode_get_parent(port0))));
    port1 = ofnode_graph_get_port_by_id(companion, 1);
	// printf("\nport name: %s, port parent name: %s\n",ofnode_get_name(port1), ofnode_get_name(ofnode_get_parent(ofnode_get_parent(port1))));
	pixel_order = tidss_oldi_get_dual_link_pixel_order(port0, port1);
	// printf("\npixel_order = %d\n", pixel_order);
	switch (pixel_order) {
	case -EINVAL:
		/*
		 * The dual link properties were not found in at least
		 * one of the sink nodes. Since 2 OLDI ports are present
		 * in the DT, it can be safely assumed that the required
		 * configuration is Clone Mode.
		 */
		return OLDI_MODE_CLONE_SINGLE_LINK;

	case TIDSS_OLDI_DUAL_LINK_ODD_EVEN_PIXELS:
		return OLDI_MODE_DUAL_LINK;

	/* Unsupported OLDI Modes */
	case TIDSS_OLDI_DUAL_LINK_EVEN_ODD_PIXELS:
	default:
		return OLDI_MODE_UNSUPPORTED;
	}
}

static int get_parent_dss_vp(ofnode oldi_tx, u32 *parent_vp)
{
    ofnode ep, dss_port;
    int ret = *parent_vp;

    ep = ofnode_graph_get_endpoint_by_regs(oldi_tx, 0, -1);
	// printf("\noldi_instance: %s, ep: %s\n", ofnode_get_name(oldi_tx),ofnode_get_name(ep));
    if (ofnode_valid(ep)) {
        dss_port = ofnode_graph_get_remote_port(ep);
        if (!ofnode_valid(dss_port)) {
            ret = -ENODEV;
        }

        *parent_vp = ofnode_read_u32_default(dss_port, "reg", *parent_vp);
        if(ret == *parent_vp){
            return -ENODEV;
        }
        return 0;
    }

    return -ENODEV;
}

static int tidss_init_oldi_io_ctrl(struct udevice *dev, struct tidss_oldi *tidss_oldi)
{
	struct udevice *syscon;
	struct regmap *regmap = NULL;
	int ret = 0;

	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev, "ti,am65x-oldi-io-ctrl",
					   &syscon);
	if (ret) {
		printf("\ntidss_init_oldi_io_ctrl: unable to find ti,am65x-oldi-io-ctrl syscon device (%d)\n", ret);
		debug("unable to find ti,am65x-oldi-io-ctrl syscon device (%d)\n", ret);
		return ret;
	}

	/* get grf-reg base address */
	regmap = syscon_get_regmap(syscon);
	if (!regmap) {
		debug("unable to find rockchip grf regmap\n");
		return -ENODEV;
	}
	tidss_oldi->io_ctrl = regmap;
	return 0;
}


int tidss_oldi_init(struct udevice *dev, struct tidss_oldi **tidss_oldis, int *num_oldis){
	u32 parent_vp = 6, oldi_instance = 6, companion_instance;
    ofnode child;
    ofnode ep;
    int ret, tidss_oldi_panel_count = 0;
    enum tidss_oldi_link_type link_type = OLDI_MODE_UNSUPPORTED;
    ofnode oldi_parent = ofnode_find_subnode(dev_ofnode(dev), "oldi-transmitters");
	struct tidss_oldi *tidss_oldi;
	struct clk serial;
    
    if (!ofnode_valid(oldi_parent))
		/* Return gracefully */
		return 0;
	
    ofnode_for_each_subnode(child, oldi_parent){ 
		tidss_oldis[tidss_oldi_panel_count] = NULL;
		parent_vp = 6;
		// printf("\nchild = %s\n", ofnode_get_name(child));
		ret = get_parent_dss_vp(child, &parent_vp);
		// printf("\nchild = %s, parent_dss_vp = %d\n", ofnode_get_name(child), parent_vp);
		if (ret == -ENODEV) {
			/*
            * ENODEV means that this particular OLDI node
            * is not connected with the DSS, which is not
			* a harmful case. There could be another OLDI
            * which may still be connected.
			* Continue to search for that.
            */
		   ret = 0;
		   continue;
        }
		
        ret = oldi_instance;
        oldi_instance = ofnode_read_u32_default(child, "reg", oldi_instance);
        if (ret == oldi_instance) {
			ret = -ENODEV;
            break;
        }
		companion_instance = -2;
		// printf("oldi_instance = %d, companion_instance = %d, link_type = %d\n", oldi_instance, companion_instance, link_type);
        link_type = get_oldi_mode(child, &companion_instance);
		if (link_type == OLDI_MODE_UNSUPPORTED) {
			// printf("OLDI%u: Unsupported OLDI connection.\n",
			// 	oldi_instance);
			} else if (link_type == OLDI_MODE_SECONDARY) {
				/*
				* This is the secondary OLDI node, which serves as a
				* companinon to the primary OLDI, when it is configured
				* for the dual-lvds mode. Since the primary OLDI will
				* be a part of bridge chain, no need to put this one
				* too. Continue onto the next OLDI node.
				*/
			// printf("oldi instance: %d, is a secondary/companion in dual_lvds mode\n",oldi_instance);
			continue;
		}
		// printf("\n oldi instance: %d, parent vp : %d\n",oldi_instance, parent_vp);
		tidss_oldi = malloc(sizeof(struct tidss_oldi));
		tidss_oldi->dev = dev;
		tidss_oldi->parent_vp = parent_vp;
		tidss_oldi->oldi_instance = oldi_instance;
		tidss_oldi->companion_instance = companion_instance;
		tidss_oldi->link_type = link_type;
		printf("\noldi_instance_name/child: %s\n", ofnode_get_name(child));
		ret = tidss_init_oldi_io_ctrl(dev, tidss_oldi);
		if(ret) {
			debug("Could not initialize oldi_io_ctrl\n");
			return ret;
		}
		ret = clk_get_by_name_nodev(child, "serial", &serial);
		printf("\nhas property : clock-names: %d", ofnode_has_property(child, "clock-names"));
		if (ret) {
			dev_err(dev, "video port %d clock enable error %d\n", parent_vp, ret);
			return ret;
		}
		tidss_oldi->serial = malloc(sizeof(struct clk));
		*(tidss_oldi->serial) = serial;

		tidss_oldis[tidss_oldi_panel_count] = tidss_oldi;
		// tidss_oldi->next_bridge = bridge;
		tidss_oldi_panel_count++;
    }
	*num_oldis = tidss_oldi_panel_count;
    return ret;
}
 