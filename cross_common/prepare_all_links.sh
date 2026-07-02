#!/bin/bash

./setup_nuttx_links.sh
./setup_apps_links.sh
./setup_rtonboot_links.sh nuttx_rtonboot_links.txt
./setup_rtonboot_links.sh uboot_rtonboot_links.txt
./setup_rtonboot_links.sh kernel_rtonboot_links.txt
./setup_rtonboot_links.sh buildroot_rtonboot_links.txt
