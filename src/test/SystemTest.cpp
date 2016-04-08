#include <SystemTest.h>


extern "C"{

uint32_t test_ble_gap_adv_data_set(Node* node, const uint8_t* p_data, uint8_t dlen, const uint8_t* p_sr_data, uint8_t srdlen)
{


	return 0;
}

uint32_t test_ble_gap_adv_stop(Node* node)
{


	return 0;
}

uint32_t test_ble_gap_adv_start(Node* node, const ble_gap_adv_params_t* p_adv_params)
{


	return 0;
}

uint32_t test_ble_gap_device_name_set(Node* node, const ble_gap_conn_sec_mode_t* p_write_perm, const uint8_t* p_dev_name, uint16_t len)
{


	return 0;
}

uint32_t test_ble_gap_appearance_set(Node* node, uint16_t appearance)
{


	return 0;
}

uint32_t test_ble_gap_ppcp_set(Node* node, const ble_gap_conn_params_t* p_conn_params)
{


	return 0;
}

uint32_t test_ble_gap_connect(Node* node, const ble_gap_addr_t* p_peer_addr, const ble_gap_scan_params_t* p_scan_params, const ble_gap_conn_params_t* p_conn_params)
{


	return 0;
}

uint32_t test_ble_gap_disconnect(Node* node, uint16_t conn_handle, uint8_t hci_status_code)
{


	return 0;
}

uint32_t test_ble_gap_sec_info_reply(Node* node, uint16_t conn_handle, const ble_gap_enc_info_t* p_enc_info, const ble_gap_irk_t* p_id_info, const ble_gap_sign_info_t* p_sign_info)
{


	return 0;
}

uint32_t test_ble_gap_encrypt(Node* node, uint16_t conn_handle, const ble_gap_master_id_t* p_master_id, const ble_gap_enc_info_t* p_enc_info)
{


	return 0;
}

uint32_t test_ble_gap_conn_param_update(Node* node, uint16_t conn_handle, const ble_gap_conn_params_t* p_conn_params)
{


	return 0;
}

uint32_t test_ble_uuid_vs_add(Node* node, const ble_uuid128_t* p_vs_uuid, uint8_t* p_uuid_type)
{


	return 0;
}

uint32_t test_ble_gatts_service_add(Node* node, uint8_t type, const ble_uuid_t* p_uuid, uint16_t* p_handle)
{


	return 0;
}

uint32_t test_ble_gatts_characteristic_add(Node* node, uint16_t service_handle, const ble_gatts_char_md_t* p_char_md, const ble_gatts_attr_t* p_attr_char_value, ble_gatts_char_handles_t* p_handles)
{


	return 0;
}

uint32_t test_ble_gatts_sys_attr_set(Node* node, uint16_t conn_handle, const uint8_t* p_sys_attr_data, uint16_t len, uint32_t flags)
{


	return 0;
}

uint32_t test_ble_gattc_primary_services_discover(Node* node, uint16_t conn_handle, uint16_t start_handle, const ble_uuid_t* p_srvc_uuid)
{


	return 0;
}

uint32_t test_ble_gattc_characteristics_discover(Node* node, uint16_t conn_handle, const ble_gattc_handle_range_t* p_handle_range)
{


	return 0;
}

uint32_t test_ble_gattc_write(Node* node, uint16_t conn_handle, const ble_gattc_write_params_t* p_write_params)
{


	return 0;
}

uint32_t test_ble_gap_scan_stop(Node* node)
{


	return 0;
}

uint32_t test_ble_gap_scan_start(Node* node, const ble_gap_scan_params_t* p_scan_params)
{


	return 0;
}

uint32_t test_ble_tx_packet_count_get(Node* node, uint16_t conn_handle, uint8_t* p_count)
{


	return 0;
}

uint32_t test_ble_gap_address_set(Node* node, uint8_t addr_cycle_mode, const ble_gap_addr_t* p_addr)
{


	return 0;
}

uint32_t test_ble_gap_address_get(Node* node, ble_gap_addr_t* p_addr)
{


	return 0;
}

uint32_t test_nvic_SystemReset(Node* node)
{


	return 0;
}

uint32_t test_ble_gap_rssi_start(Node* node, uint16_t conn_handle, uint8_t threshold_dbm, uint8_t skip_count)
{


	return 0;
}

uint32_t test_ble_gap_rssi_stop(Node* node, uint16_t conn_handle)
{


	return 0;
}

uint32_t test_power_dcdc_mode_set(Node* node, uint8_t dcdc_mode)
{


	return 0;
}

uint32_t test_power_mode_set(Node* node, uint8_t power_mode)
{


	return 0;
}

uint32_t test_flash_page_erase(Node* node, uint32_t page_number)
{


	return 0;
}

uint32_t test_flash_write(Node* node, uint32_t* const p_dst, const uint32_t* const p_src, uint32_t size)
{


	return 0;
}

uint32_t test_rand_application_vector_get(Node* node, uint8_t* p_buff, uint8_t length)
{
	memset(p_buff, 0x12, length);

	return 0;
}

uint32_t test_ble_evt_get(Node* node, uint8_t* p_dest, uint16_t* p_len)
{


	return 2;
}

uint32_t test_app_evt_wait(Node* node)
{


	return 0;
}

uint32_t test_nvic_ClearPendingIRQ(Node* node, IRQn_Type IRQn)
{


	return 0;
}

uint32_t test_ble_enable(Node* node, ble_enable_params_t* p_ble_enable_params, uint32_t* p_app_ram_base)
{


	return 0;
}

uint32_t test_ble_gap_tx_power_set(Node* node, int8_t tx_power)
{


	return 0;
}

uint32_t test_nvic_EnableIRQ(Node* node, IRQn_Type IRQn)
{

	return 0;
}

uint32_t test_nvic_SetPriority(Node* node, IRQn_Type IRQn, uint32_t priority)
{

	return 0;
}

uint32_t test_radio_notification_cfg_set(Node* node, uint8_t type, uint8_t distance)
{

	return 0;
}

uint32_t test_softdevice_enable(Node* node, nrf_clock_lf_cfg_t* clock_source, uint8_t* unsure)
{

	return 0;
}

uint32_t test_evt_get(Node* node, uint32_t* evt_id)
{

	return 0;
}



}
