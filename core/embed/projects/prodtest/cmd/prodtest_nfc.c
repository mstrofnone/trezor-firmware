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
#include <sys/systick.h>
#include "rfal_nfc.h"
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

  nfc_status_t ret = nfc_init();

  if (ret != NFC_OK) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC init failed");
    goto cleanup;
  } else {
    if (timeout_set) {
      cli_trace(cli, "NFC activated in reader mode for %d ms.", timeout);
    } else {
      cli_trace(cli, "NFC activated in reader mode");
    }
  }

  if (NFC_OK != nfc_register_tech(NFC_POLLER_TECH_A | NFC_POLLER_TECH_B |
                                  NFC_POLLER_TECH_F | NFC_POLLER_TECH_V)) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC tech registration failed");
    goto cleanup;
  }

  if (NFC_OK != nfc_activate_stm()) {
    cli_error(cli, CLI_ERROR_FATAL, "NFC activation failed");
    goto cleanup;
  }

  nfc_event_t nfc_event;
  uint32_t expire_time = ticks_timeout(timeout);

  while (true) {
    if (timeout_set && ticks_expired(expire_time)) {
      cli_error(cli, CLI_ERROR_TIMEOUT, "NFC timeout");
      goto cleanup;
    }

    nfc_status_t nfc_status = nfc_get_event(&nfc_event);

    if (nfc_status != NFC_OK) {
      cli_error(cli, CLI_ERROR, "NFC error");
      goto cleanup;
    }

    if (nfc_event == NFC_EVENT_ACTIVATED) {
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

      if (timeout_set) {
        nfc_dev_deactivate();
        cli_trace(cli, "NFC reader mode over");
        break;
      }

      systick_delay_ms(100);
      nfc_dev_deactivate();
    } else if (nfc_event == NFC_EVENT_DEACTIVATED) {
      cli_trace(cli, "NFC card removed.");
    } else if (nfc_event == NFC_EVENT_ACTIVATED_NFCA_T4T) {
      cli_trace(cli, "NFC Type A T4T tag activated: UID: %s", dev_info.uid);

      rfalIsoDepApduBufFormat apdu_buffer;
      uint16_t apdu_buffer_len;
      ReturnCode ret_code = rfalT4TPollerComposeSelectAppl(
          &apdu_buffer, &aid_select_ndef_app[0], sizeof(aid_select_ndef_app),
          &apdu_buffer_len);

      if (ret_code == RFAL_ERR_NONE) {
        uint8_t* apdu_resp = NULL;
        uint16_t* apdu_resp_len = NULL;
        nfc_status =
            nfc_transceive((const uint8_t*)&apdu_buffer.apdu, apdu_buffer_len,
                           &apdu_resp, &apdu_resp_len);

        cli_trace(cli, "nfc_transceive: stat=%d / 0x%02X 0x%02X, len=%d",
                  nfc_status, apdu_resp[0], apdu_resp[1], *apdu_resp_len);
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

  nfc_register_tech(NFC_POLLER_TECH_A | NFC_POLLER_TECH_B | NFC_POLLER_TECH_F |
                    NFC_POLLER_TECH_V);
  nfc_activate_stm();

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

    if (nfc_event == NFC_EVENT_ACTIVATED) {
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

static void prodtest_nfc_send_apdu_cmd(cli_t* cli) {
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

  nfc_register_tech(NFC_POLLER_TECH_A | NFC_POLLER_TECH_B | NFC_POLLER_TECH_F |
                    NFC_POLLER_TECH_V);
  nfc_activate_stm();

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

    if (nfc_event == NFC_EVENT_ACTIVATED) {
      nfc_dev_read_info(&dev_info);

      if (dev_info.type != NFC_DEV_TYPE_A) {
        cli_error(cli, CLI_ERROR_ABORT, "Only NFC type A cards supported");
        goto cleanup;
      }

      cli_trace(cli, "NFC tag %s", dev_info.uid);

      // cli_trace(cli, "Writing URI to NFC tag %s", dev_info.uid);
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
