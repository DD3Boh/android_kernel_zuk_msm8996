/*
 * Copyright (c) 2015-2019 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * DOC: wma_hdd_ocb.c
 *
 * WLAN Host Device Driver 802.11p OCB implementation
 */

#include "vos_sched.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_ocb.h"
#include "wlan_hdd_trace.h"
#include "wlan_tgt_def_config.h"
#include "schApi.h"
#include "wma.h"
#include "vos_types.h"
#include "wlan_hdd_request_manager.h"

/* Structure definitions for WLAN_SET_DOT11P_CHANNEL_SCHED */
#define AIFSN_MIN		(2)
#define AIFSN_MAX		(15)
#define CW_MIN			(1)
#define CW_MAX			(10)

/* Maximum time(ms) to wait for OCB operations */
#define WLAN_WAIT_TIME_OCB_CMD 1500
#define HDD_OCB_MAGIC 0x489a154f

/**
 * hdd_set_dot11p_config() - Set 802.11p config flag
 * @hdd_ctx: HDD Context pointer
 *
 * TODO-OCB: This has been temporarily added to ensure this paramter
 * is set in CSR when we init the channel list. This should be removed
 * once the 5.9 GHz channels are added to the regulatory domain.
 */
void hdd_set_dot11p_config(hdd_context_t *hdd_ctx)
{
	sme_set_dot11p_config(hdd_ctx->hHal,
			      hdd_ctx->cfg_ini->dot11p_mode !=
				WLAN_HDD_11P_DISABLED);
}

/**
 * dot11p_validate_qos_params() - Check if QoS parameters are valid
 * @qos_params:   Array of QoS parameters
 *
 * Return: 0 on success. error code on failure.
 */
static int dot11p_validate_qos_params(struct sir_qos_params qos_params[])
{
	int i;

	for (i = 0; i < MAX_NUM_AC; i++) {
		if ((!qos_params[i].aifsn) && (!qos_params[i].cwmin)
				&& (!qos_params[i].cwmax))
			continue;

		/* Validate AIFSN */
		if ((qos_params[i].aifsn < AIFSN_MIN)
				|| (qos_params[i].aifsn > AIFSN_MAX)) {
			hddLog(LOGE, FL("Invalid QoS parameter aifsn %d"),
				qos_params[i].aifsn);
			return -EINVAL;
		}

		/* Validate CWMin */
		if ((qos_params[i].cwmin < CW_MIN)
				|| (qos_params[i].cwmin > CW_MAX)) {
			hddLog(LOGE, FL("Invalid QoS parameter cwmin %d"),
				qos_params[i].cwmin);
			return -EINVAL;
		}

		/* Validate CWMax */
		if ((qos_params[i].cwmax < CW_MIN)
				|| (qos_params[i].cwmax > CW_MAX)) {
			hddLog(LOGE, FL("Invalid QoS parameter cwmax %d"),
				qos_params[i].cwmax);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef FEATURE_STATICALLY_ADD_11P_CHANNELS

#define DOT11P_TX_PWR_MAX	30
#define DOT11P_TX_ANTENNA_MAX	6
#define NUM_DOT11P_CHANNELS	ARRAY_SIZE(valid_dot11p_channels)
/*
 * If FEATURE_STATICALLY_ADD_11P_CHANNELS
 * is defined, IEEE80211_CHAN_NO_10MHZ,
 * and IEEE80211_CHAN_NO_20MHZ won't
 * be defined.
 */
#define IEEE80211_CHAN_NO_20MHZ	(1<<11)
#define IEEE80211_CHAN_NO_10MHZ	(1<<12)

/**
 * struct chan_info - information for the channel
 * @center_freq: center frequency
 * @max_bandwidth: maximum bandwidth of the channel in MHz
 */
struct chan_info {
	uint32_t center_freq;
	uint32_t max_bandwidth;
};

struct chan_info valid_dot11p_channels[] = {
	{5860, 10},
	{5870, 10},
	{5880, 10},
	{5890, 10},
	{5900, 10},
	{5910, 10},
	{5920, 10},
	{5875, 20},
	{5905, 20},
	{5852, 5},
	{5857, 5},
	{5862, 5},
	{5867, 5},
	{5872, 5},
	{5877, 5},
	{5882, 5},
	{5887, 5},
	{5892, 5},
	{5897, 5},
	{5902, 5},
	{5907, 5},
	{5912, 5},
	{5917, 5},
	{5922, 5},
};

/**
 * dot11p_validate_channel_static_channels() - validate a DSRC channel
 * @center_freq: the channel's center frequency
 * @bandwidth: the channel's bandwidth
 * @tx_power: transmit power
 * @reg_power: (output) the max tx power from the regulatory domain
 * @antenna_max: (output) the max antenna gain from the regulatory domain
 *
 * This function of the function checks the channel parameters against a
 * hardcoded list of valid channels based on the FCC rules.
 *
 * Return: 0 if the channel is valid, error code otherwise.
 */
static int dot11p_validate_channel_static_channels(struct wiphy *wiphy,
	uint32_t channel_freq, uint32_t bandwidth, uint32_t tx_power,
	uint8_t *reg_power, uint8_t *antenna_max)
{
	int i;

	for (i = 0; i < NUM_DOT11P_CHANNELS; i++) {
		if (channel_freq == valid_dot11p_channels[i].center_freq) {
			if (reg_power)
				*reg_power = DOT11P_TX_PWR_MAX;
			if (antenna_max)
				*antenna_max = DOT11P_TX_ANTENNA_MAX;

			if (bandwidth == 0)
				bandwidth =
					valid_dot11p_channels[i].max_bandwidth;
			else if (bandwidth >
				 valid_dot11p_channels[i].max_bandwidth)
				return -EINVAL;

			if (bandwidth != 5 && bandwidth != 10 &&
			    bandwidth != 20)
				return -EINVAL;
			if (tx_power > DOT11P_TX_PWR_MAX)
				return -EINVAL;

			return 0;
		}
	}

	return -EINVAL;
}

static int dot11p_validate_channel(struct wiphy *wiphy,
				   uint32_t channel_freq, uint32_t bandwidth,
				   uint32_t tx_power, uint8_t *reg_power,
				   uint8_t *antenna_max)
{
	return dot11p_validate_channel_static_channels(wiphy, channel_freq,
		bandwidth, tx_power, reg_power, antenna_max);
}
#else
/**
 * dot11p_validate_channel() - validates a DSRC channel
 * @center_freq: the channel's center frequency
 * @bandwidth: the channel's bandwidth
 * @tx_power: transmit power
 * @reg_power: (output) the max tx power from the regulatory domain
 * @antenna_max: (output) the max antenna gain from the regulatory domain
 *
 * Return: 0 if the channel is valid, error code otherwise.
 */
static int dot11p_validate_channel(struct wiphy *wiphy,
				   uint32_t channel_freq, uint32_t bandwidth,
				   uint32_t tx_power, uint8_t *reg_power,
				   uint8_t *antenna_max)
{
	int band_idx, channel_idx;
	struct ieee80211_supported_band *current_band;
	struct ieee80211_channel *current_channel;

	for (band_idx = 0; band_idx < IEEE80211_NUM_BANDS; band_idx++) {
		current_band = wiphy->bands[band_idx];
		if (!current_band)
			continue;

		for (channel_idx = 0; channel_idx < current_band->n_channels;
		      channel_idx++) {
			current_channel = &current_band->channels[channel_idx];

			if (channel_freq == current_channel->center_freq) {
				if (current_channel->flags &
				    IEEE80211_CHAN_DISABLED)
					return -EINVAL;

				if (reg_power)
					*reg_power =
					    current_channel->max_reg_power;
				if (antenna_max)
					*antenna_max =
					    current_channel->max_antenna_gain;

				switch (bandwidth) {
				case 0:
					if (current_channel->flags &
					    IEEE80211_CHAN_NO_10MHZ)
						bandwidth = 5;
					else if (current_channel->flags &
						 IEEE80211_CHAN_NO_20MHZ)
						bandwidth = 10;
					else
						bandwidth = 20;
					break;
				case 5:
					break;
				case 10:
					if (current_channel->flags &
					    IEEE80211_CHAN_NO_10MHZ)
						return -EINVAL;
					break;
				case 20:
					if (current_channel->flags &
					    IEEE80211_CHAN_NO_20MHZ)
						return -EINVAL;
					break;
				default:
					return -EINVAL;
				}

				if (tx_power > current_channel->max_power)
					return -EINVAL;

				return 0;
			}
		}
	}

	return -EINVAL;
}
#endif

/**
 * hdd_ocb_validate_config() - Validates the config data
 * @config: configuration to be validated
 *
 * Return: 0 on success.
 */
static int hdd_ocb_validate_config(hdd_adapter_t *adapter,
				   struct sir_ocb_config *config)
{
	int i;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	for (i = 0; i < config->channel_count; i++) {
		if (dot11p_validate_channel(hdd_ctx->wiphy,
					    config->channels[i].chan_freq,
					    config->channels[i].bandwidth,
					    config->channels[i].max_pwr,
					    &config->channels[i].reg_pwr,
					    &config->channels[i].antenna_max)) {
			hddLog(LOGE, FL("Invalid channel frequency %d"),
				config->channels[i].chan_freq);
			return -EINVAL;
		}
		if (dot11p_validate_qos_params(config->channels[i].qos_params))
			return -EINVAL;
	}

	return 0;
}

/**
 * hdd_ocb_register_sta() - Register station with Transport Layer
 * @adapter: Pointer to HDD Adapter
 *
 * This function should be invoked in the OCB Set Schedule callback
 * to enable the data path in the TL by calling RegisterSTAClient
 *
 * Return: 0 on success. -1 on failure.
 */
static int hdd_ocb_register_sta(hdd_adapter_t *adapter)
{
	VOS_STATUS vos_status = VOS_STATUS_E_FAILURE;
	WLAN_STADescType sta_desc = {0};
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	u_int8_t peer_id;
	v_MACADDR_t wildcardBSSID = {
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	};

	vos_status = WLANTL_RegisterOCBPeer(hdd_ctx->pvosContext,
					    adapter->macAddressCurrent.bytes,
					    &peer_id);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		hddLog(LOGE, FL("Error registering OCB Self Peer!"));
		return -EINVAL;
	}

	hdd_ctx->sta_to_adapter[peer_id] = adapter;

	sta_desc.ucSTAId = peer_id;
	/* Fill in MAC addresses */
	vos_copy_macaddr(&sta_desc.vSelfMACAddress,
			 &adapter->macAddressCurrent);
	vos_copy_macaddr(&sta_desc.vSTAMACAddress, &adapter->macAddressCurrent);
	vos_copy_macaddr(&sta_desc.vBSSIDforIBSS, &wildcardBSSID);

	sta_desc.wSTAType = WLAN_STA_OCB;
	sta_desc.ucQosEnabled = 1;
	sta_desc.ucInitState = WLANTL_STA_AUTHENTICATED;

	vos_status = WLANTL_RegisterSTAClient(hdd_ctx->pvosContext,
					      hdd_rx_packet_cbk,
					      &sta_desc,
					      0);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		hddLog(LOGE, FL("Failed to register STA client. ret=[0x%08X]"),
		       vos_status);
		return -EINVAL;
	}

	if (pHddStaCtx->conn_info.staId[0] != 0 &&
	    pHddStaCtx->conn_info.staId[0] != peer_id) {
		hddLog(LOGE, FL("The ID for the OCB station has changed."));
	}

	pHddStaCtx->conn_info.staId[0] = peer_id;
	vos_copy_macaddr(&pHddStaCtx->conn_info.peerMacAddress[0],
			 &adapter->macAddressCurrent);

	return 0;
}

/**
 * hdd_ocb_config_new() - Creates a new OCB configuration
 * @num_channels: the number of channels
 * @num_schedule: the schedule size
 * @ndl_chan_list_len: length in bytes of the NDL chan blob
 * @ndl_active_state_list_len: length in bytes of the active state blob
 *
 * Return: A pointer to the OCB configuration struct, NULL on failure.
 */
static
struct sir_ocb_config *hdd_ocb_config_new(uint32_t num_channels,
					  uint32_t num_schedule,
					  uint32_t ndl_chan_list_len,
					  uint32_t ndl_active_state_list_len)
{
	struct sir_ocb_config *ret = 0;
	size_t len;
	void *cursor;

	if (num_channels > CFG_TGT_NUM_OCB_CHANNELS ||
			num_schedule > CFG_TGT_NUM_OCB_SCHEDULES)
		return NULL;

	len = sizeof(*ret) +
		num_channels * sizeof(struct sir_ocb_config_channel) +
		num_schedule * sizeof(struct sir_ocb_config_sched) +
		ndl_chan_list_len +
		ndl_active_state_list_len;

	cursor = vos_mem_malloc(len);
	if (!cursor)
		goto fail;

	vos_mem_zero(cursor, len);
	ret = cursor;
	cursor += sizeof(*ret);

	ret->channel_count = num_channels;
	ret->channels = cursor;
	cursor += num_channels * sizeof(*ret->channels);

	ret->schedule_size = num_schedule;
	ret->schedule = cursor;
	cursor += num_schedule * sizeof(*ret->schedule);

	ret->dcc_ndl_chan_list = cursor;
	cursor += ndl_chan_list_len;

	ret->dcc_ndl_active_state_list = cursor;
	cursor += ndl_active_state_list_len;

	return ret;

fail:
	vos_mem_free(ret);
	return NULL;
}

/**
 * hdd_ocb_set_config_priv - private parameter for ocb set config
 * @status: status of set config
 */
struct hdd_ocb_set_config_priv {
	int status;
};

/**
 * enum ocb_channel_config_status - ocb config status
 * @OCB_CHANNEL_CONFIG_SUCCESS: success
 * @OCB_CHANNEL_CONFIG_FAIL: failure
 * @OCB_CHANNEL_CONFIG_STATUS_MAX: place holder, not a real status
 */
enum ocb_channel_config_status {
	OCB_CHANNEL_CONFIG_SUCCESS = 0,
	OCB_CHANNEL_CONFIG_FAIL,
	OCB_CHANNEL_CONFIG_STATUS_MAX
};

/**
 * hdd_ocb_set_config_callback() - OCB set config callback function
 * @context_ptr: OCB call context
 * @response_ptr: Pointer to response structure
 *
 * This function is registered as a callback with the lower layers
 * and is used to respond with the status of a OCB set config command.
 */
static void hdd_ocb_set_config_callback(void *context_ptr, void *response_ptr)
{
	struct hdd_request *hdd_request;
	struct hdd_ocb_set_config_priv *priv;
	struct sir_ocb_set_config_response *response = response_ptr;

	hdd_request = hdd_request_get(context_ptr);
	if (!hdd_request) {
		hddLog(LOGE, FL("Obsolete request"));
		return;
	}
	priv = hdd_request_priv(hdd_request);

	if (response && (response->status != OCB_CHANNEL_CONFIG_SUCCESS))
		hddLog(LOGE, FL("Operation failed: %d"), response->status);

	if (response && (response->status == OCB_CHANNEL_CONFIG_SUCCESS))
		priv->status = 0;
	else
		priv->status = -EINVAL;

	hdd_request_complete(hdd_request);
	hdd_request_put(hdd_request);
}

/**
 * hdd_ocb_set_config_req() - Send an OCB set config request
 * @adapter: a pointer to the adapter
 * @config: a pointer to the OCB configuration
 *
 * Return: 0 on success.
 */
static int hdd_ocb_set_config_req(hdd_adapter_t *adapter,
				  struct sir_ocb_config *config)
{
	int i, rc;
	eHalStatus halStatus;
	bool enable_chan_stats;
	struct dsrc_radio_chan_stats_ctxt *ctx;
	void *cookie;
	struct hdd_request *hdd_request;
	struct hdd_ocb_set_config_priv *priv;
	static const struct hdd_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_OCB_CMD,
	};

	if (hdd_ocb_validate_config(adapter, config)) {
		hddLog(LOGE, FL("The configuration is invalid"));
		return -EINVAL;
	}

	/*
	 * Save OCB configured channel information for
	 * DSRC Radio channel statistics event processsor.
	 */
	ctx = &adapter->dsrc_chan_stats;
	ctx->config_chans_num = config->channel_count;
	for (i = 0; i < config->channel_count; i++)
		ctx->config_chans_freq[i] = config->channels[i].chan_freq;
	if (ctx->cur_req) {
		vos_mem_free(ctx->cur_req);
		ctx->cur_req = NULL;
	}
	/* Disable Channel Statistics */
	enable_chan_stats = ctx->enable_chan_stats;
	if (enable_chan_stats)
		wlan_hdd_dsrc_config_radio_chan_stats(adapter, false);

	hdd_request = hdd_request_alloc(&params);
	if (!hdd_request) {
		hddLog(LOGE, FL("Request allocation failure"));
		return -ENOMEM;
	}
	cookie = hdd_request_cookie(hdd_request);

	hddLog(LOG1, FL("Disabling queues"));
	wlan_hdd_netif_queue_control(adapter, WLAN_NETIF_TX_DISABLE_N_CARRIER,
				     WLAN_CONTROL_PATH);
	/* Call the SME API to set the config */
	halStatus = sme_ocb_set_config(
		((hdd_context_t *)adapter->pHddCtx)->hHal, cookie,
		hdd_ocb_set_config_callback, config);
	if (halStatus != eHAL_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error calling SME function."));
		/* Convert from eHalStatus to errno */
		rc = -EINVAL;
		goto end;
	}

	/* Wait for the function to complete. */
	rc = hdd_request_wait_for_response(hdd_request);
	if (rc) {
		hddLog(LOGE, FL("Operation timed out"));
		goto end;
	}

	priv = hdd_request_priv(hdd_request);
	rc = priv->status;
	if (rc) {
		hddLog(LOGE, FL("Operation failed: %d"), rc);
		goto end;
	}

	/*
	 * OCB set config command successful.
	 * Open the TX data path
	 */
	if (!hdd_ocb_register_sta(adapter))
		wlan_hdd_netif_queue_control(adapter,
					WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);
	if (enable_chan_stats)
		wlan_hdd_dsrc_config_radio_chan_stats(adapter, true);

	/*
	* Net device mtu size is 1500 by default, But for OCB RAW mode,
	* driver need later convert 802.3 data header to IEEE802.11
	* data header and EPD header, which will increase total frame
	* length. In such case, long packet length will exceed the
	* target credit size. It resulted in that the packet is cut
	* down, data would be missed and the traffic would be broken.
	* So decrease the netdev mtu size to work around this issue
	* in IEEE80211p RAW mode.
	*/
	if (config->flags & OCB_CONFIG_FLAG_80211_FRAME_MODE)
		adapter->dev->mtu = ETH_DATA_LEN - 8;
	else
		adapter->dev->mtu = ETH_DATA_LEN;

	/* fall through */
end:
	if (rc) {
		/* Flush already saved configured channel frequence */
		ctx->config_chans_num = 0;
		vos_mem_zero(ctx->config_chans_freq, 2 * sizeof(uint32_t));
	}

	hdd_request_put(hdd_request);
	return rc;
}

/**
 * __iw_set_dot11p_channel_sched() - Handler for WLAN_SET_DOT11P_CHANNEL_SCHED
 *				     ioctl
 * @dev: Pointer to net_device structure
 * @iw_request_info: IW Request Info
 * @wrqu: IW Request Userspace Data Pointer
 * @extra: IW Request Kernel Data Pointer
 *
 * Return: 0 on success
 */
static int __iw_set_dot11p_channel_sched(struct net_device *dev,
					 struct iw_request_info *info,
					 union iwreq_data *wrqu, char *extra)
{
	int rc = 0;
	struct dot11p_channel_sched *sched;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct sir_ocb_config *config = NULL;
	uint8_t *mac_addr;
	int i, j;

	if (wlan_hdd_validate_context(WLAN_HDD_GET_CTX(adapter)))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	sched = (struct dot11p_channel_sched *)extra;

	/* Scheduled slots same as num channels for compatibility */
	config = hdd_ocb_config_new(sched->num_channels, sched->num_channels,
				    0, 0);
	if (config == NULL) {
		hddLog(LOGE, FL("Failed to allocate memory!"));
		return -ENOMEM;
	}

	/* Identify the vdev interface */
	config->session_id = adapter->sessionId;

	/* Release all the mac addresses used for OCB */
	for (i = 0; i < adapter->ocb_mac_addr_count; i++) {
		wlan_hdd_release_intf_addr(adapter->pHddCtx,
					   adapter->ocb_mac_address[i]);
	}
	adapter->ocb_mac_addr_count = 0;

	config->channel_count = 0;
	for (i = 0; i < sched->num_channels; i++) {
		if (0 == sched->channels[i].channel_freq) {
			continue;
		}

		config->channels[config->channel_count].chan_freq =
			sched->channels[i].channel_freq;
		/*
		 * tx_power is divided by 2 because ocb_channel.tx_power is
		 * in half dB increments and sir_ocb_config_channel.max_pwr
		 * is in 1 dB increments.
		 */
		config->channels[config->channel_count].max_pwr =
			sched->channels[i].tx_power / 2;
		config->channels[config->channel_count].bandwidth =
			sched->channels[i].channel_bandwidth;
		/* assume 10 as default if not provided */
		if (config->channels[config->channel_count].bandwidth == 0) {
			config->channels[config->channel_count].bandwidth = 10;
		}

		/*
		 * Setup locally administered mac addresses for each channel.
		 * First channel uses the adapter's address.
		 */
		if (i == 0) {
			vos_mem_copy(config->channels[config->channel_count].mac_address,
				     adapter->macAddressCurrent.bytes,
				     sizeof(tSirMacAddr));
		} else {
			mac_addr = wlan_hdd_get_intf_addr(adapter->pHddCtx);
			if (mac_addr == NULL) {
				hddLog(LOGE, FL("Cannot obtain mac address"));
				rc = -EINVAL;
				goto fail;
			}
			vos_mem_copy(config->channels[
				     config->channel_count].mac_address,
				     mac_addr, sizeof(tSirMacAddr));
			/* Save the mac address to release later */
			vos_mem_copy(adapter->ocb_mac_address[
				     adapter->ocb_mac_addr_count],
				     mac_addr,
				     sizeof(adapter->ocb_mac_address[
				     adapter->ocb_mac_addr_count]));
			adapter->ocb_mac_addr_count++;
		}

		for (j = 0; j < MAX_NUM_AC; j++) {
			config->channels[config->channel_count].qos_params[j].aifsn =
				sched->channels[i].qos_params[j].aifsn;
			config->channels[config->channel_count].qos_params[j].cwmin =
				sched->channels[i].qos_params[j].cwmin;
			config->channels[config->channel_count].qos_params[j].cwmax =
				sched->channels[i].qos_params[j].cwmax;
		}

		config->channel_count++;
	}

	/*
	 * Scheduled slots same as num channels for compatibility with
	 * legacy use.
	 */
	for (i = 0; i < sched->num_channels; i++) {
		config->schedule[i].chan_freq = sched->channels[i].channel_freq;
		config->schedule[i].guard_interval =
			sched->channels[i].start_guard_interval;
		config->schedule[i].total_duration =
			sched->channels[i].duration;
	}

	rc = hdd_ocb_set_config_req(adapter, config);
	if (rc) {
		hddLog(LOGE, FL("Error while setting OCB config"));
		goto fail;
	}

	rc = 0;

fail:
	vos_mem_free(config);
	return rc;
}

/**
 * iw_set_dot11p_channel_sched() - IOCTL interface for setting channel schedule
 * @dev: Pointer to net_device structure
 * @iw_request_info: IW Request Info
 * @wrqu: IW Request Userspace Data Pointer
 * @extra: IW Request Kernel Data Pointer
 *
 * Return: 0 on success.
 */
int iw_set_dot11p_channel_sched(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_set_dot11p_channel_sched(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

static const struct nla_policy qca_wlan_vendor_ocb_set_config_policy[
		QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_ARRAY] = {
		.type = NLA_BINARY
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_ARRAY] = {
		.type = NLA_BINARY
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_CHANNEL_ARRAY] = {
		.type = NLA_BINARY
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_ACTIVE_STATE_ARRAY] = {
		.type = NLA_BINARY
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_DEF_TX_PARAM] = {
		.type = NLA_BINARY
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_TA_MAX_DURATION] = {
		.type = NLA_U32
	},
};

static const struct nla_policy qca_wlan_vendor_ocb_set_utc_time_policy[
		QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_VALUE] = {
		.type = NLA_BINARY, .len = SIZE_UTC_TIME
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_ERROR] = {
		.type = NLA_BINARY, .len = SIZE_UTC_TIME_ERROR
	},
};

static const struct nla_policy qca_wlan_vendor_ocb_start_timing_advert_policy[
		QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE] = {
		.type = NLA_U32
	},
};

static const struct nla_policy  qca_wlan_vendor_ocb_stop_timing_advert_policy[
		QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ] = {
		.type = NLA_U32
	},
};

static const struct nla_policy qca_wlan_vendor_ocb_get_tsf_timer_resp[] = {
	[QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_HIGH] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_LOW] = {
		.type = NLA_U32
	},
};

static const struct nla_policy qca_wlan_vendor_dcc_get_stats[] = {
	[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_CHANNEL_COUNT] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_REQUEST_ARRAY] = {
		.type = NLA_BINARY
	},
};

static const struct nla_policy qca_wlan_vendor_dcc_get_stats_resp[] = {
	[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_CHANNEL_COUNT] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_STATS_ARRAY] = {
		.type = NLA_BINARY
	},
};

static const struct nla_policy qca_wlan_vendor_dcc_clear_stats[] = {
	[QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_BITMAP] = {
		.type = NLA_U32
	},
};

static const struct nla_policy qca_wlan_vendor_dcc_update_ndl[
		QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_COUNT] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_ARRAY] = {
		.type = NLA_BINARY
	},
	[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_ACTIVE_STATE_ARRAY] = {
		.type = NLA_BINARY
	},
};

/**
 * struct wlan_hdd_ocb_config_channel
 * @chan_freq: frequency of the channel
 * @bandwidth: bandwidth of the channel, either 10 or 20 MHz
 * @mac_address: MAC address assigned to this channel
 * @qos_params: QoS parameters
 * @max_pwr: maximum transmit power of the channel (1/2 dBm)
 * @min_pwr: minimum transmit power of the channel (1/2 dBm)
 */
struct wlan_hdd_ocb_config_channel {
	uint32_t chan_freq;
	uint32_t bandwidth;
	uint16_t flags;
	uint8_t reserved[4];
	sir_qos_params_t qos_params[MAX_NUM_AC];
	uint32_t max_pwr;
	uint32_t min_pwr;
	uint32_t datarate;
	uint8_t mac_address[6];
};

static void wlan_hdd_ocb_config_channel_to_sir_ocb_config_channel(
    struct sir_ocb_config_channel *dest,
    struct wlan_hdd_ocb_config_channel *src,
    uint32_t channel_count)
{
	uint32_t i;

	vos_mem_zero(dest, channel_count * sizeof(*dest));

	for (i = 0; i < channel_count; i++) {
		dest[i].chan_freq = src[i].chan_freq;
		dest[i].bandwidth = src[i].bandwidth;
		vos_mem_copy(dest[i].qos_params, src[i].qos_params,
			     sizeof(dest[i].qos_params));
		/*
		 *  max_pwr and min_pwr are divided by 2 because
		 *  wlan_hdd_ocb_config_channel.max_pwr and min_pwr
		 *  are in 1/2 dB increments and
		 *  sir_ocb_config_channel.max_pwr and min_pwr are in
		 *  1 dB increments.
		 */
		dest[i].max_pwr = src[i].max_pwr / 2;
		dest[i].min_pwr = (src[i].min_pwr + 1) / 2;
		dest[i].flags = src[i].flags;
		vos_mem_copy(dest[i].mac_address, src[i].mac_address, 6);
	}
}

/**
 * __wlan_hdd_cfg80211_ocb_set_config() - Interface for set config command
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int __wlan_hdd_cfg80211_ocb_set_config(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX + 1];
	struct nlattr *channel_array;
	struct nlattr *sched_array;
	struct nlattr *ndl_chan_list;
	uint32_t ndl_chan_list_len;
	struct nlattr *ndl_active_state_list;
	uint32_t ndl_active_state_list_len;
	uint32_t flags = 0;
	uint32_t ta_max_duration = 0;
	void *def_tx_param = NULL;
	uint32_t def_tx_param_size = 0;
	int i;
	uint32_t channel_count, schedule_size;
	struct sir_ocb_config *config;
	int rc = -EINVAL;
	uint8_t *mac_addr;

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX,
			data,
			data_len, qca_wlan_vendor_ocb_set_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Get the number of channels in the schedule */
	if (!tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT]) {
		hddLog(LOGE, FL("CHANNEL_COUNT is not present"));
		return -EINVAL;
	}
	channel_count = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT]);

	/* Get the size of the channel schedule */
	if (!tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE]) {
		hddLog(LOGE, FL("SCHEDULE_SIZE is not present"));
		return -EINVAL;
	}
	schedule_size = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE]);

	/* Get the ndl chan array and the ndl active state array. */
	ndl_chan_list =
		tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_CHANNEL_ARRAY];
	ndl_chan_list_len = (ndl_chan_list ? nla_len(ndl_chan_list) : 0);

	ndl_active_state_list =
		tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_ACTIVE_STATE_ARRAY];
	ndl_active_state_list_len = (ndl_active_state_list ?
				    nla_len(ndl_active_state_list) : 0);

	/* Get the flags. This parameter is optional. */
	if (tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS])
		flags = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS]);

	/* Get the default TX parameters. This parameter is optional. */
	if (tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_DEF_TX_PARAM]) {
		def_tx_param_size = nla_len(tb[
			QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_DEF_TX_PARAM]);
		def_tx_param = nla_data(tb[
			QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_DEF_TX_PARAM]);
	}

	/* Get the ta max duration. This parameter is optional. */
	if (tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_TA_MAX_DURATION])
		ta_max_duration = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_TA_MAX_DURATION]);

	config = hdd_ocb_config_new(channel_count, schedule_size,
				    ndl_chan_list_len,
				    ndl_active_state_list_len);
	if (config == NULL) {
		hddLog(LOGE, FL("Failed to allocate memory!"));
		return -ENOMEM;
	}

	config->channel_count = channel_count;
	config->schedule_size = schedule_size;
	config->flags = flags;
	/*
	 * Set max duration after the last TA received that the local time set
	 * by TA is synchronous to other communicating OCB STAs. If it expires,
	 * the OCB STA itself without UTC time source like GPS thinks that the
	 * local time of itself is not sync to other STAs and stop scheduling
	 * DSRC channel switch.
	 */
	config->ta_max_duration = ta_max_duration;
	config->def_tx_param = def_tx_param;
	config->def_tx_param_size = def_tx_param_size;

	/* Read the channel array */
	channel_array = tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_ARRAY];
	if (!channel_array) {
		hddLog(LOGE, FL("No channel present"));
		goto fail;
	}
	if (nla_len(channel_array) != channel_count *
	    sizeof(struct wlan_hdd_ocb_config_channel)) {
		hddLog(LOGE, FL("CHANNEL_ARRAY is not the correct size"));
		goto fail;
	}
	wlan_hdd_ocb_config_channel_to_sir_ocb_config_channel(
	    config->channels, nla_data(channel_array), channel_count);

	/* Identify the vdev interface */
	config->session_id = adapter->sessionId;

	/* Release all the mac addresses used for OCB */
	for (i = 0; i < adapter->ocb_mac_addr_count; i++) {
		wlan_hdd_release_intf_addr(adapter->pHddCtx,
					   adapter->ocb_mac_address[i]);
	}
	adapter->ocb_mac_addr_count = 0;

	/*
	 * Setup locally managed mac addresses for each channel.
	 * If no configured mac address set from userspace,
	 * first channel uses the adapter's default address.
	 */
	for (i = 0; i < config->channel_count; i++) {
		/*
		 * If dsrc_config not set the mac address, then default mac
		 * address from dsrc_config app is all zero and invalid.
		 */
		if (!vos_is_macaddr_zero(
		    (v_MACADDR_t *)config->channels[i].mac_address))
			continue;

		if (i == 0) {
			vos_mem_copy(config->channels[i].mac_address,
				adapter->macAddressCurrent.bytes,
				sizeof(tSirMacAddr));
		} else {
			mac_addr = wlan_hdd_get_intf_addr(adapter->pHddCtx);
			if (mac_addr == NULL) {
				hddLog(LOGE, FL("Cannot obtain mac address"));
				goto fail;
			}
			vos_mem_copy(config->channels[i].mac_address,
				mac_addr, sizeof(tSirMacAddr));
			/* Save the mac address to release later */
			vos_mem_copy(adapter->ocb_mac_address[
				     adapter->ocb_mac_addr_count],
				     config->channels[i].mac_address,
				     sizeof(adapter->ocb_mac_address[
				     adapter->ocb_mac_addr_count]));
			adapter->ocb_mac_addr_count++;
		}
	}

	/* Read the schedule array */
	sched_array = tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_ARRAY];
	if (!sched_array) {
		hddLog(LOGE, FL("No channel present"));
		goto fail;
	}
	if (nla_len(sched_array) != schedule_size * sizeof(*config->schedule)) {
		hddLog(LOGE, FL("SCHEDULE_ARRAY is not the correct size"));
		goto fail;
	}
	vos_mem_copy(config->schedule, nla_data(sched_array),
		nla_len(sched_array));

	/* Copy the NDL chan array */
	if (ndl_chan_list_len) {
		config->dcc_ndl_chan_list_len = ndl_chan_list_len;
		vos_mem_copy(config->dcc_ndl_chan_list, nla_data(ndl_chan_list),
			nla_len(ndl_chan_list));
	}

	/* Copy the NDL active state array */
	if (ndl_active_state_list_len) {
		config->dcc_ndl_active_state_list_len =
			ndl_active_state_list_len;
		vos_mem_copy(config->dcc_ndl_active_state_list,
			nla_data(ndl_active_state_list),
			nla_len(ndl_active_state_list));
	}

	rc = hdd_ocb_set_config_req(adapter, config);
	if (rc)
		hddLog(LOGE, FL("Error while setting OCB config: %d"), rc);

fail:
	vos_mem_free(config);
	return rc;
}

/**
 * wlan_hdd_cfg80211_ocb_set_config() - Interface for set config command
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_ocb_set_config(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ocb_set_config(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_ocb_set_utc_time() - Interface for set UTC time command
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int __wlan_hdd_cfg80211_ocb_set_utc_time(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX + 1];
	struct nlattr *utc_attr;
	struct nlattr *time_error_attr;
	struct sir_ocb_utc *utc;
	int rc = -EINVAL;

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb,
		      QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX,
		      data,
		      data_len, qca_wlan_vendor_ocb_set_utc_time_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Read the UTC time */
	utc_attr = tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_VALUE];
	if (!utc_attr) {
		hddLog(LOGE, FL("UTC_TIME is not present"));
		return -EINVAL;
	}
	if (nla_len(utc_attr) != SIZE_UTC_TIME) {
		hddLog(LOGE, FL("UTC_TIME is not the correct size"));
		return -EINVAL;
	}

	/* Read the time error */
	time_error_attr = tb[QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_ERROR];
	if (!time_error_attr) {
		hddLog(LOGE, FL("UTC_TIME is not present"));
		return -EINVAL;
	}
	if (nla_len(time_error_attr) != SIZE_UTC_TIME_ERROR) {
		hddLog(LOGE, FL("UTC_TIME is not the correct size"));
		return -EINVAL;
	}

	utc = vos_mem_malloc(sizeof(*utc));
	if (!utc) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}
	utc->vdev_id = adapter->sessionId;
	vos_mem_copy(utc->utc_time, nla_data(utc_attr), SIZE_UTC_TIME);
	vos_mem_copy(utc->time_error, nla_data(time_error_attr),
		SIZE_UTC_TIME_ERROR);

	if (sme_ocb_set_utc_time(utc) != eHAL_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error while setting UTC time"));
		rc = -EINVAL;
	} else {
		rc = 0;
	}

	vos_mem_free(utc);
	return rc;
}

/**
 * wlan_hdd_cfg80211_ocb_set_utc_time() - Interface for the set UTC time command
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_ocb_set_utc_time(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ocb_set_utc_time(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_ocb_start_timing_advert() - Interface for start TA cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int
__wlan_hdd_cfg80211_ocb_start_timing_advert(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX + 1];
	struct sir_ocb_timing_advert *timing_advert;
	int rc = -EINVAL;

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	timing_advert = vos_mem_malloc(sizeof(*timing_advert));
	if (!timing_advert) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}
	vos_mem_zero(timing_advert, sizeof(*timing_advert));
	timing_advert->vdev_id = adapter->sessionId;

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb,
		      QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX,
		      data,
		      data_len,
		      qca_wlan_vendor_ocb_start_timing_advert_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		goto fail;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ]) {
		hddLog(LOGE, FL("CHANNEL_FREQ is not present"));
		goto fail;
	}
	timing_advert->chan_freq = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE]) {
		hddLog(LOGE, FL("REPEAT_RATE is not present"));
		goto fail;
	}
	timing_advert->repeat_rate = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE]);

	timing_advert->template_length =
		sme_ocb_gen_timing_advert_frame(hdd_ctx->hHal,
			*(tSirMacAddr *)&adapter->macAddressCurrent.bytes,
			&timing_advert->template_value,
			&timing_advert->timestamp_offset,
			&timing_advert->time_value_offset);
	if (timing_advert->template_length <= 0) {
		hddLog(LOGE, FL("Error while generating the TA frame"));
		goto fail;
	}

	if (sme_ocb_start_timing_advert(timing_advert) != eHAL_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error while starting timing advert"));
		rc = -EINVAL;
	} else {
		rc = 0;
	}

fail:
	if (timing_advert->template_value)
		vos_mem_free(timing_advert->template_value);
	vos_mem_free(timing_advert);
	return rc;
}

/**
 * wlan_hdd_cfg80211_ocb_start_timing_advert() - Interface for the start TA cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_ocb_start_timing_advert(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ocb_start_timing_advert(wiphy, wdev,
							  data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_ocb_stop_timing_advert() - Interface for the stop TA cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int
__wlan_hdd_cfg80211_ocb_stop_timing_advert(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX + 1];
	struct sir_ocb_timing_advert *timing_advert;
	int rc = -EINVAL;

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	timing_advert = vos_mem_malloc(sizeof(*timing_advert));
	if (!timing_advert) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}
	vos_mem_zero(timing_advert, sizeof(sizeof(*timing_advert)));
	timing_advert->vdev_id = adapter->sessionId;

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb,
		      QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX,
		      data,
		      data_len,
		      qca_wlan_vendor_ocb_stop_timing_advert_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		goto fail;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ]) {
		hddLog(LOGE, FL("CHANNEL_FREQ is not present"));
		goto fail;
	}
	timing_advert->chan_freq = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ]);

	if (sme_ocb_stop_timing_advert(timing_advert) != eHAL_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error while stopping timing advert"));
		rc = -EINVAL;
	} else {
		rc = 0;
	}

fail:
	vos_mem_free(timing_advert);
	return rc;
}

/**
 * wlan_hdd_cfg80211_ocb_stop_timing_advert() - Interface for the stop TA cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_ocb_stop_timing_advert(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ocb_stop_timing_advert(wiphy, wdev,
							 data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_ocb_get_tsf_timer_priv - private parameters for get tsf timer
 * @response: response from SME
 * @status: status of response
 */
struct hdd_ocb_get_tsf_timer_priv {
	struct sir_ocb_get_tsf_timer_response response;
	int status;
};

/**
 * hdd_ocb_get_tsf_timer_callback() - Callback to get TSF command
 * @context_ptr: request context
 * @response_ptr: response data
 */
static void hdd_ocb_get_tsf_timer_callback(void *context_ptr,
					   void *response_ptr)
{
	struct hdd_request *hdd_request;
	struct hdd_ocb_get_tsf_timer_priv *priv;
	struct sir_ocb_get_tsf_timer_response *response = response_ptr;

	hdd_request = hdd_request_get(context_ptr);
	if (!hdd_request) {
		hddLog(LOGE, FL("Obsolete request"));
		return;
	}

	priv = hdd_request_priv(hdd_request);
	if (response) {
		priv->response = *response;
		priv->status = 0;
	} else {
		priv->status = -EINVAL;
	}
	hdd_request_complete(hdd_request);
	hdd_request_put(hdd_request);
}

/**
 * __wlan_hdd_cfg80211_ocb_get_tsf_timer() - Interface for get TSF timer cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int
__wlan_hdd_cfg80211_ocb_get_tsf_timer(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)
{
	struct sk_buff *nl_resp = 0;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int rc = -EINVAL;
	struct sir_ocb_get_tsf_timer request = {0};
	void *cookie;
	struct hdd_request *hdd_request;
	struct hdd_ocb_get_tsf_timer_priv *priv;
	static const struct hdd_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_OCB_CMD,
	};

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	/* Initialize the callback context */
	hdd_request = hdd_request_alloc(&params);
	if (!hdd_request) {
		hddLog(LOGE, FL("Request allocation failure"));
		return -ENOMEM;
	}
	cookie = hdd_request_cookie(hdd_request);

	request.vdev_id = adapter->sessionId;
	/* Call the SME function */
	rc = sme_ocb_get_tsf_timer(hdd_ctx->hHal, cookie,
				   hdd_ocb_get_tsf_timer_callback,
				   &request);
	if (rc) {
		hddLog(LOGE, FL("Error calling SME function"));
		/* Need to convert from eHalStatus to errno. */
		rc = -EINVAL;
		goto end;
	}

	rc = hdd_request_wait_for_response(hdd_request);
	if (rc) {
		hddLog(LOGE, FL("Operation timed out"));
		goto end;
	}

	priv = hdd_request_priv(hdd_request);
	rc = priv->status;
	if (rc) {
		hddLog(LOGE, FL("Operation failed: %d"), rc);
		goto end;
	}

	/* Allocate the buffer for the response. */
	nl_resp = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
		2 * (NLA_HDRLEN + sizeof(uint32_t)) + NLMSG_HDRLEN);

	if (!nl_resp) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		rc = -ENOMEM;
		goto end;
	}

	hddLog(LOGE, FL("Got TSF timer response, high=%d, low=%d"),
	       priv->response.timer_high,
	       priv->response.timer_low);

	/* Populate the response. */
	rc = nla_put_u32(nl_resp,
			QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_HIGH,
			priv->response.timer_high);
	if (rc)
		goto end;
	rc = nla_put_u32(nl_resp,
			    QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_LOW,
			    priv->response.timer_low);
	if (rc)
		goto end;

	/* Send the response. */
	rc = cfg80211_vendor_cmd_reply(nl_resp);
	nl_resp = NULL;
	if (rc) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_reply failed: %d"), rc);
		goto end;
	}

end:
	hdd_request_put(hdd_request);
	if (nl_resp)
		kfree_skb(nl_resp);
	return rc;
}

/**
 * wlan_hdd_cfg80211_ocb_get_tsf_timer() - Interface for get TSF timer cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_ocb_get_tsf_timer(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ocb_get_tsf_timer(wiphy, wdev,
						    data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_dcc_stats_priv - DCC stats private parameters
 * @response: response from SME
 * @status: status of response
 */
struct hdd_dcc_stats_priv {
	struct sir_dcc_get_stats_response *response;
	int status;
};

/**
 * hdd_dcc_get_stats_dealloc() - Callback when put request
 * @context_ptr: request context
 *
 * Return: None
 */
static void hdd_dcc_get_stats_dealloc(void *context_ptr)
{
	struct hdd_dcc_stats_priv *priv = context_ptr;

	if (priv->response) {
		vos_mem_free(priv->response);
		priv->response = NULL;
	}
}

/**
 * hdd_dcc_get_stats_callback() - Callback to get stats command
 * @context: request context
 * @response_ptr: response data
 */
static void hdd_dcc_get_stats_callback(void *context, void *response_ptr)
{
	struct hdd_dcc_stats_priv *priv;
	struct sir_dcc_get_stats_response *response = response_ptr;
	struct sir_dcc_get_stats_response *hdd_resp;
	struct hdd_request *request;

	request = hdd_request_get(context);
	if (!request) {
		hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Obsolete request", __func__);
		return;
	}

	priv = hdd_request_priv(request);

	priv->response = vos_mem_malloc(sizeof(*response) +
					response->channel_stats_array_len);
	if (!priv->response) {
		priv->status = -ENOMEM;
		goto end;
	}

	if (response) {
		hdd_resp = priv->response;
		*hdd_resp = *response;
		hdd_resp->channel_stats_array = (uint8_t *)hdd_resp +
						sizeof(*hdd_resp);
		vos_mem_copy(hdd_resp->channel_stats_array,
			     response->channel_stats_array,
			     response->channel_stats_array_len);
		priv->status = 0;
	} else {
		priv->status = -EINVAL;
	}

end:
	hdd_request_complete(request);
	hdd_request_put(request);
}

/**
 * __wlan_hdd_cfg80211_dcc_get_stats() - Interface for get dcc stats
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int __wlan_hdd_cfg80211_dcc_get_stats(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	uint32_t channel_count = 0;
	uint32_t request_array_len = 0;
	void *request_array = 0;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_MAX + 1];
	struct sk_buff *nl_resp = 0;
	int rc = -EINVAL;
	struct sir_dcc_get_stats request = {0};
	void *cookie;
	struct hdd_request *hdd_request;
	struct sir_dcc_get_stats_response *response;
	struct hdd_dcc_stats_priv *priv;
	static const struct hdd_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_OCB_CMD,
		.dealloc = hdd_dcc_get_stats_dealloc,
	};

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_MAX,
		      data,
		      data_len,
		      qca_wlan_vendor_dcc_get_stats)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Validate all the parameters are present */
	if (!tb[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_CHANNEL_COUNT] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_REQUEST_ARRAY]) {
		hddLog(LOGE, FL("Parameters are not present."));
		return -EINVAL;
	}

	channel_count = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_CHANNEL_COUNT]);
	request_array_len = nla_len(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_REQUEST_ARRAY]);
	request_array = nla_data(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_REQUEST_ARRAY]);

	/* Check channel count. Per 11p spec, max 2 channels allowed */
	if (!channel_count || channel_count > CFG_TGT_NUM_OCB_CHANNELS) {
		hddLog(LOGE, FL("Invalid channel_count %d"), channel_count);
		return -EINVAL;
	}

	/* Initialize the callback context */
	hdd_request = hdd_request_alloc(&params);
	if (!hdd_request) {
		hddLog(LOGE, FL("Request allocation failure"));
		return -ENOMEM;
	}
	priv = hdd_request_priv(hdd_request);
	priv->response = NULL;
	cookie = hdd_request_cookie(hdd_request);

	request.vdev_id = adapter->sessionId;
	request.channel_count = channel_count;
	request.request_array_len = request_array_len;
	request.request_array = request_array;

	/* Call the SME function. */
	rc = sme_dcc_get_stats(hdd_ctx->hHal, cookie,
			       hdd_dcc_get_stats_callback,
			       &request);
	if (rc) {
		hddLog(LOGE, FL("Error calling SME function"));
		/* Need to convert from eHalStatus to errno. */
		rc = -EINVAL;
		goto end;
	}

	/* Wait for the function to complete. */
	rc = hdd_request_wait_for_response(hdd_request);
	if (rc) {
		hddLog(LOGE, FL("Operation timed out"));
		goto end;
	}

	priv = hdd_request_priv(hdd_request);
	rc = priv->status;
	if (rc) {
		hddLog(LOGE, FL("Operation failed: %d"), rc);
		goto end;
	}

	response = priv->response;
	/* Allocate the buffer for the response. */
	nl_resp = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(uint32_t) +
		response->channel_stats_array_len + 2 * NLA_HDRLEN +
		NLMSG_HDRLEN);
	if (!nl_resp) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		rc = -ENOMEM;
		goto end;
	}

	/* Populate the response. */
	rc = nla_put_u32(nl_resp,
			 QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_CHANNEL_COUNT,
			 response->num_channels);
	if (rc)
		goto end;
	rc = nla_put(nl_resp,
		     QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_STATS_ARRAY,
		     response->channel_stats_array_len,
		     response->channel_stats_array);
	if (rc)
		goto end;

	/* Send the response. */
	rc = cfg80211_vendor_cmd_reply(nl_resp);
	nl_resp = NULL;
	if (rc) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_reply failed: %d"), rc);
		goto end;
	}

	/* fall through */
end:
	hdd_request_put(hdd_request);
	if (nl_resp)
		kfree_skb(nl_resp);
	return rc;
}

/**
 * wlan_hdd_cfg80211_dcc_get_stats() - Interface for get dcc stats
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_dcc_get_stats(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_dcc_get_stats(wiphy, wdev,
						data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_dcc_clear_stats() - Interface for clear dcc stats cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int __wlan_hdd_cfg80211_dcc_clear_stats(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_MAX + 1];

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb,
		      QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_MAX,
		      data,
		      data_len,
		      qca_wlan_vendor_dcc_clear_stats)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Verify that the parameter is present */
	if (!tb[QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_BITMAP]) {
		hddLog(LOGE, FL("Parameters are not present."));
		return -EINVAL;
	}

	/* Call the SME function */
	if (sme_dcc_clear_stats(adapter->sessionId,
		nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_BITMAP])) !=
			eHAL_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error calling SME function."));
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_dcc_clear_stats() - Interface for clear dcc stats cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_dcc_clear_stats(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_dcc_clear_stats(wiphy, wdev,
						  data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

struct hdd_dcc_update_ndl_priv {
	int status;
};

/**
 * hdd_dcc_update_ndl_callback() - Callback to update NDL command
 * @context_ptr: request context
 * @response_ptr: response data
 */
static void hdd_dcc_update_ndl_callback(void *context_ptr, void *response_ptr)
{
	struct hdd_request *hdd_request;
	struct hdd_dcc_update_ndl_priv *priv;
	struct sir_dcc_update_ndl_response *response = response_ptr;

	hdd_request = hdd_request_get(context_ptr);
	if (!hdd_request) {
		hddLog(LOGE, FL("Obsolete request"));
		return;
	}
	priv = hdd_request_priv(hdd_request);
	if (response && (0 == response->status))
		priv->status = 0;
	else
		priv->status = -EINVAL;
	hdd_request_complete(hdd_request);
	hdd_request_put(hdd_request);
}

/**
 * __wlan_hdd_cfg80211_dcc_update_ndl() - Interface for update dcc cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
static int __wlan_hdd_cfg80211_dcc_update_ndl(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_MAX + 1];
	struct sir_dcc_update_ndl request;
	uint32_t channel_count;
	uint32_t ndl_channel_array_len;
	void *ndl_channel_array;
	uint32_t ndl_active_state_array_len;
	void *ndl_active_state_array;
	int rc = -EINVAL;
	void *cookie;
	struct hdd_request *hdd_request;
	struct hdd_dcc_update_ndl_priv *priv;
	static const struct hdd_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_OCB_CMD,
	};

	ENTER();

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != WLAN_HDD_OCB) {
		hddLog(LOGE, FL("Device not in OCB mode!"));
		return -EINVAL;
	}

	if (!wma_is_vdev_up(adapter->sessionId)) {
		hddLog(LOGE, FL("The device has not been started"));
		return -EINVAL;
	}

	/* Parse the netlink message */
	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_MAX,
		      data,
		      data_len,
		      qca_wlan_vendor_dcc_update_ndl)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Verify that the parameter is present */
	if (!tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_COUNT] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_ARRAY] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_ACTIVE_STATE_ARRAY]) {
		hddLog(LOGE, FL("Parameters are not present."));
		return -EINVAL;
	}

	channel_count = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_COUNT]);
	ndl_channel_array_len = nla_len(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_ARRAY]);
	ndl_channel_array = nla_data(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_ARRAY]);
	ndl_active_state_array_len = nla_len(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_ACTIVE_STATE_ARRAY]);
	ndl_active_state_array = nla_data(
		tb[QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_ACTIVE_STATE_ARRAY]);

	hdd_request = hdd_request_alloc(&params);
	if (!hdd_request) {
		hddLog(LOGE, "Request allocation failure");
		return -ENOMEM;
	}
	cookie = hdd_request_cookie(hdd_request);

	/* Copy the parameters to the request structure. */
	request.vdev_id = adapter->sessionId;
	request.channel_count = channel_count;
	request.dcc_ndl_chan_list_len = ndl_channel_array_len;
	request.dcc_ndl_chan_list = ndl_channel_array;
	request.dcc_ndl_active_state_list_len = ndl_active_state_array_len;
	request.dcc_ndl_active_state_list = ndl_active_state_array;

	/* Call the SME function */
	rc = sme_dcc_update_ndl(hdd_ctx->hHal, cookie,
				hdd_dcc_update_ndl_callback,
				&request);
	if (rc) {
		hddLog(LOGE, FL("Error calling SME function."));
		/* Convert from eHalStatus to errno */
		rc = -EINVAL;
		goto end;
	}

	/* Wait for the function to complete. */
	rc = hdd_request_wait_for_response(hdd_request);
	if (rc) {
		hddLog(LOGE, FL("Operation timed out"));
		goto end;
	}

	priv = hdd_request_priv(hdd_request);
	rc = priv->status;
	if (rc)
		hddLog(LOGE, FL("Operation failed: %d"), rc);

	/* fall through */
end:
	hdd_request_put(hdd_request);

	return rc;
}

/**
 * wlan_hdd_cfg80211_dcc_update_ndl() - Interface for update dcc cmd
 * @wiphy: pointer to the wiphy
 * @wdev: pointer to the wdev
 * @data: The netlink data
 * @data_len: The length of the netlink data in bytes
 *
 * Return: 0 on success.
 */
int wlan_hdd_cfg80211_dcc_update_ndl(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_dcc_update_ndl(wiphy, wdev,
						 data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * wlan_hdd_dcc_stats_event_callback() - Callback to get stats event
 * @context_ptr: request context
 * @response_ptr: response data
 */
static void wlan_hdd_dcc_stats_event_callback(void *context_ptr,
					      void *response_ptr)
{
	hdd_context_t *hdd_ctx = (hdd_context_t *)context_ptr;
	struct sir_dcc_get_stats_response *resp = response_ptr;
	struct sk_buff *vendor_event;

	ENTER();

	vendor_event =
		cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
			NULL, sizeof(uint32_t) + resp->channel_stats_array_len +
			NLMSG_HDRLEN,
			QCA_NL80211_VENDOR_SUBCMD_DCC_STATS_EVENT_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_CHANNEL_COUNT,
			resp->num_channels) ||
		nla_put(vendor_event,
			QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_STATS_ARRAY,
			resp->channel_stats_array_len,
			resp->channel_stats_array)) {
		hddLog(LOGE, FL("nla put failed"));
		kfree_skb(vendor_event);
		return;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

/**
 * wlan_hdd_dcc_register_for_dcc_stats_event() - Register for dcc stats events
 * @hdd_ctx: hdd context
 */
void wlan_hdd_dcc_register_for_dcc_stats_event(hdd_context_t *hdd_ctx)
{
	int rc;

	rc = sme_register_for_dcc_stats_event(hdd_ctx->hHal, hdd_ctx,
		wlan_hdd_dcc_stats_event_callback);
	if (rc)
		hddLog(LOGE, FL("Register callback failed: %d"), rc);
}

static void wlan_hdd_dsrc_update_radio_chan_stats(
		struct dsrc_radio_chan_stats_ctxt *ctx,
		struct radio_chan_stats_rsp *resp)
{
	int i, j;
	struct radio_chan_stats_info *src, *dest;

	if (!ctx || !resp)
		return;

	if (resp->num_chans > ctx->config_chans_num)
		return;

	src = resp->chan_stats;
	dest = ctx->chan_stats;
	/* Check if current event is for previous channel configuration. */
	for (i = 0; i < resp->num_chans; i++, src++) {
		if (!src) {
			hddLog(LOGE, FL("Channel stats data is null"));
			return;
		}

		for (j = 0; j < ctx->config_chans_num; j++) {
			if (src->chan_freq == ctx->config_chans_freq[j])
				break;
		}
		if (j == ctx->config_chans_num) {
			/*
			 * This DSRC Channel Radio channel statistics event
			 * is for previous old channel configuration.
			 * Now just Ignore this type event and clear saved
			 * entry in driver. If possible, driver can post
			 * the recorders to application.
			 */
			adf_os_spin_lock(&ctx->chan_stats_lock);
			ctx->chan_stats_num = 0;
			vos_mem_zero(dest, 2 * sizeof(*dest));
			adf_os_spin_unlock(&ctx->chan_stats_lock);
			hddLog(LOGE, FL("Old Chan Stats Data"));
			return;
		}
	}

	/* Save the first channels statistics event in adapter. */
	src = resp->chan_stats;
	if (!ctx->chan_stats_num) {
		adf_os_spin_lock(&ctx->chan_stats_lock);
		vos_mem_copy(dest, src, resp->num_chans * sizeof(*src));
		ctx->chan_stats_num = resp->num_chans;
		adf_os_spin_unlock(&ctx->chan_stats_lock);
		return;
	}

	/* Merge new received channel statistics data to previous entry. */
	adf_os_spin_lock(&ctx->chan_stats_lock);
	for (i = 0; i < resp->num_chans; i++, src++) {
		struct radio_chan_stats_info *dest_entry = NULL;
		struct radio_chan_stats_info *empty_entry = NULL;

		/* Now only two channel stats supported. */
		dest = ctx->chan_stats;
		for (j = 0; j < DSRC_MAX_CHAN_STATS_CNT; j++, dest++) {
			/* Get empty entry */
			if (dest->chan_freq == 0) {
				empty_entry = dest;
				continue;
			}
			if (src->chan_freq == dest->chan_freq) {
				dest_entry = dest;
				break;
			}
		}

		if (dest_entry) {
			dest = dest_entry;
		} else if (empty_entry) {
			/* Copy new recorders to new entry*/
			ctx->chan_stats_num++;
			vos_mem_copy(empty_entry, src, sizeof(*src));
			continue;
		} else {
			adf_os_spin_unlock(&ctx->chan_stats_lock);
			hddLog(LOGE, FL("No entry found."));
			return;
		}

		/* Ignore Invalid statistics data. */
		if (src->measurement_period == 0) {
			hddLog(LOGE, FL("Invalid stats data."));
			continue;
		}

		/* Merge Channel Statistics. */
		dest->measurement_period += src->measurement_period;
		dest->on_chan_us += src->on_chan_us;
		dest->on_chan_ratio = (uint32_t)vos_do_div64(
				dest->on_chan_us * 100,
				dest->measurement_period);
		dest->tx_duration_us += src->tx_duration_us;
		dest->rx_duration_us += src->rx_duration_us;
		if (dest->on_chan_us == 0) {
			dest->chan_busy_ratio = 0;
		} else {
			dest->chan_busy_ratio = (uint32_t)vos_do_div64(
				(dest->tx_duration_us +
				 dest->rx_duration_us) * 100,
				dest->on_chan_us);
		}
		dest->tx_mpdus += src->tx_mpdus;
		dest->tx_msdus += src->tx_msdus;
		dest->rx_succ_pkts += src->rx_succ_pkts;
		dest->rx_fail_pkts += src->rx_fail_pkts;
	}
	adf_os_spin_unlock(&ctx->chan_stats_lock);

	return;
}

/**
 * wlan_hdd_dsrc_radio_chan_stats_event_callback() - Callback function for
 * WLAN DSRC Radio channel statistics event.
 * @context_ptr: pointer to radio channel statistics context.
 * @resp_ptr: pointer to radio channel statistics event buffer.
 */
static void wlan_hdd_dsrc_radio_chan_stats_event_callback(void *context_ptr,
							  void *resp_ptr)
{
	int i;
	struct radio_chan_stats_req *req;
	struct radio_chan_stats_rsp *resp;
	struct radio_chan_stats_info *chan_stats;
	struct dsrc_radio_chan_stats_ctxt *ctx;

	if (!context_ptr || !resp_ptr)
		return;

	ctx = (struct dsrc_radio_chan_stats_ctxt *)context_ptr;
	resp = (struct radio_chan_stats_rsp *)resp_ptr;
	chan_stats = resp->chan_stats;

	if (!chan_stats) {
		hddLog(LOGE, FL("No channel stats data"));
		return;
	}

	wlan_hdd_dsrc_update_radio_chan_stats(ctx, resp);

	/*
	 * DSRC Radio channel statistics RADIO_CHAN_STATS event is reported
	 * to HDD as following cases.
	 * 1. FW randomly report event caused by overflow,
	 *	configuration change....
	 * 2. Firmware response to the request from Host APP.
	 * Need check whether current event is response for request.
	 */
	adf_os_spin_lock(&hdd_context_lock);
	if ((ctx->magic != HDD_OCB_MAGIC) || (!ctx->cur_req)) {
		adf_os_spin_unlock(&hdd_context_lock);
		return;
	}
	req = ctx->cur_req;
	switch (req->req_type) {
	case WLAN_DSRC_REQUEST_ONE_RADIO_CHAN_STATS:
		if ((resp->num_chans == 1) &&
		    (req->chan_freq == chan_stats->chan_freq)) {
			complete(&ctx->completion_evt);
			adf_os_spin_unlock(&hdd_context_lock);
			return;
		}
		break;
	case WLAN_DSRC_REQUEST_ALL_RADIO_CHAN_STATS:
		if (resp->num_chans != ctx->config_chans_num) {
			adf_os_spin_unlock(&hdd_context_lock);
			return;
		}
		/* Check response channel is configured. */
		for (i = 0; i < resp->num_chans; i++) {
			if (chan_stats[i].chan_freq !=
			    ctx->config_chans_freq[i]) {
				adf_os_spin_unlock(&hdd_context_lock);
				return;
			}
		}
		complete(&ctx->completion_evt);
		break;
	}
	adf_os_spin_unlock(&hdd_context_lock);

	return;
}

int wlan_hdd_dsrc_config_radio_chan_stats(hdd_adapter_t *adapter,
					  bool enable_chan_stats)
{
	int ret = 0;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct dsrc_radio_chan_stats_ctxt *ctx;
	struct radio_chan_stats_info *chan_stats;

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	ctx = &adapter->dsrc_chan_stats;
	if (ctx->enable_chan_stats == enable_chan_stats) {
		hddLog(LOGE, FL("DSRC channel stats already %s\n"),
			enable_chan_stats == true ? "enable" : "disable");
		return ret;
	}

	ctx->chan_stats_num = 0;
	chan_stats = ctx->chan_stats;
	vos_mem_zero(chan_stats, DSRC_MAX_CHAN_STATS_CNT * sizeof(*chan_stats));

	if (enable_chan_stats) {
		adf_os_spinlock_init(&ctx->chan_stats_lock);
		ret = sme_register_radio_chan_stats_cb(
			((hdd_context_t *)adapter->pHddCtx)->hHal, (void *)ctx,
			wlan_hdd_dsrc_radio_chan_stats_event_callback);
	} else {
		ret = sme_unregister_radio_chan_stats_cb(
			((hdd_context_t *)adapter->pHddCtx)->hHal);
	}

	ret = process_wma_set_command((int)adapter->sessionId,
			(int)WMI_PDEV_PARAM_RADIO_CHAN_STATS_ENABLE,
			enable_chan_stats, PDEV_CMD);

	if (!ret)
		ctx->enable_chan_stats = enable_chan_stats;

	return ret;
}

int wlan_hdd_dsrc_request_radio_chan_stats(hdd_adapter_t *adapter,
					   struct radio_chan_stats_req *req)
{
	int ret = 0;
	eHalStatus halStatus;
	struct dsrc_radio_chan_stats_ctxt *ctx;

	halStatus = sme_request_radio_chan_stats(
			((hdd_context_t *)adapter->pHddCtx)->hHal, req);
	if (halStatus != eHAL_STATUS_SUCCESS) {
		hddLog(LOGE, FL("Error call dsrc chan stats req func."));
		return -EINVAL;
	}

	ctx = &adapter->dsrc_chan_stats;
	init_completion(&ctx->completion_evt);
	adf_os_spin_lock(&hdd_context_lock);
	ctx->magic = HDD_OCB_MAGIC;
	adf_os_spin_unlock(&hdd_context_lock);
	if (!wait_for_completion_timeout(&ctx->completion_evt,
		msecs_to_jiffies(WLAN_WAIT_TIME_OCB_CMD))) {
		hddLog(LOGE, FL("Wait for request completion timedout."));
		ret = -ETIMEDOUT;
	}

	adf_os_spin_lock(&hdd_context_lock);
	ctx->magic = 0;
	adf_os_spin_unlock(&hdd_context_lock);
	return ret;
}

void wlan_hdd_dsrc_deinit_chan_stats(hdd_adapter_t *adapter)
{
	struct dsrc_radio_chan_stats_ctxt *ctx;

	if (!adapter)
		return;

	ctx = &adapter->dsrc_chan_stats;
	if (ctx->cur_req) {
		vos_mem_free(ctx->cur_req);
		ctx->cur_req = NULL;
	}

	return;
}
