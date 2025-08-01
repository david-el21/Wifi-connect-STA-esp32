/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "os/endian.h"

#if MYNEWT_VAL(BLE_GATTS)

#define PPCP_ENABLED \
    MYNEWT_VAL(BLE_ROLE_PERIPHERAL) && \
    (MYNEWT_VAL(BLE_SVC_GAP_PPCP_MIN_CONN_INTERVAL) || \
     MYNEWT_VAL(BLE_SVC_GAP_PPCP_MAX_CONN_INTERVAL) || \
     MYNEWT_VAL(BLE_SVC_GAP_PPCP_SLAVE_LATENCY) || \
     MYNEWT_VAL(BLE_SVC_GAP_PPCP_SUPERVISION_TMO))

#define BLE_SVC_GAP_NAME_MAX_LEN \
    MYNEWT_VAL(BLE_SVC_GAP_DEVICE_NAME_MAX_LENGTH)

static ble_svc_gap_chr_changed_fn *ble_svc_gap_chr_changed_cb_fn;

static char ble_svc_gap_name[BLE_SVC_GAP_NAME_MAX_LEN + 1] =
        MYNEWT_VAL(BLE_SVC_GAP_DEVICE_NAME);
static uint16_t ble_svc_gap_appearance = MYNEWT_VAL(BLE_SVC_GAP_APPEARANCE);

#if MYNEWT_VAL(ENC_ADV_DATA)
static uint16_t ble_svc_gap_enc_adv_data_handle;
static struct key_material km = {
    .session_key = {0},
    .iv = {0},
};
#endif

#if NIMBLE_BLE_CONNECT
static int
ble_svc_gap_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def ble_svc_gap_defs[] = {
    {
        /*** Service: GAP. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Device Name. */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME),
            .access_cb = ble_svc_gap_access,
            .flags = BLE_GATT_CHR_F_READ |
#if MYNEWT_VAL(BLE_SVC_GAP_DEVICE_NAME_WRITE_PERM) >= 0
                     BLE_GATT_CHR_F_WRITE |
                     MYNEWT_VAL(BLE_SVC_GAP_DEVICE_NAME_WRITE_PERM) |
#endif
                     0,
        }, {
            /*** Characteristic: Appearance. */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_APPEARANCE),
            .access_cb = ble_svc_gap_access,
            .flags = BLE_GATT_CHR_F_READ |
#if MYNEWT_VAL(BLE_SVC_GAP_APPEARANCE_WRITE_PERM) >= 0
                     BLE_GATT_CHR_F_WRITE |
                     MYNEWT_VAL(BLE_SVC_GAP_APPEARANCE_WRITE_PERM) |
#endif
                     0,
        }, {
#if PPCP_ENABLED
            /*** Characteristic: Peripheral Preferred Connection Parameters. */
            .uuid =
                BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS),
            .access_cb = ble_svc_gap_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
#endif
#if MYNEWT_VAL(BLE_SVC_GAP_CENTRAL_ADDRESS_RESOLUTION) >= 0
            /*** Characteristic: Central Address Resolution. */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_CENTRAL_ADDRESS_RESOLUTION),
            .access_cb = ble_svc_gap_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
#endif
#if MYNEWT_VAL(BLE_SVC_GAP_RPA_ONLY)
            /*** Characteristic: Resolvable Private Address Only. */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_RPA_ONLY),
            .access_cb = ble_svc_gap_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
#endif
#if MYNEWT_VAL(ENC_ADV_DATA)
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_KEY_MATERIAL),
            .access_cb = ble_svc_gap_access,
            .val_handle = &ble_svc_gap_enc_adv_data_handle,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN | BLE_GATT_CHR_F_READ_AUTHOR | BLE_GATT_CHR_F_INDICATE,
        }, {
#endif
#if MYNEWT_VAL(BLE_SVC_GAP_GATT_SECURITY_LEVEL)
            /*** Characteristic: LE GATT Security Levels. */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_GAP_CHR_UUID16_LE_GATT_SECURITY_LEVELS),
            .access_cb = ble_svc_gap_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
#endif
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static int
ble_svc_gap_device_name_read_access(struct ble_gatt_access_ctxt *ctxt)
{
    int rc;

    rc = os_mbuf_append(ctxt->om, ble_svc_gap_name, strlen(ble_svc_gap_name));

    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
ble_svc_gap_device_name_write_access(struct ble_gatt_access_ctxt *ctxt)
{
#if MYNEWT_VAL(BLE_SVC_GAP_DEVICE_NAME_WRITE_PERM) < 0
    assert(0);
    return 0;
#else
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len > BLE_SVC_GAP_NAME_MAX_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(ctxt->om, ble_svc_gap_name, om_len, NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    ble_svc_gap_name[om_len] = '\0';

    if (ble_svc_gap_chr_changed_cb_fn) {
        ble_svc_gap_chr_changed_cb_fn(BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME);
    }

    return rc;
#endif
}

static int
ble_svc_gap_appearance_read_access(struct ble_gatt_access_ctxt *ctxt)
{
    uint16_t appearance = htole16(ble_svc_gap_appearance);
    int rc;

    rc = os_mbuf_append(ctxt->om, &appearance, sizeof(appearance));

    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
ble_svc_gap_appearance_write_access(struct ble_gatt_access_ctxt *ctxt)
{
#if MYNEWT_VAL(BLE_SVC_GAP_APPEARANCE_WRITE_PERM) < 0
    assert(0);
    return 0;
#else
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len != sizeof(ble_svc_gap_appearance)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(ctxt->om, &ble_svc_gap_appearance, om_len, NULL);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    ble_svc_gap_appearance = le16toh(ble_svc_gap_appearance);

    if (ble_svc_gap_chr_changed_cb_fn) {
        ble_svc_gap_chr_changed_cb_fn(BLE_SVC_GAP_CHR_UUID16_APPEARANCE);
    }

    return rc;
#endif
}

#if MYNEWT_VAL(BLE_SVC_GAP_GATT_SECURITY_LEVEL)
static int
ble_svc_gap_security_level_read_access(uint16_t conn_handle, struct os_mbuf * om)
{
    uint8_t security_level[2];
    int rc;
    
    /* Currently this characteristic is only supported for
     * Security Mode 1
     */
    security_level[0] = 0x01; //Mode 1
    security_level[1] = ble_gatts_security_mode_1_level(); //Mode 1 Level
    
    rc = os_mbuf_append(om, security_level, sizeof(security_level));

    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
#endif


static int
ble_svc_gap_access(uint16_t conn_handle, uint16_t attr_handle,
                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;
#if MYNEWT_VAL(BLE_SVC_GAP_CENTRAL_ADDRESS_RESOLUTION) >= 0
    uint8_t central_ar = MYNEWT_VAL(BLE_SVC_GAP_CENTRAL_ADDRESS_RESOLUTION);
#endif
#if PPCP_ENABLED
    uint16_t ppcp[4] = {
        htole16(MYNEWT_VAL(BLE_SVC_GAP_PPCP_MIN_CONN_INTERVAL)),
        htole16(MYNEWT_VAL(BLE_SVC_GAP_PPCP_MAX_CONN_INTERVAL)),
        htole16(MYNEWT_VAL(BLE_SVC_GAP_PPCP_SLAVE_LATENCY)),
        htole16(MYNEWT_VAL(BLE_SVC_GAP_PPCP_SUPERVISION_TMO))
    };
#endif
#if MYNEWT_VAL(BLE_SVC_GAP_RPA_ONLY)
    /* As per Core Specification 6.0, Vol 3: Host, Part C: GAP, 12.5
     * The only allowed value for the characteristic is zero.
     * All other values are RFU.
     * As such, the presence of the characteristic itself indicates that
     * the device is RPA only
     */
    uint8_t rpa_only = 0;
#endif
    int rc;

    uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    assert(uuid16 != 0);

    switch (uuid16) {
    case BLE_SVC_GAP_CHR_UUID16_DEVICE_NAME:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = ble_svc_gap_device_name_read_access(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = ble_svc_gap_device_name_write_access(ctxt);
        } else {
            assert(0);
            rc = BLE_ATT_ERR_UNLIKELY;
        }
        return rc;

    case BLE_SVC_GAP_CHR_UUID16_APPEARANCE:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = ble_svc_gap_appearance_read_access(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            rc = ble_svc_gap_appearance_write_access(ctxt);
        } else {
            assert(0);
            rc = BLE_ATT_ERR_UNLIKELY;
        }
        return rc;

#if PPCP_ENABLED
    case BLE_SVC_GAP_CHR_UUID16_PERIPH_PREF_CONN_PARAMS:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, &ppcp, sizeof(ppcp));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
#endif

#if MYNEWT_VAL(BLE_SVC_GAP_CENTRAL_ADDRESS_RESOLUTION) >= 0
    case BLE_SVC_GAP_CHR_UUID16_CENTRAL_ADDRESS_RESOLUTION:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, &central_ar, sizeof(central_ar));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
#endif

#if MYNEWT_VAL(BLE_SVC_GAP_RPA_ONLY)
    case BLE_SVC_GAP_CHR_UUID16_RPA_ONLY:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, &rpa_only, sizeof(rpa_only));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
#endif

#if MYNEWT_VAL(ENC_ADV_DATA)
    case BLE_SVC_GAP_CHR_UUID16_KEY_MATERIAL:
        rc = os_mbuf_append(ctxt->om, &(km.session_key), sizeof(km.session_key));
        rc = os_mbuf_append(ctxt->om, &(km.iv), sizeof(km.iv));

        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
#endif

#if MYNEWT_VAL(BLE_SVC_GAP_GATT_SECURITY_LEVEL)
    case BLE_SVC_GAP_CHR_UUID16_LE_GATT_SECURITY_LEVELS:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = ble_svc_gap_security_level_read_access(conn_handle, ctxt->om);
        return rc;
#endif

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}
#endif

const char *
ble_svc_gap_device_name(void)
{
    return ble_svc_gap_name;
}

int
ble_svc_gap_device_name_set(const char *name)
{
    int len;

    len = strlen(name);
    if (len > BLE_SVC_GAP_NAME_MAX_LEN) {
        return BLE_HS_EINVAL;
    }

    memcpy(ble_svc_gap_name, name, len);
    ble_svc_gap_name[len] = '\0';

    return 0;
}

uint16_t
ble_svc_gap_device_appearance(void)
{
  return ble_svc_gap_appearance;
}

int
ble_svc_gap_device_appearance_set(uint16_t appearance)
{
    ble_svc_gap_appearance = appearance;

    return 0;
}

void
ble_svc_gap_set_chr_changed_cb(ble_svc_gap_chr_changed_fn *cb)
{
    ble_svc_gap_chr_changed_cb_fn = cb;
}

#if MYNEWT_VAL(ENC_ADV_DATA)
int
ble_svc_gap_device_key_material_set(uint8_t *session_key, uint8_t *iv)
{
    memcpy(&km.session_key, session_key, BLE_EAD_KEY_SIZE);
    memcpy(&km.iv, iv, BLE_EAD_IV_SIZE);
    ble_gatts_chr_updated(ble_svc_gap_enc_adv_data_handle);
    return 0;
}
#endif

void
ble_svc_gap_init(void)
{
#if NIMBLE_BLE_CONNECT
    int rc;
#endif

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

#if NIMBLE_BLE_CONNECT
    rc = ble_gatts_count_cfg(ble_svc_gap_defs);
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = ble_gatts_add_svcs(ble_svc_gap_defs);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}

void
ble_svc_gap_deinit(void)
{
    ble_gatts_free_svcs();
}
#endif
