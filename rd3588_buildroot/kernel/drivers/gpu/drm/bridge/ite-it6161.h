/*
 * Separate from the ite-it6161.c file
 * */

#ifndef _ITE_IT6161_H_
#define _ITE_IT6161_H_

#include <linux/types.h>

#define DRM_HDCP_KSV_LEN 5
#define MAX_HDCP_DOWN_STREAM_COUNT 10
#define HDCP_SHA1_FIFO_LEN (MAX_HDCP_DOWN_STREAM_COUNT * 5 + 10)

struct it6161 {
    struct device *dev;
    struct drm_bridge bridge;
    struct i2c_client *i2c_mipi_rx;
    struct i2c_client *i2c_hdmi_tx;
    struct i2c_client *i2c_cec;
    struct edid *edid;
    struct drm_connector connector;
    struct mutex mode_lock;
    struct regmap *regmap_mipi_rx;
    struct regmap *regmap_hdmi_tx;
    struct regmap *regmap_cec;
    u32 it6161_addr_hdmi_tx;
    u32 it6161_addr_cec;
    struct device_node *host_node;
    struct mipi_dsi_device *dsi;
    struct completion wait_hdcp_event;
    struct completion wait_edid_complete;
    struct delayed_work hdcp_work;
    struct work_struct wait_hdcp_ksv_list;
    u8 hdmi_tx_hdcp_retry;
    /* kHz */
    u32 hdmi_tx_rclk;
    /* kHz */
    u32 hdmi_tx_pclk;
    /* kHz */
    u32 mipi_rx_mclk;
    /* kHz */
    u32 mipi_rx_rclk;
    /* kHz */
    u32 mipi_rx_pclk;
    struct drm_display_mode mipi_rx_p_display_mode;
    struct drm_display_mode hdmi_tx_display_mode;
    struct drm_display_mode source_display_mode;
    struct hdmi_avi_infoframe source_avi_infoframe;
    u32 vic;
    u8 mipi_rx_lane_count;
    bool is_repeater;
    u8 hdcp_downstream_count;
    u8 bksv[DRM_HDCP_KSV_LEN];
    u8 sha1_transform_input[HDCP_SHA1_FIFO_LEN];
    u16 bstatus;
    bool enable_drv_hold;
    u8 hdmi_tx_output_color_space;
    u8 hdmi_tx_input_color_space;
    u8 hdmi_tx_mode;
    u8 support_audio;

    u8 bOutputAudioMode;
    u8 bAudioChannelSwap;
    u8 bAudioChannelEnable;
    u8 bAudFs;
    u32 TMDSClock;
    u32 RCLK;
#ifdef _SUPPORT_HDCP_REPEATER_
    HDMITX_HDCP_State TxHDCP_State;
    u16 usHDCPTimeOut;
    u16 Tx_BStatus;
#endif
    u8 bAuthenticated : 1;
    bool hdmi_mode;
    u8 bAudInterface;

    struct gpio_desc *reset_gpio;
    struct gpio_desc *enable_gpio;
    struct gpio_desc *test_gpio;

    struct timer_list timer;
    struct delayed_work restart;
};

#endif /* _ITE_IT6161_H_  */
