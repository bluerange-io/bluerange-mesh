/*
 * sstem_test.h
 *
 *  Created on: 01.02.2016
 *      Author: Marius Heil
 */

#ifndef SYSTEMTEST_H_
#define SYSTEMTEST_H_

//This header file must be included in the build settings before every other include file (C and C++ Build Settings)
//It will define the SVCALL macro to erase function declarations from the nordic header files
//It will then declare special test_ functions that are implemented in SystemTest.c


//This define erases all sd_ declarations from the nordic header files
#define SVCALL(number, return_type, signature)

#define sd_ble_gap_adv_data_set(A, B, C, D) test_ble_gap_adv_data_set(NULL, A, B, C, D)
#define sd_ble_gap_adv_start(A) test_ble_gap_adv_start(NULL, A)
#define sd_ble_gap_adv_stop(A) test_ble_gap_adv_stop(NULL)
#define sd_ble_gap_device_name_set(A, B, C) test_ble_gap_device_name_set(NULL, A, B, C)
#define sd_ble_gap_appearance_set(A) test_ble_gap_appearance_set(NULL, A)
#define sd_ble_gap_ppcp_set(A) test_ble_gap_ppcp_set(NULL, A)
#define sd_ble_gap_connect(A, B, C) test_ble_gap_connect(NULL, A, B, C)
#define sd_ble_gap_disconnect(A, B) test_ble_gap_disconnect(NULL, A, B)
#define sd_ble_gap_sec_info_reply(A, B, C, D) test_ble_gap_sec_info_reply(NULL, A, B, C, D)
#define sd_ble_gap_encrypt(A, B, C) test_ble_gap_encrypt(NULL, A, B, C)
#define sd_ble_gap_conn_param_update(A, B) test_ble_gap_conn_param_update(NULL, A, B)
#define sd_ble_uuid_vs_add(A, B) test_ble_uuid_vs_add(NULL, A, B)
#define sd_ble_gatts_service_add(A, B, C) test_ble_gatts_service_add(NULL, A, B, C)
#define sd_ble_gatts_characteristic_add(A, B, C, D) test_ble_gatts_characteristic_add(NULL, A, B, C, D)
#define sd_ble_gatts_sys_attr_set(A, B, C, D) test_ble_gatts_sys_attr_set(NULL, A, B, C, D)
#define sd_ble_gattc_primary_services_discover(A, B, C) test_ble_gattc_primary_services_discover(NULL, A, B, C)
#define sd_ble_gattc_characteristics_discover(A, B) test_ble_gattc_characteristics_discover(NULL, A, B)
#define sd_ble_gattc_write(A, B) test_ble_gattc_write(NULL, A, B)
#define sd_ble_gap_scan_start(A) test_ble_gap_scan_start(NULL, A)
#define sd_ble_gap_scan_stop(A) test_ble_gap_scan_stop(NULL)
#define sd_ble_tx_packet_count_get(A, B) test_ble_tx_packet_count_get(NULL, A, B)
#define sd_ble_gap_disconnect(A, B) test_ble_gap_disconnect(NULL, A, B)
#define sd_ble_gap_address_set(A, B) test_ble_gap_address_set(NULL, A, B)
#define sd_ble_gap_address_get(A) test_ble_gap_address_get(NULL, A)
#define sd_nvic_SystemReset(A) test_nvic_SystemReset(NULL)
#define sd_ble_gap_rssi_start(A, B, C) test_ble_gap_rssi_start(NULL, A, B, C);
#define sd_ble_gap_rssi_stop(A) test_ble_gap_rssi_stop(NULL, A)
#define sd_power_dcdc_mode_set(A) test_power_dcdc_mode_set(NULL, A)
#define sd_power_mode_set(A) test_power_mode_set(NULL, A)
#define sd_flash_page_erase(A) test_flash_page_erase(NULL, A)
#define sd_flash_write(A, B, C) test_flash_write(NULL, A, B, C)
#define sd_rand_application_vector_get(A, B) test_rand_application_vector_get(NULL, A, B)
#define sd_ble_evt_get(A, B) test_ble_evt_get(NULL, A, B)
#define sd_app_evt_wait(A) test_app_evt_wait(NULL)
#define sd_nvic_ClearPendingIRQ(A) test_nvic_ClearPendingIRQ(NULL, A)
#define sd_ble_enable(A, B) test_ble_enable(NULL, A, B)
#define sd_ble_gap_tx_power_set(A) test_ble_gap_tx_power_set(NULL, A)


#define sd_softdevice_enable(A, B) test_softdevice_enable(NULL, A, B)
#define sd_nvic_EnableIRQ(A) test_nvic_EnableIRQ(NULL, A)
#define sd_nvic_SetPriority(A, B) test_nvic_SetPriority(NULL, A, B)
#define sd_radio_notification_cfg_set(A, B) test_radio_notification_cfg_set(NULL, A, B)
#define sd_evt_get(A) test_evt_get(NULL, A)



#ifdef __cplusplus
extern "C"{
#endif

#include <sdk_common.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#if defined(NRF51)
#include <nrf51.h>
#elif defined(NRF52)
#include <nrf52.h>
#endif
#include <nrf_sdm.h>
#include <ble.h>

typedef struct Node Node;

uint32_t test_ble_gap_adv_data_set(Node* node, uint8_t const *p_data, uint8_t dlen, uint8_t const *p_sr_data, uint8_t srdlen);
uint32_t test_ble_gap_adv_stop(Node* node);
uint32_t test_ble_gap_adv_start(Node* node, ble_gap_adv_params_t const *p_adv_params);
uint32_t test_ble_gap_device_name_set(Node* node, ble_gap_conn_sec_mode_t const *p_write_perm, uint8_t const *p_dev_name, uint16_t len);
uint32_t test_ble_gap_appearance_set(Node* node, uint16_t appearance);
uint32_t test_ble_gap_ppcp_set(Node* node, ble_gap_conn_params_t const *p_conn_params);
uint32_t test_ble_gap_connect(Node* node, ble_gap_addr_t const *p_peer_addr, ble_gap_scan_params_t const *p_scan_params, ble_gap_conn_params_t const *p_conn_params);
uint32_t test_ble_gap_disconnect(Node* node, uint16_t conn_handle, uint8_t hci_status_code);
uint32_t test_ble_gap_sec_info_reply(Node* node, uint16_t conn_handle, ble_gap_enc_info_t const *p_enc_info, ble_gap_irk_t const *p_id_info, ble_gap_sign_info_t const *p_sign_info);
uint32_t test_ble_gap_encrypt(Node* node, uint16_t conn_handle, ble_gap_master_id_t const *p_master_id, ble_gap_enc_info_t const *p_enc_info);
uint32_t test_ble_gap_conn_param_update(Node* node, uint16_t conn_handle, ble_gap_conn_params_t const *p_conn_params);
uint32_t test_ble_uuid_vs_add(Node* node, ble_uuid128_t const *p_vs_uuid, uint8_t *p_uuid_type);
uint32_t test_ble_gatts_service_add(Node* node, uint8_t type, ble_uuid_t const *p_uuid, uint16_t *p_handle);
uint32_t test_ble_gatts_characteristic_add(Node* node, uint16_t service_handle, ble_gatts_char_md_t const *p_char_md, ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles);
uint32_t test_ble_gatts_sys_attr_set(Node* node, uint16_t conn_handle, uint8_t const *p_sys_attr_data, uint16_t len, uint32_t flags);
uint32_t test_ble_gattc_primary_services_discover(Node* node, uint16_t conn_handle, uint16_t start_handle, ble_uuid_t const *p_srvc_uuid);
uint32_t test_ble_gattc_characteristics_discover(Node* node, uint16_t conn_handle, ble_gattc_handle_range_t const *p_handle_range);
uint32_t test_ble_gattc_write(Node* node, uint16_t conn_handle, ble_gattc_write_params_t const *p_write_params);
uint32_t test_ble_gap_scan_stop(Node* node);
uint32_t test_ble_gap_scan_start(Node* node, ble_gap_scan_params_t const *p_scan_params);
uint32_t test_ble_tx_packet_count_get(Node* node, uint16_t conn_handle, uint8_t *p_count);
uint32_t test_ble_gap_disconnect(Node* node, uint16_t conn_handle, uint8_t hci_status_code);
uint32_t test_ble_gap_address_set(Node* node, uint8_t addr_cycle_mode, ble_gap_addr_t const *p_addr);
uint32_t test_ble_gap_address_get(Node* node, ble_gap_addr_t *p_addr);
uint32_t test_nvic_SystemReset(Node* node);
uint32_t test_ble_gap_rssi_start(Node* node, uint16_t conn_handle, uint8_t threshold_dbm, uint8_t skip_count);
uint32_t test_ble_gap_rssi_stop(Node* node, uint16_t conn_handle);
uint32_t test_power_dcdc_mode_set(Node* node, uint8_t dcdc_mode);
uint32_t test_power_mode_set(Node* node, uint8_t power_mode);
uint32_t test_flash_page_erase(Node* node, uint32_t page_number);
uint32_t test_flash_write(Node* node, uint32_t * const p_dst, uint32_t const * const p_src, uint32_t size);
uint32_t test_rand_application_vector_get(Node* node, uint8_t * p_buff, uint8_t length);
uint32_t test_ble_evt_get(Node* node, uint8_t *p_dest, uint16_t *p_len);
uint32_t test_app_evt_wait(Node* node);
uint32_t test_nvic_ClearPendingIRQ(Node* node, IRQn_Type IRQn);
uint32_t test_ble_enable(Node* node, ble_enable_params_t * p_ble_enable_params, uint32_t * p_app_ram_base);
uint32_t test_ble_gap_tx_power_set(Node* node, int8_t tx_power);


uint32_t test_nvic_EnableIRQ(Node* node, IRQn_Type IRQn);
uint32_t test_nvic_SetPriority(Node* node, IRQn_Type IRQn, uint32_t priority);
uint32_t test_radio_notification_cfg_set(Node* node, uint8_t type, uint8_t distance);

uint32_t test_softdevice_enable(Node* node, nrf_clock_lf_cfg_t* clock_source, uint8_t* unsure);
uint32_t test_evt_get(Node* node, uint32_t* evt_id);
#ifdef __cplusplus
}
#endif


#endif /* SYSTEMTEST_H_ */
