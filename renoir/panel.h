/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RENOIR_TOUCH_PANEL_H
#define _RENOIR_TOUCH_PANEL_H

#include <drm/drm_panel.h>
#include <linux/of.h>

static inline struct drm_panel *renoir_touch_panel(struct device_node *touch)
{
	struct device_node *node;
	struct drm_panel *panel;
	int i;

	for (i = 0; (node = of_parse_phandle(touch, "panel", i)); i++) {
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel))
			return panel;
	}
	return ERR_PTR(i ? -EPROBE_DEFER : -EINVAL);
}
#endif
