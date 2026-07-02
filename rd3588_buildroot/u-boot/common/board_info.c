/*
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <linux/libfdt.h>
#include <linux/compiler.h>
#include <asm/io.h>
#include <asm/string.h>

int __weak checkboard(void)
{
	return 0;
}

/*
 * If the root node of the DTB has a "model" property, show it.
 * Then call checkboard().
 */
int __weak show_board_info(void)
{
#ifdef CONFIG_OF_CONTROL
	DECLARE_GLOBAL_DATA_PTR;
	const char *model;

	model = fdt_getprop(gd->fdt_blob, 0, "model", NULL);

	if (model)
		printf("Model: %s\n", model);
#endif
	printf("MPIDR: 0x%lx\n", (ulong)read_mpidr());

#ifdef CONFIG_ARM64_BOOT_AARCH32
	if (!(gd->flags & GD_FLG_RELOC))
		printf("CPU: AArch32\n");
#endif
    if (strstr(model, "rk3588") != NULL) {
        printf("rpdzkj: pull down gpio0_b2 for init wifi\n");
            writel(0xFFFF0400, 0xFD8A0008); // direction out
            writel(0xFFFE0000, 0xFD8A0000); // output low
        }

	return checkboard();
}
