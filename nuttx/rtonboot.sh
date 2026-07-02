#!/bin/bash
#
# Copyright (c) 2024 Winfred Young
#
# SPDX-License-Identifier: GPL-2.0
#

SRCTREE=`pwd`
SCRIPT_RTONBOOT="${SRCTREE}/loaderimage"
PATH_BIN="${SRCTREE}/nuttx.bin"
PATH_RTONBOOT="${SRCTREE}/rtonboot.img"
LOAD_ADDR=0xecc00000

function pack_rtonboot_image()
{
	rm rtonboot.img -f
	
	${SCRIPT_RTONBOOT} --pack --uboot ${PATH_BIN} ${PATH_RTONBOOT} ${LOAD_ADDR}
}

pack_rtonboot_image
date

