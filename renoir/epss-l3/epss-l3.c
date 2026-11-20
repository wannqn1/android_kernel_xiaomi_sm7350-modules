// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define EPSS_L3_VOTE		0x90
#define EPSS_L3_FREQ_LUT	0x100
#define EPSS_L3_DOMAIN_SIZE	0x1000
#define EPSS_L3_MAX_STATES	40
#define EPSS_L3_LUT_XO		BIT(30)
#define EPSS_L3_LUT_MULT	GENMASK(7, 0)

#define EPSS_CPU_VOTE(domain, cpu) \
	(EPSS_L3_VOTE + (domain) * EPSS_L3_DOMAIN_SIZE + (cpu) * sizeof(u32))

/* Keep the one-cell order from the qcom,epss-l3.h binding. */
static const struct {
	const char *name;
	u32 offset;
} epss_nodes[] = {
	{ "mas_epss_l3_apps" },
	{ "slv_epss_l3_cpu0", EPSS_CPU_VOTE(1, 0) },
	{ "slv_epss_l3_cpu1", EPSS_CPU_VOTE(1, 1) },
	{ "slv_epss_l3_cpu2", EPSS_CPU_VOTE(1, 2) },
	{ "slv_epss_l3_cpu3", EPSS_CPU_VOTE(1, 3) },
	{ "slv_epss_l3_cpu4", EPSS_CPU_VOTE(2, 0) },
	{ "slv_epss_l3_cpu5", EPSS_CPU_VOTE(2, 1) },
	{ "slv_epss_l3_cpu6", EPSS_CPU_VOTE(2, 2) },
	{ "slv_epss_l3_cpu7", EPSS_CPU_VOTE(3, 0) },
	{ "slv_epss_l3_shared", EPSS_CPU_VOTE(0, 0) },
};

struct epss_l3 {
	void __iomem *base;
	unsigned long rates[EPSS_L3_MAX_STATES];
	unsigned int num_rates;
	struct icc_provider provider;
};

static int epss_l3_get_bw(struct icc_node *node, u32 *avg, u32 *peak)
{
	/* Keep firmware's votes until a consumer explicitly votes. */
	*avg = 0;
	*peak = 0;
	return 0;
}

static int epss_l3_set(struct icc_node *src, struct icc_node *dst)
{
	struct epss_l3 *l3 = container_of(src->provider, struct epss_l3, provider);
	const u32 *offset = dst->data;
	unsigned int index;

	if (src == dst || !*offset)
		return 0;

	/* The downstream Shima devfreq consumer passes L3 rates in Hz. */
	for (index = 0; index + 1 < l3->num_rates; index++)
		if (dst->peak_bw <= l3->rates[index])
			break;

	writel_relaxed(index, l3->base + *offset);
	return 0;
}

static int epss_l3_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct icc_onecell_data *data;
	struct resource *res;
	struct epss_l3 *l3;
	unsigned long xo_rate, alt_rate, rate;
	struct clk *xo, *alternate;
	struct icc_node *node;
	unsigned int i;
	u32 entry;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res || resource_size(res) < 4 * EPSS_L3_DOMAIN_SIZE)
		return -EINVAL;

	xo = devm_clk_get(dev, "xo");
	if (IS_ERR(xo))
		return dev_err_probe(dev, PTR_ERR(xo), "failed to get xo clock\n");

	alternate = devm_clk_get(dev, "alternate");
	if (IS_ERR(alternate))
		return dev_err_probe(dev, PTR_ERR(alternate),
				     "failed to get alternate clock\n");

	xo_rate = clk_get_rate(xo);
	alt_rate = clk_get_rate(alternate) / 2;
	if (!xo_rate || !alt_rate)
		return -EINVAL;

	l3 = devm_kzalloc(dev, sizeof(*l3), GFP_KERNEL);
	if (!l3)
		return -ENOMEM;

	/* The EPSS window is shared with the CPU performance controller. */
	l3->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!l3->base)
		return -ENOMEM;

	for (i = 0; i < EPSS_L3_MAX_STATES; i++) {
		entry = readl_relaxed(l3->base + EPSS_L3_FREQ_LUT + i * sizeof(u32));
		rate = (entry & EPSS_L3_LUT_XO) ?
			xo_rate * FIELD_GET(EPSS_L3_LUT_MULT, entry) : alt_rate;
		if (!rate || (i && l3->rates[i - 1] == rate))
			break;
		l3->rates[i] = rate;
	}
	if (!i)
		return dev_err_probe(dev, -ENODEV, "empty EPSS L3 LUT\n");
	l3->num_rates = i;

	data = devm_kzalloc(dev, struct_size(data, nodes, ARRAY_SIZE(epss_nodes)),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->num_nodes = ARRAY_SIZE(epss_nodes);
	l3->provider.dev = dev;
	l3->provider.set = epss_l3_set;
	l3->provider.get_bw = epss_l3_get_bw;
	l3->provider.aggregate = icc_std_aggregate;
	l3->provider.xlate = of_icc_xlate_onecell;
	l3->provider.data = data;
	icc_provider_init(&l3->provider);

	for (i = 0; i < data->num_nodes; i++) {
		node = icc_node_create_dyn();
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto remove_nodes;
		}

		ret = icc_node_set_name(node, &l3->provider, epss_nodes[i].name);
		if (ret) {
			icc_node_destroy(node->id);
			goto remove_nodes;
		}

		node->data = (void *)&epss_nodes[i].offset;
		icc_node_add(node, &l3->provider);
		data->nodes[i] = node;
	}

	for (i = 1; i < data->num_nodes; i++) {
		ret = icc_link_nodes(data->nodes[0], &data->nodes[i]);
		if (ret)
			goto remove_nodes;
	}

	ret = icc_provider_register(&l3->provider);
	if (ret)
		goto remove_nodes;

	platform_set_drvdata(pdev, l3);
	return 0;

remove_nodes:
	icc_nodes_remove(&l3->provider);
	return ret;
}

static void epss_l3_remove(struct platform_device *pdev)
{
	struct epss_l3 *l3 = platform_get_drvdata(pdev);

	icc_provider_deregister(&l3->provider);
	icc_nodes_remove(&l3->provider);
}

static const struct of_device_id epss_l3_of_match[] = {
	{ .compatible = "qcom,lahaina-epss-l3-cpu" },
	{ }
};
MODULE_DEVICE_TABLE(of, epss_l3_of_match);

static struct platform_driver epss_l3_driver = {
	.probe = epss_l3_probe,
	.remove = epss_l3_remove,
	.driver = {
		.name = "epss-l3",
		.of_match_table = epss_l3_of_match,
		.sync_state = icc_sync_state,
	},
};
module_platform_driver(epss_l3_driver);

MODULE_DESCRIPTION("Qualcomm EPSS L3 interconnect provider");
MODULE_LICENSE("GPL");
