#include "Adafruit_MP3.h"
#include "FastDac.h"

volatile bool activeOutbuf;
LixMP3_outbuf outbufs[2];
volatile int16_t *outptr;
//static void(*sampleReadyCallback)(int16_t, int16_t);

uint8_t LixMP3::numChannels = 0;

#pragma region TIMER

/**
 *****************************************************************************************
 *  @brief      enable the playback timer
 *
 *  @return     none
 ****************************************************************************************/
static inline void enableTimer()
{
	NVIC_ClearPendingIRQ(MP3_IRQn);
	NVIC_EnableIRQ(MP3_IRQn);
	TC_Start(MP3_TC, MP3_TC_CHANNEL);
}

/**
 *****************************************************************************************
 *  @brief      disable the playback timer
 *
 *  @return     none
 ****************************************************************************************/
static inline void disableTimer()
{
	NVIC_DisableIRQ(MP3_IRQn);
	TC_Stop(MP3_TC, MP3_TC_CHANNEL);
}

static inline void setFrequency(uint32_t frequency) {
	pmc_set_writeprotect(false);
	// Enable clock for the timer
	pmc_enable_periph_clk((uint32_t)MP3_IRQn);
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
	TC_Configure(MP3_TC, MP3_TC_CHANNEL, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | clock);
	// Reset counter and fire interrupt when RC value is matched:
	TC_SetRC(MP3_TC, MP3_TC_CHANNEL, rc);
	// Enable the RC Compare Interrupt...
	MP3_TC->TC_CHANNEL[MP3_TC_CHANNEL].TC_IER = TC_IER_CPCS;
	// ... and disable all others.
	MP3_TC->TC_CHANNEL[MP3_TC_CHANNEL].TC_IDR = ~TC_IER_CPCS;
}
static inline void configureTimer()
{
	setFrequency(MP3_SAMPLE_RATE_DEFAULT);
}

static inline void acknowledgeInterrupt()
{
	TC_GetStatus(MP3_TC, MP3_TC_CHANNEL);
}

static inline void updateTimerFreq(uint32_t freq)
{
	disableTimer();
	setFrequency(freq);
	enableTimer();
}

#pragma endregion

/**
 *****************************************************************************************
 *  @brief      Begin the mp3 player. This initializes the playback timer and necessary interrupts.
 *
 *  @return     none
 ****************************************************************************************/
bool LixMP3::begin()
{
	//sampleReadyCallback = NULL;
	//bufferCallback = NULL;

	configureTimer();

	if ((hMP3Decoder = MP3InitDecoder()) == 0)
	{
		return false;
	}
	else return true;
}

/**
 *****************************************************************************************
 *  @brief      Set the function the player will call when it's buffers need to be filled.
 *				Care must be taken to ensure that the callback function is efficient.
 *				If the callback takes too long to fill the buffer, playback will be choppy
 *
 *	@param		fun_ptr the pointer to the callback function. This function must take a pointer
 *				to the location the bytes will be written, as well as an integer that represents
 *				the maximum possible bytes that can be written. The function should return the
 *				actual number of bytes that were written.
 *
 *  @return     none
 ****************************************************************************************/
 //void LixMP3::setBufferCallback(int(*fun_ptr)(uint8_t *, int)) { bufferCallback = fun_ptr; }

 /**
  *****************************************************************************************
  *  @brief      Set the function that the player will call when the playback timer fires.
  *				The callback is called inside of an ISR, so it should be short and efficient.
  *				This will usually just be writing samples to the DAC.
  *
  *	@param		fun_ptr the pointer to the callback function. The function must take two
  *				unsigned 16 bit integers. The first argument to the callback will be the
  *				left channel sample, and the second channel will be the right channel sample.
  *				If the played file is mono, only the left channel data is used.
  *
  *  @return     none
  ****************************************************************************************/
  //void LixMP3::setSampleReadyCallback(void(*fun_ptr)(int16_t, int16_t)) { sampleReadyCallback = fun_ptr; }

  /**
   *****************************************************************************************
   *  @brief      Play an mp3 file. This function resets the buffers and should only be used
   *				when beginning playback of a new mp3 file. If playback has been stopped
   *				and you would like to resume playback at the current location, use LixMP3::resume instead.
   *
   *  @return     none
   ****************************************************************************************/
int LixMP3::play()
{
	bytesLeft = 0;
	activeOutbuf = 0;
	readPtr = inBuf;
	writePtr = inBuf;

	outbufs[0].count = 0;
	outbufs[1].count = 0;
	playing = false;

	MP3FrameInfo frameInfo;
	int err, offset;

	/* Find start of next MP3 frame. Assume EOF if no sync found. */
	offset = MP3FindSyncWord(readPtr, bytesLeft);
	if (offset >= 0) {
		readPtr += offset;
		bytesLeft -= offset;
	}
	else {
		return ERR_MP3_END_OF_FILE;
	}

	err = MP3GetNextFrameInfo(hMP3Decoder, &frameInfo, readPtr);
	if (err != ERR_MP3_INVALID_FRAMEHEADER) {
		if (frameInfo.samprate != MP3_SAMPLE_RATE_DEFAULT)
		{
			updateTimerFreq(frameInfo.samprate);
		}
		playing = true;
		numChannels = frameInfo.nChans;
		//start the playback timer
		FastDAC_enable();
		enableTimer();
		return ERR_MP3_NONE;
	}
	return err;
}

//void LixMP3_DMA::play()
//{
//#if defined(__SAMD51__)
//	//don't need an interrupt here
//	MP3_TC->COUNT16.INTENCLR.bit.MC0 = 1;
//#endif
//	LixMP3::play();
//	NVIC_DisableIRQ(MP3_IRQn); //we don't need the interrupt
//	leftoverSamples = 0;
//
//	//fill both buffers
//	fill();
//}

/**
 *****************************************************************************************
 *  @brief      pause playback. This function stops the playback timer.
 *
 *  @return     none
 ****************************************************************************************/
void LixMP3::pause()
{
	disableTimer();
}
void LixMP3::stop() {
	disableTimer();
	FastDAC_disable();
	playing = false;
	bytesLeft = 0;
	activeOutbuf = 0;
}
/**
 *****************************************************************************************
 *  @brief      Resume playback. This function re-enables the playback timer. If you are
 *				starting playback of a new file, use LixMP3::play instead
 *
 *  @return     none
 ****************************************************************************************/
void LixMP3::resume()
{
	enableTimer();
}

/**
 *****************************************************************************************
 *  @brief      Get the number of bytes until the end of the ID3 tag.
 *
 *	@param		readPtr current read pointer
 *
 *  @return     none
 ****************************************************************************************/
int LixMP3::findID3Offset(uint8_t *readPtr)
{
	char header[10];
	memcpy(header, readPtr, 10);
	//http://id3.org/id3v2.3.0#ID3v2_header
	if (header[0] == 0x49 && header[1] == 0x44 && header[2] == 0x33 && header[3] < 0xFF) {
		//this is a tag
		uint32_t sz = ((uint32_t)header[6] << 23) | ((uint32_t)header[7] << 15) | ((uint32_t)header[8] << 7) | header[9];
		return sz;
	}
	else {
		//this is not a tag
		return 0;
	}
}

//void LixMP3_DMA::getBuffers(int16_t **ping, int16_t **pong) {
//	*pong = outbufs[0].buffer;
//	*ping = outbufs[1].buffer;
//}

/**
 *****************************************************************************************
 *  @brief      The main loop of the mp3 player. This function should be called as fast as
 *				possible in the loop() function of your sketch. This checks to see if the
 *				buffers need to be filled, and calls the buffer callback function if necessary.
 *				It also calls the functions to decode another frame of mp3 data.
 *
 *  @return     none
 ****************************************************************************************/
int LixMP3::tick(File &file) {
	noInterrupts();
	if (outbufs[activeOutbuf].count == 0 && outbufs[!activeOutbuf].count > 0) {
		//time to swap the buffers
		activeOutbuf = !activeOutbuf;
		outptr = outbufs[activeOutbuf].buffer;
	}
	interrupts();

	//if we are running out of samples, and don't yet have another buffer ready, get busy.
	if (outbufs[activeOutbuf].count < BUFFER_LOWER_THRESH && outbufs[!activeOutbuf].count < (MP3_OUTBUF_SIZE) >> 1) {
		//dumb, but we need to move any bytes to the beginning of the buffer
		if (readPtr != inBuf && bytesLeft < BUFFER_LOWER_THRESH) {
			memmove(inBuf, readPtr, bytesLeft);
			readPtr = inBuf;
			writePtr = inBuf + bytesLeft;
		}

		//get more data from the file
		if (inbufend - writePtr > 0) {
			int toRead = min((int)(inbufend - writePtr), MP3_INBUF_SIZE); //limit the number of bytes we can read at a time so the file isn't interrupted
			if (file.available())
			{
				int bytesRead = file.readBytes(writePtr, toRead);
				if (bytesRead > 0) {
					writePtr += bytesRead;
					bytesLeft += bytesRead;
				}
				else {
					stop();
					return ERR_MP3_END_OF_FILE;
				}
			}
			else {
				stop();
				return ERR_MP3_END_OF_FILE;
			}
		}

		int offset = MP3FindSyncWord(readPtr, bytesLeft);
		if (offset >= 0) {
			readPtr += offset;
			bytesLeft -= offset;

			//fil the inactive outbuffer
			int err = MP3Decode(hMP3Decoder, &readPtr, (int*)&bytesLeft, outbufs[!activeOutbuf].buffer + outbufs[!activeOutbuf].count, 0);
			MP3DecInfo *mp3DecInfo = (MP3DecInfo *)hMP3Decoder;
			outbufs[!activeOutbuf].count += mp3DecInfo->nGrans * mp3DecInfo->nGranSamps * mp3DecInfo->nChans;

			if (err) {
				return err;
			}
		}
		else {
			pause();
			return ERR_MP3_END_OF_FILE;
		}
	}
	return ERR_MP3_NONE;
}

//fill a buffer with data
//int LixMP3_DMA::fill() {
//
//	int ret = 0;
//	int16_t *curBuf = outbufs[activeOutbuf].buffer;
//
//	//put any leftover samples in the new buffer
//	if (leftoverSamples > 0) {
//		memcpy(outbufs[activeOutbuf].buffer, leftover, leftoverSamples * sizeof(int16_t));
//		outbufs[activeOutbuf].count = leftoverSamples;
//		leftoverSamples = 0;
//	}
//
//	while (outbufs[activeOutbuf].count < MP3_OUTBUF_SIZE) {
//	loopstart:
//		//dumb, but we need to move any bytes to the beginning of the buffer
//		if (readPtr != inBuf && bytesLeft < BUFFER_LOWER_THRESH) {
//			memmove(inBuf, readPtr, bytesLeft);
//			readPtr = inBuf;
//			writePtr = inBuf + bytesLeft;
//		}
//
//		//get more data from the user application
//		if (file.available()) {
//			if (inbufend - writePtr > 0) {
//				int bytesRead = bufferCallback(writePtr, inbufend - writePtr);
//				if (bytesRead == 0) {
//					ret = 1;
//					break;
//				}
//				writePtr += bytesRead;
//				bytesLeft += bytesRead;
//			}
//		}
//
//		int err, offset;
//
//		if (!playing) {
//			/* Find start of next MP3 frame. Assume EOF if no sync found. */
//			offset = MP3FindSyncWord(readPtr, bytesLeft);
//			if (offset >= 0) {
//				readPtr += offset;
//				bytesLeft -= offset;
//			}
//
//			err = MP3GetNextFrameInfo(hMP3Decoder, &frameInfo, readPtr);
//			if (err != ERR_MP3_INVALID_FRAMEHEADER) {
//				if (frameInfo.samprate != MP3_SAMPLE_RATE_DEFAULT)
//				{
//					updateTimerFreq(frameInfo.samprate);
//				}
//				playing = true;
//				LixMP3::numChannels = frameInfo.nChans;
//			}
//			if (framebuf != NULL) free(framebuf);
//			framebuf = (int16_t *)malloc(frameInfo.outputSamps * sizeof(int16_t));
//			goto loopstart;
//		}
//
//		offset = MP3FindSyncWord(readPtr, bytesLeft);
//		if (offset >= 0) {
//			readPtr += offset;
//			bytesLeft -= offset;
//
//			MP3DecInfo *mp3DecInfo = (MP3DecInfo *)hMP3Decoder;
//			int toRead = mp3DecInfo->nGrans * mp3DecInfo->nGranSamps * mp3DecInfo->nChans;
//			if (outbufs[activeOutbuf].count + toRead < MP3_OUTBUF_SIZE) {
//				//we can read directly into the output buffer so lets do that
//				err = MP3Decode(hMP3Decoder, &readPtr, (int*)&bytesLeft, outbufs[activeOutbuf].buffer + outbufs[activeOutbuf].count, 0);
//				outbufs[activeOutbuf].count += toRead;
//			}
//			else {
//				//the frame would cross byte boundaries, we need to split manually
//				err = MP3Decode(hMP3Decoder, &readPtr, (int*)&bytesLeft, framebuf, 0);
//				int remainder = MP3_OUTBUF_SIZE - outbufs[activeOutbuf].count;
//				memcpy(outbufs[activeOutbuf].buffer + outbufs[activeOutbuf].count, framebuf, remainder * sizeof(int16_t));
//				leftover = framebuf + remainder;
//				leftoverSamples = (toRead - remainder);
//
//				//swap buffers
//				activeOutbuf = !activeOutbuf;
//				outbufs[activeOutbuf].count = 0;
//				ret = 0;
//				break;
//			}
//
//			if (err) {
//				return err;
//			}
//		}
//	}
//
//	if (decodeCallback != NULL) decodeCallback(curBuf, MP3_OUTBUF_SIZE);
//
//
//	return ret;
//}

/**
 *****************************************************************************************
 *  @brief      The IRQ function that gets called whenever the playback timer fires.
 *
 *  @return     none
 ****************************************************************************************/
#define MP3_DAC_CONV(x) ((((uint32_t)(x)) + 32768U) / 16U)
void MP3_Handler()
{
	if (outbufs[activeOutbuf].count >= LixMP3::numChannels) {
		//it's sample time!
		if (LixMP3::numChannels == 1)
			FastDAC_write(MP3_DAC_CONV(*outptr));
		//sampleReadyCallback(*outptr, 0);
		else
			FastDAC_write(MP3_DAC_CONV(((*outptr) + (*(outptr + 1))) / 2));//	sampleReadyCallback(*outptr, *(outptr + 1));

		//increment the read position and decrement the remaining sample count
		outptr += LixMP3::numChannels;
		outbufs[activeOutbuf].count -= LixMP3::numChannels;
	}
	acknowledgeInterrupt();
}