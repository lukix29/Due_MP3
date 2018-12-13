#ifndef LIB_LixMP3_H
#define LIB_LixMP3_H

#include "Arduino.h"
#include <SD.h>
#include "FastDac.h"
#include "arm_math.h"
#include "mp3lib/mp3dec.h"
#include "mp3lib/mp3common.h"

#define MP3_OUTBUF_SIZE (4 * 1024)
#define MP3_INBUF_SIZE (2 * 1024)
#define BUFFER_LOWER_THRESH  (1 * 1024)

#define MP3_SAMPLE_RATE_DEFAULT 48000

#define MP3_TC				TC1
#define MP3_IRQn			TC3_IRQn
#define MP3_Handler			TC3_Handler
#define MP3_TC_CHANNEL		0

struct LixMP3_outbuf {
	volatile int count;
	int16_t buffer[MP3_OUTBUF_SIZE];
};

class LixMP3 {
public:
	LixMP3() : hMP3Decoder() { inbufend = (inBuf + MP3_INBUF_SIZE); }
	~LixMP3() { MP3FreeDecoder(hMP3Decoder); };
	bool begin();
	//void setBufferCallback(int (*fun_ptr)(uint8_t *, int));
	//void setSampleReadyCallback(void (*fun_ptr)(int16_t, int16_t));

	int play();
	void pause();
	void resume();

	void stop();

	int tick(File &file);

	static uint8_t numChannels;

protected:

	HMP3Decoder hMP3Decoder;

	volatile int bytesLeft;
	uint8_t *readPtr;
	uint8_t *writePtr;
	uint8_t inBuf[MP3_INBUF_SIZE];
	uint8_t *inbufend;
	bool playing = false;

	//int (*bufferCallback)(uint8_t *, int);
	int findID3Offset(uint8_t *readPtr);
};

//class LixMP3_DMA : public LixMP3 {
//public:
//	LixMP3_DMA() : LixMP3() {
//		framebuf = NULL;
//		decodeCallback = NULL;
//	}
//	~LixMP3_DMA() {
//		if(framebuf != NULL) free(framebuf);
//	}
//
//	void getBuffers(int16_t **ping, int16_t **pong);
//	void setDecodeCallback(void (*fun_ptr)(int16_t *, int)) { decodeCallback = fun_ptr; }
//
//	void play();
//	int fill();
//private:
//	int16_t *framebuf, *leftover;
//	int leftoverSamples;
//	MP3FrameInfo frameInfo;
//	void (*decodeCallback)(int16_t *, int);
//};

#endif
