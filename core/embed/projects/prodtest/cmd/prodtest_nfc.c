/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (c) SatoshiLabs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USE_NFC
#define USE_NFC
#endif
#ifdef USE_NFC

#include <trezor_rtl.h>

#include <io/nfc.h>
#include <rtl/cli.h>
#include <sys/sysevent_source.h>
#include <sys/systick.h>
#include "rfal_nfc.h"
#include "rfal_t2t.h"
#include "rfal_t4t.h"

static nfc_dev_info_t dev_info = {0};

static const uint8_t aid_select_ndef_app[7] = {0xd2, 0x76, 0x00, 0x00,
                                               0x85, 0x01, 0x01};
// static const uint8_t aid_select_cc_file[7] = {0x00, 0xa4, 0x00, 0x0c,
//                                               0x02, 0xe1, 0x03};
// static const uint8_t aid_read_cc_file[5] = {0x00, 0xb0, 0x00, 0x00, 0x00};
// static const uint8_t aid_select_ndef_file[7] = {0x00, 0xa4, 0x00, 0x0c,
//                                                 0x02, 0xe1, 0x04};
// static const uint8_t aid_read_ndef_file_len[5] = {0x00, 0xb0, 0x00, 0x00,
// 0x02}; static const uint8_t aid_read_ndef_file[5] = {0x00, 0xb0, 0x00, 0x02,
// 0x0e};

static void prodtest_nfc_read_card(cli_t* cli) {
  uint32_t timeout = 0;
  bool timeout_set = false;
  memset(&dev_info, 0, sizeof(dev_info));

  if (cli_has_arg(cli, "timeout")) {
    if (!cli_arg_uint32(cli, "timeout", &timeout)) {
      cli_error_arg(cli, "Expecting timeout argument.");
      return;
    }
    timeout_set = true;
  }

  if (cli_arg_count(cli) > 1) {
    cli_error_arg_count(cli);
    return;
  }

  nfc_status_t nfc_status = nfc_start_discovery();
  if (nfc_status == NFC_NOT_INITIALIZED) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC not initialized");
    goto cleanup;
  } else if (nfc_status != NFC_OK) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC activation failed");
    goto cleanup;
  } else if (timeout_set) {
    cli_trace(cli, "NFC activated in reader mode for %d ms.", timeout);
  } else {
    cli_trace(cli, "NFC activated in reader mode");
  }

  // Clear leftover events
  nfc_event_t event_flag;
  nfc_get_event(&event_flag);
  sysevents_t awaited_events = {0};
  awaited_events.read_ready = 1 << SYSHANDLE_NFC;
  sysevents_t signalled_events = {0};
  sysevents_poll(&awaited_events, &signalled_events, ticks_timeout(0));

  uint32_t expire_time = ticks_timeout(timeout);
  while (true) {
    if (cli_aborted(cli)) {
      cli_trace(cli, "NFC test aborted");
      break;
    }

    if (timeout_set && ticks_expired(expire_time)) {
      cli_error(cli, CLI_ERROR_TIMEOUT, "NFC timeout");
      goto cleanup;
    }

    sysevents_poll(&awaited_events, &signalled_events, ticks_timeout(10));

    if ((signalled_events.read_ready & 1 << SYSHANDLE_NFC) == 0) {
      continue;
    }

    if (!nfc_get_event(&event_flag)) {
      cli_error(cli, CLI_ERROR, "Failed to get NFC events");
      continue;
    }

    if (event_flag == NFC_EVENT_CONNECTED) {
      nfc_dev_read_info(&dev_info);
      cli_trace(cli, "NFC card detected.");

      switch (dev_info.type) {
        case NFC_DEV_TYPE_A:
          cli_trace(cli, "NFC Type A: UID: %s", dev_info.uid);
          break;
        case NFC_DEV_TYPE_B:
          cli_trace(cli, "NFC Type B: UID: %s", dev_info.uid);
          break;
        case NFC_DEV_TYPE_F:
          cli_trace(cli, "NFC Type F: UID: %s", dev_info.uid);
          break;
        case NFC_DEV_TYPE_V:
          cli_trace(cli, "NFC Type V: UID: %s", dev_info.uid);
          break;
        case NFC_DEV_TYPE_ST25TB:
          cli_trace(cli, "NFC Type ST25TB: UID: %s", dev_info.uid);
          break;
        case NFC_DEV_TYPE_AP2P:
          cli_trace(cli, "NFC Type AP2P: UID: %s", dev_info.uid);
          break;
        case NFC_DEV_TYPE_UNKNOWN:
          cli_trace(cli, "NFC Type UNKNOWN");
          break;
        default:
          cli_error(cli, CLI_ERROR_ABORT, "NFC ERROR Unexpected");
          goto cleanup;
      }

      switch (dev_info.interface) {
        case NFC_DEV_INTERFACE_RF:
          cli_trace(cli, "NFC Tag Type: RF (%d)", dev_info.interface);
          break;
        case NFC_DEV_INTERFACE_ISODEP:
          cli_trace(cli, "NFC Tag Type: ISO-DEP (%d)", dev_info.interface);
          break;
        case NFC_DEV_INTERFACE_NFCDEP:
          cli_trace(cli, "NFC Tag Type: NFC-DEP (%d)", dev_info.interface);
        break;
        default:
          cli_error(cli, CLI_ERROR_ABORT, "NFC Unexpected Tag Type (%d)",
                    dev_info.interface);
          goto cleanup;
      }

    } else if (event_flag == NFC_EVENT_DISCONNECTED) {
      cli_trace(cli, "NFC card removed.");
    }

    systick_delay_ms(1);
  }

  cli_ok(cli, "");

cleanup:
  nfc_stop_discovery();
}

static void prodtest_nfc_emulate_card(cli_t* cli) {
  uint32_t timeout = 0;
  bool timeout_set = false;

  if (cli_has_arg(cli, "timeout")) {
    if (!cli_arg_uint32(cli, "timeout", &timeout)) {
      cli_error_arg(cli, "Expecting timeout argument.");
      return;
    }
    timeout_set = true;
  }

  if (cli_arg_count(cli) > 1) {
    cli_error_arg_count(cli);
    return;
  }

  nfc_status_t ret = nfc_init();

  if (ret != NFC_OK) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC init failed");
    goto cleanup;
  } else {
    if (timeout_set) {
      cli_trace(cli, "Emulation started for %d ms", timeout);
    } else {
      cli_trace(cli, "Emulation started");
    }
  }

  nfc_register_tech(NFC_CARD_EMU_TECH_A);
  nfc_activate_stm();

  uint32_t expire_time = ticks_timeout(timeout);
  nfc_event_t nfc_event;

  while (!timeout_set || !ticks_expired(expire_time)) {
    nfc_status_t nfc_status = nfc_get_event(&nfc_event);

    if (nfc_status != NFC_OK) {
      cli_error(cli, CLI_ERROR, "NFC error");
      goto cleanup;
    }

    if (cli_aborted(cli)) {
      goto cleanup;
    }
    systick_delay_ms(1);
  }

  cli_trace(cli, "Emulation over");

  cli_ok(cli, "");

cleanup:
  nfc_deinit();
}

static void prodtest_nfc_write_card(cli_t* cli) {
  uint32_t timeout = 0;
  bool timeout_set = false;
  memset(&dev_info, 0, sizeof(dev_info));

  if (cli_has_arg(cli, "timeout")) {
    if (!cli_arg_uint32(cli, "timeout", &timeout)) {
      cli_error_arg(cli, "Expecting timeout argument.");
      return;
    }
    timeout_set = true;
  }

  if (cli_arg_count(cli) > 1) {
    cli_error_arg_count(cli);
    return;
  }

  nfc_status_t ret = nfc_init();

  if (ret != NFC_OK) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC init failed");
    goto cleanup;
  } else {
    if (timeout_set) {
      cli_trace(cli,
                "NFC reader on, put the card on the reader (timeout %d ms)",
                timeout);
    } else {
      cli_trace(cli, "NFC reader on, put the card on the reader");
    }
  }

  nfc_start_discovery();

  nfc_event_t nfc_event;
  uint32_t expire_time = ticks_timeout(timeout);

  while (true) {
    if (timeout_set && ticks_expired(expire_time)) {
      cli_error(cli, CLI_ERROR_TIMEOUT, "NFC timeout");
      goto cleanup;
    }

    nfc_status_t nfc_status = nfc_get_event(&nfc_event);

    if (nfc_status != NFC_OK) {
      cli_error(cli, CLI_ERROR_FATAL, "NFC error");
      goto cleanup;
    }

    if (nfc_event == NFC_DEVICE_CONNECTED) {
      nfc_dev_read_info(&dev_info);

      if (dev_info.type != NFC_DEV_TYPE_A) {
        cli_error(cli, CLI_ERROR_ABORT, "Only NFC type A cards supported");
        goto cleanup;
      }

      cli_trace(cli, "Writing URI to NFC tag %s", dev_info.uid);
      nfc_dev_write_ndef_uri();

      if (timeout_set) {
        nfc_dev_deactivate();
        cli_trace(cli, "NFC reader mode over");
        break;
      }

      systick_delay_ms(100);
      nfc_dev_deactivate();
    }

    if (cli_aborted(cli)) {
      goto cleanup;
    }

    systick_delay_ms(1);
  }

  cli_ok(cli, "");

cleanup:
  nfc_deinit();
}


// clang-format off

PRODTEST_CLI_CMD(
  .name = "nfc-read-card",
  .func = prodtest_nfc_read_card,
  .info = "Activate NFC in reader mode",
  .args = "[<timeout>]"
);

PRODTEST_CLI_CMD(
  .name = "nfc-emulate-card",
  .func = prodtest_nfc_emulate_card,
  .info = "Activate NFC in card emulation (CE) mode",
  .args = "[<timeout>]"
);

PRODTEST_CLI_CMD(
  .name = "nfc-write-card",
  .func = prodtest_nfc_write_card,
  .info = "Activate NFC in reader mode and write a URI to the attached card",
  .args = "[<timeout>]"
);

PRODTEST_CLI_CMD(
  .name = "nfc-send_apdu",
  .func = prodtest_nfc_send_apdu_cmd,
  .info = "Send testing APDU commands over NFC to discovered device",
  .args = "[<timeout>]"
);

#endif  // USE_NFC
