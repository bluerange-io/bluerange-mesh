/*
 * sstem_test.h
 *
 *  Created on: 01.02.2016
 *      Author: Marius Heil
 */
#ifndef SYSTEMTEST_H_
#define SYSTEMTEST_H_

#ifdef _MSC_VER
#ifndef __ASM
#define __ASM               __asm
#endif

#ifndef __INLINE
#define __INLINE            
#endif

#ifndef __WEAK
#define __WEAK              
#endif

#ifndef __ALIGN
#define __ALIGN(n)          __align(n)
#endif

#define GET_SP()                __current_sp()

#define STACK_BASE 0 //FIXME: Should point to stack base
#define STACK_TOP 0 //FIXME: should point to stack top

#define _CRT_SECURE_NO_WARNINGS 1
#endif


#ifdef SIM_ENABLED

//Define this if Stdio is implemented via pipes
#define USE_SIM_PIPE

//FIXME: This was used to wrap some problematic areas in the code that could not be compiled by VS
#define SIM_PROBLEM

#define BLE_STACK_SUPPORT_REQD
#define DEBUG

//This header file must be included in the build settings before every other include file (C and C++ Build Settings)
//It will define the SVCALL macro to erase function declarations from the nordic header files
//It will then declare special test_ functions that are implemented in SystemTest.c

void SystemTestInit();

//This define erases all sd_ declarations from the nordic header files
#define SVCALL(number, return_type, signature)

#define NRF_UART_BAUDRATE_115200 (115200)
#define NRF_UART_BAUDRATE_460800 (460800)
#define NRF_UART_BAUDRATE_38400 (38400)

#define UART_BAUDRATE_BAUDRATE_Baud38400 38400

//GPIO defines, FIXME: these are not correct, do they need to be?
#define GPIO_PIN_CNF_DIR_Output 1
#define GPIO_PIN_CNF_INPUT_Disconnect 1
#define GPIO_PIN_CNF_PULL_Disabled 1
#define GPIO_PIN_CNF_DRIVE_S0S1 1
#define GPIO_PIN_CNF_SENSE_Disabled 1

#define GPIO_PIN_CNF_SENSE_Pos 1
#define GPIO_PIN_CNF_DRIVE_Pos 1
#define GPIO_PIN_CNF_PULL_Pos 1
#define GPIO_PIN_CNF_INPUT_Pos 1
#define GPIO_PIN_CNF_DIR_Pos 1

#define SD_EVT_IRQn 1


//Some config stuff
//#define ENABLE_LOGGING
#define USE_STDIO


#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

#include <sdk_common.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#include <nrf_sdm.h>
#include <ble.h>

typedef struct Node Node;
typedef struct GlobalState GlobalState;

typedef uint32_t IRQn_Type;

typedef uint32_t nrf_drv_gpiote_pin_t;
typedef uint32_t nrf_gpiote_polarity_t;

typedef uint32_t nrf_uart_baudrate_t;

typedef struct {                                    /*!< FICR Structure                                                        */
  uint32_t  RESERVED0[4];
  uint32_t  CODEPAGESIZE;                      /*!< Code memory page size in bytes.                                       */
  uint32_t  CODESIZE;                          /*!< Code memory size in pages.                                            */
  uint32_t  RESERVED1[4];
  uint32_t  CLENR0;                            /*!< Length of code region 0 in bytes.                                     */
  uint32_t  PPFC;                              /*!< Pre-programmed factory code present.                                  */
  uint32_t  RESERVED2;
  uint32_t  NUMRAMBLOCK;                       /*!< Number of individualy controllable RAM blocks.                        */

  union {
    uint32_t  SIZERAMBLOCK[4];                 /*!< Deprecated array of size of RAM block in bytes. This name is
                                                         kept for backward compatinility purposes. Use SIZERAMBLOCKS
                                                          instead.                                                             */
    uint32_t  SIZERAMBLOCKS;                   /*!< Size of RAM blocks in bytes.                                          */
  };
  uint32_t  RESERVED3[5];
  uint32_t  CONFIGID;                          /*!< Configuration identifier.                                             */
  uint32_t  DEVICEID[2];                       /*!< Device identifier.                                                    */
  uint32_t  RESERVED4[6];
  uint32_t  ER[4];                             /*!< Encryption root.                                                      */
  uint32_t  IR[4];                             /*!< Identity root.                                                        */
  uint32_t  DEVICEADDRTYPE;                    /*!< Device address type.                                                  */
  uint32_t  DEVICEADDR[2];                     /*!< Device address.                                                       */
  uint32_t  OVERRIDEEN;                        /*!< Radio calibration override enable.                                    */
  uint32_t  NRF_1MBIT[5];                      /*!< Override values for the OVERRIDEn registers in RADIO for NRF_1Mbit
                                                         mode.                                                                 */
  uint32_t  RESERVED5[10];
  uint32_t  BLE_1MBIT[5];                      /*!< Override values for the OVERRIDEn registers in RADIO for BLE_1Mbit
                                                         mode.                                                                 */
} NRF_FICR_Type;

typedef struct {                                    /*!< UICR Structure                                                        */
   uint32_t  CLENR0;                            /*!< Length of code region 0.                                              */
   uint32_t  RBPCONF;                           /*!< Readback protection configuration.                                    */
   uint32_t  XTALFREQ;                          /*!< Reset value for CLOCK XTALFREQ register.                              */
  uint32_t  RESERVED0;
  uint32_t  FWID;                              /*!< Firmware ID.                                                          */

  union {
     uint32_t  NRFFW[15];                       /*!< Reserved for Nordic firmware design.                                  */
     uint32_t  BOOTLOADERADDR;                  /*!< Bootloader start address.                                             */
  };
   uint32_t  NRFHW[12];                         /*!< Reserved for Nordic hardware design.                                  */
   uint32_t  CUSTOMER[32];                      /*!< Reserved for customer.                                                */
} NRF_UICR_Type;

typedef struct {                                    /*!< GPIO Structure                                                        */
  uint32_t  RESERVED0[321];
   uint32_t  OUT;                               /*!< Write GPIO port.                                                      */
   uint32_t  OUTSET;                            /*!< Set individual bits in GPIO port.                                     */
   uint32_t  OUTCLR;                            /*!< Clear individual bits in GPIO port.                                   */
  uint32_t  IN;                                /*!< Read GPIO port.                                                       */
   uint32_t  DIR;                               /*!< Direction of GPIO pins.                                               */
   uint32_t  DIRSET;                            /*!< DIR set register.                                                     */
   uint32_t  DIRCLR;                            /*!< DIR clear register.                                                   */
  uint32_t  RESERVED1[120];
   uint32_t  PIN_CNF[32];                       /*!< Configuration of GPIO pins.                                           */
} NRF_GPIO_Type;


#define SIM_PAGE_SIZE 1024
#define SIM_PAGES 256

//We need to redefine the macro that calculates the sizes of MasterBootRecord, Softddevice,...
#define MBR_SIZE (1024*4)
#undef SD_SIZE_GET
#define SD_SIZE_GET(A) (1024*110)

#define UNITS_TO_MSEC(TIME, RESOLUTION) (((TIME) * ((u32)RESOLUTION)) / 1000)

#define BOOTLOADER_ADDRESS (NRF_UICR->BOOTLOADERADDR)

extern NRF_FICR_Type* myFicrPtr;
extern NRF_UICR_Type* myUicrPtr;
extern NRF_GPIO_Type* myGpioPtr;
extern uint8_t* myFlashPtr;
extern GlobalState* myGlobalStatePtr;

extern int globalBreakCounter; //Can be used to increment globally everywhere in sim and break on a specific count

#define NRF_FICR myFicrPtr
#define NRF_UICR myUicrPtr
#define NRF_GPIO myGpioPtr
#define GS myGlobalStatePtr

#define FLASH_REGION_START_ADDRESS ((u32)myFlashPtr)

uint32_t sd_ble_gap_adv_data_set(uint8_t const *p_data, uint8_t dlen, uint8_t const *p_sr_data, uint8_t srdlen);
uint32_t sd_ble_gap_adv_stop();
uint32_t sd_ble_gap_adv_start(ble_gap_adv_params_t const *p_adv_params);
uint32_t sd_ble_gap_device_name_set(ble_gap_conn_sec_mode_t const *p_write_perm, uint8_t const *p_dev_name, uint16_t len);
uint32_t sd_ble_gap_appearance_set(uint16_t appearance);
uint32_t sd_ble_gap_ppcp_set(ble_gap_conn_params_t const *p_conn_params);
uint32_t sd_ble_gap_connect(ble_gap_addr_t const *p_peer_addr, ble_gap_scan_params_t const *p_scan_params, ble_gap_conn_params_t const *p_conn_params);
uint32_t sd_ble_gap_disconnect(uint16_t conn_handle, uint8_t hci_status_code);
uint32_t sd_ble_gap_sec_info_reply(uint16_t conn_handle, ble_gap_enc_info_t const *p_enc_info, ble_gap_irk_t const *p_id_info, ble_gap_sign_info_t const *p_sign_info);
uint32_t sd_ble_gap_encrypt(uint16_t conn_handle, ble_gap_master_id_t const *p_master_id, ble_gap_enc_info_t const *p_enc_info);
uint32_t sd_ble_gap_conn_param_update(uint16_t conn_handle, ble_gap_conn_params_t const *p_conn_params);
uint32_t sd_ble_uuid_vs_add(ble_uuid128_t const *p_vs_uuid, uint8_t *p_uuid_type);
uint32_t sd_ble_gatts_service_add(uint8_t type, ble_uuid_t const *p_uuid, uint16_t *p_handle);
uint32_t sd_ble_gatts_characteristic_add(uint16_t service_handle, ble_gatts_char_md_t const *p_char_md, ble_gatts_attr_t const *p_attr_char_value, ble_gatts_char_handles_t *p_handles);
uint32_t sd_ble_gatts_sys_attr_set(uint16_t conn_handle, uint8_t const *p_sys_attr_data, uint16_t len, uint32_t flags);
uint32_t sd_ble_gattc_primary_services_discover(uint16_t conn_handle, uint16_t start_handle, ble_uuid_t const *p_srvc_uuid);
uint32_t sd_ble_gattc_characteristics_discover(uint16_t conn_handle, ble_gattc_handle_range_t const *p_handle_range);
uint32_t sd_ble_gattc_write(uint16_t conn_handle, ble_gattc_write_params_t const *p_write_params);
uint32_t sd_ble_gap_scan_stop();
uint32_t sd_ble_gap_scan_start(ble_gap_scan_params_t const *p_scan_params);
uint32_t sd_ble_tx_packet_count_get(uint16_t conn_handle, uint8_t *p_count);
uint32_t sd_ble_gap_disconnect(uint16_t conn_handle, uint8_t hci_status_code);
uint32_t sd_ble_gap_address_set(uint8_t addr_cycle_mode, ble_gap_addr_t const *p_addr);
uint32_t sd_ble_gap_address_get(ble_gap_addr_t *p_addr);
uint32_t sd_nvic_SystemReset();
uint32_t sd_ble_gap_rssi_start(uint16_t conn_handle, uint8_t threshold_dbm, uint8_t skip_count);
uint32_t sd_ble_gap_rssi_stop(uint16_t conn_handle);
uint32_t sd_power_dcdc_mode_set(uint8_t dcdc_mode);
uint32_t sd_power_mode_set(uint8_t power_mode);
uint32_t sd_flash_page_erase(uint32_t page_number);
uint32_t sd_flash_write(uint32_t * const p_dst, uint32_t const * const p_src, uint32_t size);
uint32_t sd_rand_application_vector_get(uint8_t * p_buff, uint8_t length);
uint32_t sd_ble_evt_get(uint8_t *p_dest, uint16_t *p_len);
uint32_t sd_app_evt_wait();
uint32_t sd_nvic_ClearPendingIRQ(IRQn_Type IRQn);
uint32_t sd_ble_enable(ble_enable_params_t * p_ble_enable_params, uint32_t * p_app_ram_base);
uint32_t sd_ble_gap_tx_power_set(int8_t tx_power);
uint32_t sd_nvic_EnableIRQ(IRQn_Type IRQn);
uint32_t sd_nvic_SetPriority(IRQn_Type IRQn, uint32_t priority);
uint32_t sd_evt_get(uint32_t* evt_id);
uint32_t sd_ble_gatts_hvx(uint16_t conn_handle, ble_gatts_hvx_params_t const *p_hvx_params);
uint32_t sd_ecb_block_encrypt(nrf_ecb_hal_data_t * p_ecb_data);
uint32_t sd_ble_opt_set(uint32_t opt_id, ble_opt_t const *p_opt);
uint32_t sd_power_reset_reason_clr(uint32_t p);


uint32_t app_timer_cnt_get(uint32_t* time);
uint32_t app_timer_cnt_diff_compute(uint32_t previousTime, uint32_t nowTime, uint32_t* passedTime);
typedef void (*ble_radio_notification_evt_handler_t) (bool radio_active);
uint32_t ble_radio_notification_init(uint32_t irq_priority,
                                     uint8_t    distance,
                                     ble_radio_notification_evt_handler_t evt_handler);

void nrf_delay_us(uint32_t usec);

void sim_commit_flash_operations();
void sim_commit_some_flash_operations(uint8_t* failData, uint16_t numMaxEvents);

#ifdef __cplusplus
}
#endif


#endif
#endif /* SYSTEMTEST_H_ */
