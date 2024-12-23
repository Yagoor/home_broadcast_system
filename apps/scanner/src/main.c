/*
 * Copyright (c) 2022-2024 Nordic Semiconductor ASA
 * Copyright (c) 2024 Demant A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define NAME_LEN 64

struct bt_scan_recv_info {
	uint32_t broadcast_id;
	char broadcast_name[BT_AUDIO_BROADCAST_NAME_LEN_MAX + 1];
};

static bool broadcast_source_found(struct bt_data *data, void *user_data)
{
	struct bt_scan_recv_info *sr_info = (struct bt_scan_recv_info *)user_data;
	struct bt_uuid_16 adv_uuid;

	switch (data->type) {
	case BT_DATA_SVC_DATA16:
		if (data->data_len < BT_UUID_SIZE_16 + BT_AUDIO_BROADCAST_ID_SIZE) {
			return true;
		}

		if (!bt_uuid_create(&adv_uuid.uuid, data->data, BT_UUID_SIZE_16)) {
			return true;
		}

		if (bt_uuid_cmp(&adv_uuid.uuid, BT_UUID_BROADCAST_AUDIO) != 0) {
			return true;
		}

		sr_info->broadcast_id = sys_get_le24(data->data + BT_UUID_SIZE_16);
		return true;
	case BT_DATA_BROADCAST_NAME:
		if (!IN_RANGE(data->data_len, BT_AUDIO_BROADCAST_NAME_LEN_MIN,
		    BT_AUDIO_BROADCAST_NAME_LEN_MAX)) {
			return true;
		}

		utf8_lcpy(sr_info->broadcast_name, data->data, (data->data_len) + 1);
		return true;
	default:
		return true;
	}
}

static void broadcast_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
	struct bt_scan_recv_info sr_info = { 0 };

	/* We are only interested in non-connectable periodic advertisers */
	if ((info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0 ||
	    info->interval == 0) {
		return;
	}

	bt_data_parse(ad, broadcast_source_found, (void *)&sr_info);
	LOG_INF("Found broadcast with name %s and id 0x%06x", sr_info.broadcast_name, sr_info.broadcast_id);
}

static struct bt_le_scan_cb bap_scan_cb = {
	.recv = broadcast_scan_recv,
};

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth enable failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	bt_le_scan_cb_register(&bap_scan_cb);

	LOG_INF("Scanning for broadcast sources");

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
	if (err != 0 && err != -EALREADY) {
		LOG_INF("Unable to start scan for broadcast sources: %d",
				err);
		return 0;
	}

	k_sleep(K_FOREVER);

	return 0;
}
