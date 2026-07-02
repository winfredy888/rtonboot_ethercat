# ##############################################################################
# drivers/wireless/bluetooth/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

if(CONFIG_ETHERCAT_stmmac)
  set(SRCS)
  
  list(APPEND SRCS patch.c)
  list(APPEND mempool.c)
  list(APPEND skb.c)
  list(APPEND SRCS stmmac_main.c)
  list(APPEND SRCS rkpriv.c)
  list(APPEND SRCS stmmac_mdio.c)
  list(APPEND SRCS phy_realtek.c)
  list(APPEND SRCS icosdev.c)
  ## list(APPEND SRCS dwmac-generic.c)
  list(APPEND SRCS chain_mode.c)
  list(APPEND SRCS dwmac1000_core.c)
  list(APPEND SRCS dwmac1000_dma.c)
  list(APPEND SRCS dwmac100_core.c)
  list(APPEND SRCS dwmac100_dma.c)
  list(APPEND SRCS dwmac4_core.c)
  list(APPEND SRCS dwmac4_descs.c)
  list(APPEND SRCS dwmac4_dma.c)
  list(APPEND SRCS dwmac4_lib.c)
  list(APPEND SRCS dwmac5.c)
  list(APPEND SRCS dwmac_lib.c)
  ## list(APPEND SRCS dwmac-rk-tool.c)
  list(APPEND SRCS dwxgmac2_core.c)
  list(APPEND SRCS dwxgmac2_descs.c)
  list(APPEND SRCS dwxgmac2_dma.c)
  list(APPEND SRCS enh_desc.c)
  list(APPEND SRCS hwif.c)
  list(APPEND SRCS mmc_core.c)
  list(APPEND SRCS norm_desc.c)
  list(APPEND SRCS ring_mode.c)
  ## list(APPEND SRCS stmmac_ethtool.c)
  ## list(APPEND SRCS stmmac_hwtstamp.c)
  ## list(APPEND SRCS stmmac_platform.c)
  ## list(APPEND SRCS stmmac_ptp.c)
  ## list(APPEND SRCS stmmac_tc.c)
  
  target_sources(drivers PRIVATE ${SRCS})
endif()
