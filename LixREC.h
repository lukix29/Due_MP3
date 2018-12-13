#ifndef __LIX_REC__
#define __LIX_REC__

#include <Arduino.h>

//#define DEBUG

//#define RAM2ADDRESS				0x20080000UL

//#define USE_PWM

//https://www.arduino.cc/en/Hacking/PinMappingSAM3X
#define ADCMIN					0
#define ADCMAX					4095U
#define ADC_MAXSAMPLERATE		44100UL//hz
#define ADC_MAXBYTESPERSAMPLE	2UL
#define ADC_BUFFERSIZE			8000UL

#define ADC_TIMER_TCC			TC1
#define ADC_TIMER_CHANNEL		2
#define ADC_TIMER_IRQ			TC5_IRQn

#pragma region Variables
uint32_t			_ADCdataSize = 0;
uint32_t			_ADCsamplerate = 44100;
uint32_t			_ADCbitsPerSample = 16;
uint32_t			_ADCbytesPerSample = 2;
uint8_t				_ADCisPause = 0;

volatile uint8_t	_ADCrelobuff = 0;
volatile uint8_t	_ADCbufftouse = 0;
volatile uint32_t	_ADCbuffindex = 0;

uint8_t				_ADCbuffer[ADC_BUFFERSIZE];
uint8_t				_ADCbuffer1[ADC_BUFFERSIZE];

static void(*_ADC_tcc_isr_)(void);
#pragma endregion

#pragma region PRIVATE

#pragma region TIMER
void _ADC_TCC_stop() {
	NVIC_DisableIRQ(ADC_TIMER_IRQ);
	TC_Stop(ADC_TIMER_TCC, ADC_TIMER_CHANNEL);
}
void _ADC_TCC_setFreq(uint32_t frequency) {
	// Tell the Power Management Controller to disable
	// the write protection of the (Timer/Counter) registers:
	pmc_set_writeprotect(false);
	// Enable clock for the timer
	pmc_enable_periph_clk((uint32_t)ADC_TIMER_IRQ);
	// Find the best clock for the wanted frequency
	const struct { uint8_t flag; uint8_t divisor; }
	clockConfig[] = {
		{ TC_CMR_TCCLKS_TIMER_CLOCK1,   2 },
	{ TC_CMR_TCCLKS_TIMER_CLOCK2,   8 },
	{ TC_CMR_TCCLKS_TIMER_CLOCK3,  32 },
	{ TC_CMR_TCCLKS_TIMER_CLOCK4, 128 } };
	int clkId = 3;	int bestClock = 3;	float bestError = 9.999e99;
	do {
		float ticks = (float)SystemCoreClock / frequency / (float)clockConfig[clkId].divisor;
		float error = clockConfig[clkId].divisor * (ticks - (ticks + 0.5));	// Error comparison needs scaling
		if (error < bestError) {
			bestClock = clkId;
			bestError = error;
		}
	} while (clkId-- > 0);

	uint32_t rc = (uint32_t)(((float)SystemCoreClock / frequency / (float)clockConfig[bestClock].divisor) + 0.5);
	uint8_t clock = clockConfig[bestClock].flag;

	// Set up the Timer in waveform mode which creates a PWM	// in UP mode with automatic trigger on RC Compare	// and sets it up with the determined internal clock as clock input.
	TC_Configure(ADC_TIMER_TCC, ADC_TIMER_CHANNEL, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | clock);
	// Reset counter and fire interrupt when RC value is matched:
	TC_SetRC(ADC_TIMER_TCC, ADC_TIMER_CHANNEL, rc);
	// Enable the RC Compare Interrupt...
	ADC_TIMER_TCC->TC_CHANNEL[ADC_TIMER_CHANNEL].TC_IER = TC_IER_CPCS;
	// ... and disable all others.
	ADC_TIMER_TCC->TC_CHANNEL[ADC_TIMER_CHANNEL].TC_IDR = ~TC_IER_CPCS;
}
void _ADC_TCC_start() {
	NVIC_ClearPendingIRQ(ADC_TIMER_IRQ);
	NVIC_EnableIRQ(ADC_TIMER_IRQ);
	TC_Start(ADC_TIMER_TCC, ADC_TIMER_CHANNEL);
}
void TC5_Handler(void) {
	TC_GetStatus(ADC_TIMER_TCC, ADC_TIMER_CHANNEL);
	_ADC_tcc_isr_();
}
#pragma endregion

#pragma region ADC
void _ADC_DAC_enable() {
	ADC->ADC_MR |= 0x80;
	ADC->ADC_CHER = 0x80;
};
void _ADC_DAC_disable() {
};
void _ADCisr8() {
};
void _ADCisr16() {
};
#pragma endregion

#pragma region PUBLIC
void ADC_stop() {
	_ADC_TCC_stop();
	_ADC_DAC_disable();
	_ADCrelobuff = 0;
	_ADCbufftouse = 0;
	_ADCbuffindex = 0;
}
uint8_t ADC_pause() {
	if (_ADCisPause) {
		_ADC_DAC_enable();
		_ADC_TCC_start();
		_ADCisPause = false;
	}
	else {
		_ADC_TCC_stop();
		_ADC_DAC_disable();
		_ADCisPause = true;
	}
	return _ADCisPause;
}

uint8_t ADC_check(Stream &file) {
	if (_ADCrelobuff > 0) {
		switch (_ADCrelobuff)
		{
		case 1:
			file.write(_ADCbuffer, ADC_BUFFERSIZE);
			break;
		case 2:
			file.write(_ADCbuffer1, ADC_BUFFERSIZE);
			break;
		}
		_ADCrelobuff = 0;
		return 2;
	}
	return 1;
}
uint8_t ADC_playFile() {
	_ADC_TCC_stop();

	analogReadResolution(constrain(_ADCbitsPerSample, 8, 12));

	switch (_ADCbitsPerSample)
	{
	case 8:
		_ADC_tcc_isr_ = _ADCisr8;
		break;
	case 16:
		_ADC_tcc_isr_ = _ADCisr16;
		break;
	case 32:
		_ADC_tcc_isr_ = _ADCisr32;
		break;
	}

	_ADC_TCC_setFreq(_ADCsamplerate);// ADC_TIMER.setFrequency(_ADCsamplerate);
									 //_lastBuffRead = millis();

	_ADC_DAC_enable();
	_ADC_TCC_start();
	return true;
}

#pragma endregion
#endif