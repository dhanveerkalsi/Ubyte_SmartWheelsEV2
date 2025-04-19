/*
 * SmartWheels EV2 Initialization Code
 * Peripherals covered:
 *  1. I2C - Scans available devices from 0x0 to 0xff
 *  2. CAN2.0 Standard - 250kbps
 *     Transmits frames with ID 0x0 to 0xff and data from 0xff to 0x0
 *  3. Digital Input - 2 User buttons
 *  4. Digital Output - RGB LED
 *  5. Analog Input - Potentiometer
 *  6. UART - Sending Data
 *     Prints ADC, Button Input, and I2C scan Data
 */

#include "sdk_project_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* LED Pin Definitions */
#define LED_PORT  (PTD)
#define LED_RED   (15u)
#define LED_GREEN (16u)
#define LED_BLUE  (0u)

/* ADC Configuration */
#define ADC_MAX_VALUE   ((1 << 12) - 1)
#define ADC_VBG_VALUE   (1000)
#define ADC_CHANNEL     (0UL)
#define ADC_BAR_LENGTH  32
#define ADC_HYSTERESIS  2

/* CAN Configuration */
#define TX_MAILBOX  (1UL)
#define RX_MAILBOX  (0UL)

/* Function Prototypes */
void serial_uart_send(const char *format, ...);
void uint32_to_bin32(uint32_t u, char *arr32bit);
void uint32_to_bool_array(uint32_t u, bool *arr32bit);
void dig_read(void);
void CreateGraph(uint16_t value, uint8_t *result, uint8_t graphLen);
uint16_t getVrefValue(void);
void adc_read(void);
void SendCANData(uint32_t mailbox, uint32_t messageId, uint8_t *data, uint32_t len);

/* Global Variables */
volatile int exit_code = 0;
volatile uint16_t extVref_mV = 0;

/* UART Send Function */
void serial_uart_send(const char *format, ...) {
	char buffer[512] = { 0 };
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	uint32_t len = strlen(buffer);
	buffer[len] = '\0';
	LPUART_DRV_SendDataBlocking(INST_LPUART_1, (uint8_t*) buffer, len, OSIF_WAIT_FOREVER);
}

/* Convert uint32_t to Binary String */
void uint32_to_bin32(uint32_t u, char *arr32bit) {
	for (int i = 31; i >= 0; i--) {
		arr32bit[i] = (u & 1) + '0';
		u >>= 1;
	}
}

/* Convert uint32_t to Boolean Array */
void uint32_to_bool_array(uint32_t u, bool *arr32bit) {
	for (int i = 31; i >= 0; i--) {
		arr32bit[31 - i] = (u & 1);
		u >>= 1;
	}
}

/* Read Digital Input (Buttons) */
void dig_read(void) {
	bool arr32bool[32] = { 0 };
	uint32_to_bool_array(PINS_DRV_ReadPins(PTC), arr32bool);

	uint8_t button1 = arr32bool[12];
	uint8_t button2 = arr32bool[13];
	serial_uart_send("Button %u : %s\r\n", 1, button1 ? "IDLE" : "PRESSED");
	serial_uart_send("Button %u : %s\r\n", 2, button2 ? "IDLE" : "PRESSED");
}

/* Create ADC Bar Graph */
void CreateGraph(uint16_t value, uint8_t *result, uint8_t graphLen) {
	uint16_t numberOfDots = (value * graphLen) / ADC_MAX_VALUE;
	for (uint8_t i = 0; i < graphLen; i++) {
		result[i] = (i < numberOfDots) ? '#' : '-';
	}
}

/* Measure Vref Value */
uint16_t getVrefValue(void) {
	uint16_t Vbg_measured_with_VrefAlt;
	uint16_t VrefAltH_measured_with_Vbg;
	uint16_t VrefAltL_measured_with_Vbg;
	uint16_t Vref, VrefH, VrefL;

	ADC_DRV_Reset(INST_ADC_CONFIG_1);
	ADC_DRV_ConfigConverter(INST_ADC_CONFIG_1, &adc_config_1_ConvConfig0);
	ADC_DRV_ConfigChan(INST_ADC_CONFIG_1, ADC_CHANNEL, &adc_config_1_ChnConfig3);
	ADC_DRV_WaitConvDone(INST_ADC_CONFIG_1);
	ADC_DRV_GetChanResult(INST_ADC_CONFIG_1, ADC_CHANNEL, &Vbg_measured_with_VrefAlt);

	if (Vbg_measured_with_VrefAlt < ADC_MAX_VALUE) {
		return (uint16_t) ((ADC_MAX_VALUE * ADC_VBG_VALUE) / Vbg_measured_with_VrefAlt);
	} else {
		ADC_DRV_ConfigChan(INST_ADC_CONFIG_1, ADC_CHANNEL, &adc_config_1_ChnConfig1);
		ADC_DRV_WaitConvDone(INST_ADC_CONFIG_1);
		ADC_DRV_GetChanResult(INST_ADC_CONFIG_1, ADC_CHANNEL, &VrefAltH_measured_with_Vbg);

		ADC_DRV_ConfigChan(INST_ADC_CONFIG_1, ADC_CHANNEL, &adc_config_1_ChnConfig2);
		ADC_DRV_WaitConvDone(INST_ADC_CONFIG_1);
		ADC_DRV_GetChanResult(INST_ADC_CONFIG_1, ADC_CHANNEL, &VrefAltL_measured_with_Vbg);

		VrefH = (uint16_t) ((ADC_MAX_VALUE * ADC_VBG_VALUE) / VrefAltH_measured_with_Vbg);
		VrefL = (uint16_t) ((ADC_MAX_VALUE * ADC_VBG_VALUE) / VrefAltL_measured_with_Vbg);
		(void) VrefH;
		(void) VrefL;
		return (VrefH - VrefL);
	}
}

/* Read ADC Value */
void adc_read(void) {
	uint16_t loc_conversionResult = 0;
	ADC_DRV_ConfigChan(INST_ADC_CONFIG_1, 0, &adc_config_1_ChnConfig0);
	ADC_DRV_WaitConvDone(INST_ADC_CONFIG_1);
	ADC_DRV_GetChanResult(INST_ADC_CONFIG_1, 0, &loc_conversionResult);

	uint16_t g_vin = (extVref_mV * loc_conversionResult) / ADC_MAX_VALUE;
	uint8_t barbuff[ADC_BAR_LENGTH + 1];
	CreateGraph(loc_conversionResult, barbuff, ADC_BAR_LENGTH);

	serial_uart_send("ADC%1u-CH%02u [%s] Vin=%4umV Raw=%4u\n\r", INST_ADC_CONFIG_1, adc_config_1_ChnConfig0.channel, barbuff, g_vin, loc_conversionResult);
}

/* Send CAN Data */
void SendCANData(uint32_t mailbox, uint32_t messageId, uint8_t *data, uint32_t len) {
	flexcan_data_info_t dataInfo = {
			.data_length = len,
			.msg_id_type = FLEXCAN_MSG_ID_STD,
			.enable_brs = true,
			.fd_enable = true,
			.fd_padding = 0U
	};

	FLEXCAN_DRV_ConfigTxMb(INST_FLEXCAN_CONFIG_1, mailbox, &dataInfo, messageId);
	FLEXCAN_DRV_Send(INST_FLEXCAN_CONFIG_1, mailbox, &dataInfo, messageId, data);
}

/* Main Function */
int main(void) {
	CLOCK_DRV_Init(&clockMan1_InitConfig0);
	LPUART_DRV_Init(INST_LPUART_1, &lpUartState0, &lpuart_1_InitConfig0);

	extVref_mV = getVrefValue();
	ADC_DRV_ConfigConverter(INST_ADC_CONFIG_1, &adc_config_1_ConvConfig0);
	ADC_DRV_AutoCalibration(INST_ADC_CONFIG_1);
	ADC_DRV_ConfigHwAverage(INST_ADC_CONFIG_1, &adc_config_1_HwAvgConfig0);

	PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
	FLEXCAN_DRV_Init(INST_FLEXCAN_CONFIG_1, &flexcanState0, &flexcanInitConfig0);

	lpi2c_master_state_t lpi2c1MasterState;
	LPI2C_DRV_MasterInit(INST_LPI2C0, &lpi2c0_MasterConfig0, &lpi2c1MasterState);

	while (1) {
		for (int var = 0; var < 128; var++) {
			uint8_t masterTxBuffer = 0;
			uint8_t masterRxBuffer = 0;
			LPI2C_DRV_MasterSetSlaveAddr(INST_LPI2C0, var, 0);
			LPI2C_DRV_MasterSendDataBlocking(INST_LPI2C0, &masterTxBuffer, 1, true, 50);
			status_t status = LPI2C_DRV_MasterReceiveDataBlocking(INST_LPI2C0, &masterRxBuffer, 1, true, 50);

			if (status == STATUS_SUCCESS) {
				serial_uart_send("Found I2C Device at : %d\r\n", var);
			}
		}

		dig_read();
		adc_read();

		static uint8_t messageId = 0;
		static uint8_t messageData = 0xff;
		SendCANData(TX_MAILBOX, messageId, &messageData, 1);
		messageData--;
		messageId++;

		static volatile int led_c = 0;
		switch (led_c++) {
			case 1:
				PINS_DRV_WritePin(LED_PORT, LED_GREEN, 0);
				PINS_DRV_WritePin(LED_PORT, LED_RED, 1);
				PINS_DRV_WritePin(LED_PORT, LED_BLUE, 1);
				break;
			case 2:
				PINS_DRV_WritePin(LED_PORT, LED_GREEN, 1);
				PINS_DRV_WritePin(LED_PORT, LED_RED, 0);
				PINS_DRV_WritePin(LED_PORT, LED_BLUE, 1);
				break;
			case 3:
				PINS_DRV_WritePin(LED_PORT, LED_GREEN, 1);
				PINS_DRV_WritePin(LED_PORT, LED_RED, 1);
				PINS_DRV_WritePin(LED_PORT, LED_BLUE, 0);
				break;
			default:
				PINS_DRV_WritePin(LED_PORT, LED_GREEN, 0);
				PINS_DRV_WritePin(LED_PORT, LED_RED, 0);
				PINS_DRV_WritePin(LED_PORT, LED_BLUE, 0);
		}
		led_c = led_c % 4;
		OSIF_TimeDelay(1000);
	}

	return exit_code;
}
