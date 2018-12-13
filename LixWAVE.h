#ifndef __LIX_WAVE__
#define __LIX_WAVE__

#include <Arduino.h>
#include "Sinus.h"
#include "FastDac.h"

//#define DEBUG

//#define RAM2ADDRESS				0x20080000UL

//#define USE_PWM

//https://www.arduino.cc/en/Hacking/PinMappingSAM3X

#define PCM_MAXSAMPLERATE		44100UL//hz
#define PCM_MAXBYTESPERSAMPLE	2UL
#define PCM_BUFFERSIZE			16000UL
#define PCM_HEADERSIZE			44UL

#define PCM_TIMER_TCC			TC0
#define PCM_TIMER_CHANNEL		2
#define PCM_TIMER_IRQ			TC2_IRQn

//#define ADC_TIMER_TCC			TC1
//#define ADC_TIMER_CHANNEL		2
//#define ADC_TIMER_IRQ			TC5_IRQn

#define PCM_SONGEND				0
#define PCM_FILLING				2
#define PCM_NORMAL				1

#define CALCBUFFTIME(sample_rate, bytes_per_sample) ((uint32_t)((((PCM_BUFFERSIZE * 2.0f) / ((float)bytes_per_sample)) / ((float)sample_rate)) * 1000))

static char *B_CHAR(uint8_t &idx, uint8_t *buff)
{
	char arr[4];
	for (uint8_t i = idx; i < idx + 4; i++) {
		arr[i - idx] = (char)buff[i];
	}
	idx += 4;
#ifdef DEBUG
	Serial.println(arr);
#endif
	return arr;
};
static uint32_t B_UINT32(uint8_t &idx, uint8_t *buff)
{
	uint32_t v = ((uint32_t)(((uint32_t)buff[idx + 3] << 24) | ((uint32_t)buff[idx + 2] << 16) | ((uint32_t)buff[idx + 1] << 8) | (uint32_t)buff[idx]));
	idx += 4;
	return v;
};
static uint16_t B_UINT16(uint8_t &idx, uint8_t *buff)
{
	uint16_t v = ((uint16_t)((((uint16_t)buff[idx + 1] << 8) | (uint16_t)buff[idx])));
	idx += 2;
	return v;
};

#pragma region Variables
uint32_t			_PCMpreBufferTime = CALCBUFFTIME(PCM_MAXSAMPLERATE, PCM_MAXBYTESPERSAMPLE);//ms
uint32_t			_PCMdataSize = 0;
uint32_t			_PCMsamplerate = 44100;
uint32_t			_PCMbitsPerSample = 16;
uint32_t			_PCMbytesPerSample = 2;
uint32_t			_PCMchannels = 1;
uint32_t			_PCMformat = 0;
uint32_t			_PCMduration = 0;

volatile uint8_t	_PCMisPause = true;
volatile uint8_t	_PCMisPlaying = false;
volatile uint8_t	_PCMrelobuff = 0;
volatile uint8_t	_PCMbufftouse = 0;
volatile uint32_t	_PCMbuffindex = 0;
volatile uint32_t	_PCMfilePos = 0;

uint8_t				_PCMplayType = 0;
uint8_t				_PCMbuffer[PCM_BUFFERSIZE];
uint8_t				_PCMbuffer1[PCM_BUFFERSIZE];

static void(*_pcm_tcc_isr_)(void);
#pragma endregion

#pragma region PRIVATE

#pragma region TIMER
void _PCM_TCC_stop() {
	NVIC_DisableIRQ(PCM_TIMER_IRQ);
	TC_Stop(PCM_TIMER_TCC, PCM_TIMER_CHANNEL);
}
void _PCM_TCC_setFreq(uint32_t frequency) {
	// Tell the Power Management Controller to disable
	// the write protection of the (Timer/Counter) registers:
	pmc_set_writeprotect(false);
	// Enable clock for the timer
	pmc_enable_periph_clk((uint32_t)PCM_TIMER_IRQ);
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

	//switch (clock) {
	//case TC_CMR_TCCLKS_TIMER_CLOCK1:
	//	frequency = (double)F_CPU /*SystemCoreClock*/ / 2.0 / (double)rc;
	//	break;
	//case TC_CMR_TCCLKS_TIMER_CLOCK2:
	//	frequency = (double)F_CPU / 8.0 / (double)rc;
	//	break;
	//case TC_CMR_TCCLKS_TIMER_CLOCK3:
	//	frequency = (double)F_CPU / 32.0 / (double)rc;
	//	break;
	//default: // TC_CMR_TCCLKS_TIMER_CLOCK4
	//	frequency = (double)F_CPU / 128.0 / (double)rc;
	//	break;
	//}

	// Set up the Timer in waveform mode which creates a PWM	// in UP mode with automatic trigger on RC Compare	// and sets it up with the determined internal clock as clock input.
	TC_Configure(PCM_TIMER_TCC, PCM_TIMER_CHANNEL, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | clock);
	// Reset counter and fire interrupt when RC value is matched:
	TC_SetRC(PCM_TIMER_TCC, PCM_TIMER_CHANNEL, rc);
	// Enable the RC Compare Interrupt...
	PCM_TIMER_TCC->TC_CHANNEL[PCM_TIMER_CHANNEL].TC_IER = TC_IER_CPCS;
	// ... and disable all others.
	PCM_TIMER_TCC->TC_CHANNEL[PCM_TIMER_CHANNEL].TC_IDR = ~TC_IER_CPCS;
}
void _PCM_TCC_start() {
	NVIC_ClearPendingIRQ(PCM_TIMER_IRQ);
	NVIC_EnableIRQ(PCM_TIMER_IRQ);
	TC_Start(PCM_TIMER_TCC, PCM_TIMER_CHANNEL);
}
void TC2_Handler(void) {
	_pcm_tcc_isr_();
	TC_GetStatus(PCM_TIMER_TCC, PCM_TIMER_CHANNEL);
}
#pragma endregion

#define _PCMplay(v_value) { FastDAC_write(v_value); _PCMbuffindex += _PCMbytesPerSample; if (_PCMbuffindex >= PCM_BUFFERSIZE) { _PCMbuffindex = 0; if (_PCMbufftouse == 0) { _PCMrelobuff = 1; _PCMbufftouse = 1; } else { _PCMrelobuff = 2; _PCMbufftouse = 0; } } };
void _PCMisr8() {
	uint8_t v;
	switch (_PCMbufftouse)
	{
	case 0:
		v = _PCMbuffer[_PCMbuffindex];
		break;
	case 1:
		v = _PCMbuffer1[_PCMbuffindex];
		break;
	}
	_PCMplay((uint16_t)v);
};
void _PCMisr16() {
	int16_t v;
	switch (_PCMbufftouse)
	{
	case 0:
		v = (((int16_t)_PCMbuffer[_PCMbuffindex + 1] << 8) | (int16_t)_PCMbuffer[_PCMbuffindex]);
		break;
	case 1:
		v = (((int16_t)_PCMbuffer1[_PCMbuffindex + 1] << 8) | (int16_t)_PCMbuffer1[_PCMbuffindex]);
		break;
	}

	_PCMplay((uint16_t)((v + 32768U) / 16U));
};
void _PCMisr32() {
	int32_t v;
	switch (_PCMbufftouse)
	{
	case 0:
		v = (((int32_t)_PCMbuffer[_PCMbuffindex + 3] << 24) | ((int32_t)_PCMbuffer[_PCMbuffindex + 2] << 16) | ((int32_t)_PCMbuffer[_PCMbuffindex + 1] << 8) | (int32_t)_PCMbuffer[_PCMbuffindex]);
		break;
	case 1:
		v = (((int32_t)_PCMbuffer1[_PCMbuffindex + 3] << 24) | ((int32_t)_PCMbuffer1[_PCMbuffindex + 2] << 16) | ((int32_t)_PCMbuffer1[_PCMbuffindex + 1] << 8) | (int32_t)_PCMbuffer1[_PCMbuffindex]);
		break;
	}
	_PCMplay((uint16_t)((v + 2147483648UL) / 1048832UL));
};

void _PCMisrSin() {
	FastDAC_write(sinusWave[_PCMbuffindex]);
	_PCMbuffindex++;
	if (_PCMbuffindex >= sinusWaveSamples) {
		_PCMbuffindex = 0;
	}
}

uint8_t _PCMparseHeader(uint8_t *head, uint32_t fileSize) {
	_PCMpreBufferTime = 0;//ms
	_PCMdataSize = 0;
	_PCMsamplerate = 0;
	_PCMbitsPerSample = 0;
	_PCMbytesPerSample = 0;
	_PCMchannels = 0;
	_PCMformat = 0;
	_PCMduration = 0;

#ifdef DEBUG
	Serial.println("\r\n-----HEADER START-----");
	Serial.print("riff-id:\t\t");
#endif
	uint8_t i = 0;
	B_CHAR(i, head);
#ifdef DEBUG
	Serial.print("file-size:\t\t");
	uint32_t h0 =
#endif
		B_UINT32(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("riff-type:\t\t");
#endif
	B_CHAR(i, head);
#ifdef DEBUG
	Serial.print("fmt-sig:\t\t");
#endif
	char *sig = B_CHAR(i, head);
#ifdef DEBUG
	Serial.print("fmt-size:\t\t");
	//fmt length
	h0 =
#endif
		B_UINT32(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("format-tag:\t");
	//_PCMformat tag
	h0 =
#endif
		_PCMformat = B_UINT16(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("channels:\t\t");
	//_PCMchannels
	h0 =
#endif
		_PCMchannels = B_UINT16(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("sample-rate:\t");
	//_PCMsamplerate
	h0 =
#endif
		_PCMsamplerate = B_UINT32(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("bytes/sec:\t");
	//bytes/sec
	h0 =
#endif
		B_UINT32(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("block-align:\t");
	//block-align
	h0 =
#endif
		_PCMbytesPerSample = B_UINT16(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("bits/sample:\t");
	//bits/sample
	h0 =
#endif
		_PCMbitsPerSample = B_UINT16(i, head);
#ifdef DEBUG
	Serial.println(h0);
	Serial.print("data-sig:\t\t");
#endif
	B_CHAR(i, head);
#ifdef DEBUG
	Serial.print("data-size:\t\t");
#endif
	uint32_t pcmds = B_UINT32(i, head);
	uint32_t fsfs = (fileSize - PCM_HEADERSIZE);
	if (pcmds < fsfs) pcmds = fsfs;
	_PCMdataSize = pcmds;
	_PCMduration = (_PCMdataSize / _PCMbytesPerSample) / _PCMsamplerate;

#ifdef DEBUG
	Serial.println(_PCMdataSize);
#endif
	if (_PCMbitsPerSample != 32 && _PCMbitsPerSample != 16 && _PCMbitsPerSample != 8) return false;
	if (_PCMformat != 0x0001) return false;

	_PCMpreBufferTime = CALCBUFFTIME(_PCMsamplerate, _PCMbytesPerSample);
#ifdef DEBUG
	Serial.print("buffer-duration(ms):\t");
	Serial.println(_PCMpreBufferTime);
	Serial.print("data-duration(s):\t");
	Serial.println(_PCMduration);
	char tmp[16];
	memset(tmp, '\0', 16);
	Serial.println(getTimeString(tmp, _PCMduration));
	Serial.println("------HEADER END------\r\n");
	delay(100);
#endif
	return true;
};
#pragma endregion

#pragma region PUBLIC
void PCM_stop() {
	_PCM_TCC_stop();
	FastDAC_disable();
	_PCMrelobuff = 0;
	_PCMbufftouse = 0;
	_PCMbuffindex = 0;
	_PCMfilePos = 0;
	_PCMisPlaying = false;
}
uint8_t PCM_pause() {
	if (_PCMisPause) {
		FastDAC_enable();
		_PCM_TCC_start();
		_PCMisPause = false;
	}
	else {
		_PCM_TCC_stop();
		FastDAC_disable();
		_PCMisPause = true;
	}
	return _PCMisPause;
}
uint8_t PCM_readHeader(Stream &file, uint32_t size) {
	//Read RIFF-WAVE header & fmt info
	uint8_t head[PCM_HEADERSIZE];
	file.readBytes(head, PCM_HEADERSIZE);

	if (!_PCMparseHeader(head, size)) {
		return false;
	}
	return true;
}
uint8_t PCM_check(Stream &file) {
	if (_PCMplayType == 1) return 1;
	if (!_PCMisPlaying || _PCMisPause) return 0;
	if (_PCMrelobuff > 0) {
		if (
			//file.() >= _PCMdataSize			||
			!_PCMisPlaying || !file.available())
		{
			PCM_stop();
			return 0;
		}
		switch (_PCMrelobuff)
		{
		case 1:
			file.readBytes(_PCMbuffer, PCM_BUFFERSIZE);
			break;
		case 2:
			file.readBytes(_PCMbuffer1, PCM_BUFFERSIZE);
			break;
		}
		_PCMrelobuff = 0;
		_PCMfilePos += PCM_BUFFERSIZE;
		return 2;
	}
	return 1;
}
uint8_t PCM_playFile(Stream &file, uint32_t size) {
	if (!PCM_readHeader(file, size)) return false;

	_PCM_TCC_stop();
	_PCMplayType = 0;
	//Fill buffers so the timer doesn't run empty at the beginning
	file.readBytes(_PCMbuffer, PCM_BUFFERSIZE);
	file.readBytes(_PCMbuffer1, PCM_BUFFERSIZE);

	analogWriteResolution(constrain(_PCMbitsPerSample, 8, 12));

	switch (_PCMbitsPerSample)
	{
	case 8:
		_pcm_tcc_isr_ = _PCMisr8;
		break;
	case 16:
		_pcm_tcc_isr_ = _PCMisr16;
		break;
	case 32:
		_pcm_tcc_isr_ = _PCMisr32;
		break;
	}

	_PCM_TCC_setFreq(_PCMsamplerate);// PCM_TIMER.setFrequency(_PCMsamplerate);
	//_lastBuffRead = millis();

	_PCMisPause = false;
	_PCMisPlaying = true;

	FastDAC_enable();
	_PCM_TCC_start();
	return true;
}

uint8_t PCM_Sinus(uint32_t frequency) {
	_PCM_TCC_stop();

	if (frequency > 0) {
		_PCMplayType = 1;

		analogWriteResolution(12);

		_pcm_tcc_isr_ = _PCMisrSin;

		frequency = frequency * sinusWaveSamples;

		_PCM_TCC_setFreq(frequency);// PCM_TIMER.setFrequency(_PCMsamplerate);

		_PCMisPause = false;
		_PCMisPlaying = true;

		FastDAC_enable();
		_PCM_TCC_start();
		return true;
	}
	else {
		_PCMplayType = 0;
		return false;
	}
}
#pragma endregion
#endif