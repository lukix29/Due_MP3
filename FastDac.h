#ifndef __FAST_DAC_H__
#define __FAST_DAC_H__

#include <Arduino.h>

#define DACMIN					0
#define DACMAX					4095U
#define DACVOLTMIN				550U
#define DACVOLTMAX				2750U
#define DAC_CHANNEL				0

static void FastDAC_enable() {
	analogWriteResolution(12);
	pmc_enable_periph_clk(DACC_INTERFACE_ID);
	/* Reset DACC registers */
	dacc_reset(DACC_INTERFACE);
	/* Power save:	* sleep mode  - 0 (disabled)	* fast wakeup - 0 (disabled)	*/
	dacc_set_power_save(DACC_INTERFACE, 0, 0);
	/* Half word transfer mode */
	dacc_set_transfer_mode(DACC_INTERFACE, 0);
	/* Timing:	* refresh        - 0x08 (1024*8 dacc clocks)	* max speed mode -    0 (disabled)	* startup time   - 0x10 (1024 dacc clocks)	*/
	dacc_set_timing(DACC_INTERFACE, 0x08, 0, 0x10);
	/* Set up analog current */
	dacc_set_analog_control(DACC_INTERFACE, DACC_ACR_IBCTLCH0(0x02) | DACC_ACR_IBCTLCH1(0x02) | DACC_ACR_IBCTLDACCORE(0x01));
	/* Select DAC Channel*/
	dacc_enable_channel(DACC_INTERFACE, DAC_CHANNEL);
};

static void FastDAC_disable() {
	pmc_disable_periph_clk(DACC_INTERFACE_ID);
	dacc_reset(DACC_INTERFACE);
	dacc_disable_channel(DACC_INTERFACE, DAC_CHANNEL);
};

#define FastDAC_write(v_data) (DACC_INTERFACE->DACC_CDR = (v_data))
#endif