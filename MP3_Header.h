#ifndef __MP3_HEADER_H__
#define __MP3_HEADER_H__

#include <SD.h>

//#define MP3_DEBUG

#define _MP3HEADERBUFFER 256
#define reverseBits(b) ((((b) * 0x0802u & 0x22110u) | ((b) * 0x8020u & 0x88440u)) * 0x10101u >> 16)

#define ID3TAGMAX 73

#ifndef SYNCWORDH
const uint32_t SYNCWORDH = 0xff;
const uint32_t SYNCWORDL = 0xf0;
#endif

const uint16_t bitrates[] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
const uint16_t samplingrates[] = { 44100, 48000, 32000, 0 };

const char *ID3_TAGS[] PROGMEM =
{
	"AENC",
	"APIC",
	"COMM",
	"COMR",
	"ENCR",
	"EQUA",
	"ETCO",
	"GEOB",
	"GRID",
	"IPLS",
	"LINK",
	"MCDI",
	"MLLT",
	"OWNE",
	"PRIV",
	"PCNT",
	"POPM",
	"POSS",
	"RBUF",
	"RVAD",
	"RVRB",
	"SYLT",
	"SYTC",
	"TALB",
	"TBPM",
	"TCOM",
	"TCON",
	"TCOP",
	"TDAT",
	"TDLY",
	"TENC",
	"TEXT",
	"TFLT",
	"TIME",
	"TIT1",
	"TKEY",
	"TLAN",
	"TLEN",
	"TMED",
	"TOAL",
	"TOFN",
	"TOLY",
	"TOPE",
	"TORY",
	"TOWN",
	"TPE1",
	"TPOS",
	"TPUB",
	"TRCK",
	"TRDA",
	"TRSN",
	"TRSO",
	"TSIZ",
	"TSRC",
	"TSSE",
	"TYER",
	"TXXX",
	"UFID",
	"USER",
	"USLT",
	"WCOM",
	"WCOP",
	"WOAF",
	"WOAR",
	"WOAS",
	"WORS",
	"WPAY",
	"WPUB",
	"WXXX",
	"TDRC",
	"TIT2",
	"TPE2",
	"TIT3",
	"TPE3" };

enum ID3Tags : int8_t
{
	NONE = -1,
	AENC = 0,
	APIC = 1,
	COMM = 2,
	COMR = 3,
	ENCR = 4,
	EQUA = 5,
	ETCO = 6,
	GEOB = 7,
	GRID = 8,
	IPLS = 9,
	LINK = 10,
	MCDI = 11,
	MLLT = 12,
	OWNE = 13,
	PRIV = 14,
	PCNT = 15,
	POPM = 16,
	POSS = 17,
	RBUF = 18,
	RVAD = 19,
	RVRB = 20,
	SYLT = 21,
	SYTC = 22,
	TALB = 23,
	TBPM = 24,
	TCOM = 25,
	TCON = 26,
	TCOP = 27,
	TDAT = 28,
	TDLY = 29,
	TENC = 30,
	TEXT = 31,
	TFLT = 32,
	TIME = 33,
	TIT1 = 34,
	TKEY = 35,
	TLAN = 36,
	TLEN = 37,
	TMED = 38,
	TOAL = 39,
	TOFN = 40,
	TOLY = 41,
	TOPE = 42,
	TORY = 43,
	TOWN = 44,
	TPE1 = 45,
	TPOS = 46,
	TPUB = 47,
	TRCK = 48,
	TRDA = 49,
	TRSN = 50,
	TRSO = 51,
	TSIZ = 52,
	TSRC = 53,
	TSSE = 54,
	TYER = 55,
	TXXX = 56,
	UFID = 57,
	USER = 58,
	USLT = 59,
	WCOM = 60,
	WCOP = 61,
	WOAF = 62,
	WOAR = 63,
	WOAS = 64,
	WORS = 65,
	WPAY = 66,
	WPUB = 67,
	WXXX = 68,
	TDRC = 69,
	TIT2 = 70,
	TPE2 = 71,
	TIT3 = 72,
	TPE3 = 73
};

#define isArtistHeader(x)	(((x) == 45) || ((x) == 71) || ((x) == 73))
#define isAlbumHeader(x)	(((x) == 23))
#define isTitelHeader(x)	(((x) == 34) || ((x) == 70) || ((x) == 72))
#define isTaaHeader(x)		(isTitelHeader(x) || isAlbumHeader(x) || isArtistHeader(x))

static char *getTimeString(char *rtctime, uint32_t seconds) {
	uint32_t m = seconds / 60;
	char second = (char)(seconds % 60);
	char hour = (char)(m / 60);
	char minute = (char)(m % 60);
	if (hour < 10) {
		rtctime[0] = 48;
		rtctime[1] = (hour + 48);
	}
	else {
		rtctime[0] = ((hour / 10) % 10) + 48;
		rtctime[1] = (hour % 10) + 48;
	}

	rtctime[2] = ':';

	if (minute < 10) {
		rtctime[3] = 48;
		rtctime[4] = (minute + 48);
	}
	else {
		rtctime[3] = ((minute / 10) % 10) + 48;
		rtctime[4] = (minute % 10) + 48;
	}
	rtctime[5] = ':';
	if (second < 10) {
		rtctime[6] = 48;
		rtctime[7] = (second + 48);
	}
	else {
		rtctime[6] = ((second / 10) % 10) + 48;
		rtctime[7] = (second % 10) + 48;
	}
	return rtctime;
};

static uint32_t GetMP3Duration(File &fs, uint32_t index)
{
	uint8_t buffer[_MP3HEADERBUFFER];
	fs.readBytes(buffer, _MP3HEADERBUFFER);
	int32_t ii = 0;// __findSyncWord(buffer, _MP3HEADERBUFFER);

	for (; ii < _MP3HEADERBUFFER; ii++)
	{
		if ((buffer[ii + 0] & SYNCWORDH) == SYNCWORDH && (buffer[ii + 1] & SYNCWORDL) == SYNCWORDL)
			break;
	}
	if (ii >= _MP3HEADERBUFFER - 1) return fs.size();

	//Read Xing Header
	bool foundXing = false;
	while (ii++ < _MP3HEADERBUFFER - 1)
	{
		if ((buffer[ii] == 0x58) && (buffer[ii + 1] == 0x69) && (buffer[ii + 2] == 0x6E) && (buffer[ii + 3] == 0x67)) {
			foundXing = true;
			break;
		}
		else if ((buffer[ii] == 0x49) && (buffer[ii + 1] == 0x6E) && (buffer[ii + 2] == 0x66) && (buffer[ii + 3] == 0x6F)) {
			foundXing = true;
			break;
		}
	}
	uint32_t framecount = 0;
	uint32_t flags = 0;
	if (foundXing)
	{
		flags = (((uint32_t)buffer[ii + 4] << 24) |
			((uint32_t)buffer[ii + 5] << 16) |
			((uint32_t)buffer[ii + 6] << 8) |
			buffer[ii + 7]);
		framecount = (((uint32_t)buffer[ii + 8] << 24) |
			((uint32_t)buffer[ii + 9] << 16) |
			((uint32_t)buffer[ii + 10] << 8) |
			buffer[ii + 11]);
	}
	fs.readBytes(buffer, _MP3HEADERBUFFER);
	uint16_t idx = 0;
	while (true)
	{
		if ((buffer[idx + 0] & SYNCWORDH) == SYNCWORDH && (buffer[idx + 1] & SYNCWORDL) == SYNCWORDL)
		{
			break;
		}
		if (idx >= _MP3HEADERBUFFER - 6)
		{
			fs.readBytes(buffer, _MP3HEADERBUFFER);
			idx = 0;
		}
		else idx++;
	}
	fs.seek(index);

	uint32_t b = reverseBits(buffer[idx + 0]) |
		reverseBits(buffer[idx + 1]) << 8 |
		reverseBits(buffer[idx + 2]) << 16 |
		reverseBits(buffer[idx + 3]) << 24;

	uint8_t bidx = 0;
	bidx = bitWrite(bidx, 3, bitRead(b, 16));// bits[16]);
	bidx = bitWrite(bidx, 2, bitRead(b, 17));
	bidx = bitWrite(bidx, 1, bitRead(b, 18));
	bidx = bitWrite(bidx, 0, bitRead(b, 19));

	uint8_t fidx = 0;
	fidx = bitWrite(fidx, 1, bitRead(b, 20));
	fidx = bitWrite(fidx, 0, bitRead(b, 21));

	uint32_t duration = 0;
	if (foundXing)
	{
		duration = (int32_t)(framecount * 1152.0f / (float)samplingrates[fidx]);//VBR
	}
	else
	{
		duration = (int32_t)((fs.size() - index) / bitrates[bidx] * 8.0f) / 1000;//CBR
	}
	return duration;
};

static uint32_t ParseID3Header(File &fs, char *buffer, uint16_t size = 256)
{
	//http://id3.org/id3v2.3.0#ID3v2_header
	char abuff[256];
	memset(abuff, '\0', 256);
	memset(buffer, '\0', size);

	fs.readBytes(buffer, 10);

	bool artistTag = false;
	bool titleTag = false;

	uint32_t sz = 0;
	if (buffer[0] == 0x49 && buffer[1] == 0x44 && buffer[2] == 0x33 && buffer[3] < 0xFF) {
		sz = (((uint32_t)buffer[6] << 23) | ((uint32_t)buffer[7] << 15) | ((uint32_t)buffer[8] << 7) | buffer[9]);
	}
	else {
		return 0;
	}
#ifdef  MP3_DEBUG
	Serial.print("ID3V2.");
	Serial.print(buffer[3], DEC);
	Serial.println(sz);
#endif

	while (fs.position() < sz) {
		memset(buffer, '\0', size);
		fs.readBytes(buffer, 10);
		if (buffer[0] == 0)
			continue;

		ID3Tags tag = ID3Tags::NONE;
		for (int8_t i = 0; i <= ID3TAGMAX; i++) {
			if (strncmp(buffer, ID3_TAGS[i], 4) == 0) {
				tag = (ID3Tags)i;
				break;
			}
		}
		uint32_t z = ((uint32_t)buffer[4] << 23) | ((uint32_t)buffer[5] << 15) | ((uint32_t)buffer[6] << 7) | buffer[7];
		memset(buffer, '\0', size);
		uint16_t index = 0;
		for (uint32_t i = 0; i < z; i++) {
			int b = fs.read();
			if (b >= ' ' && i < 64) {
				buffer[index] = (char)b;
				index++;
			}
		}
		if (tag != ID3Tags::NONE) {
			if (isTaaHeader(tag)) {
				if (strlen(buffer) > 2) {
					if (isArtistHeader(tag)) {
						if (!artistTag) {
							artistTag = true;
							strcpy(abuff + 127, buffer);
						}
					}
					else if (isTitelHeader(tag)) {
						if (!titleTag) {
							titleTag = true;
							strncpy(abuff, buffer, 127);
						}
					}
				}
#ifdef  MP3_DEBUG
				Serial.print("(");
				Serial.print(z);
				Serial.print("):\t");
				Serial.println(buffer);
#endif
				}
			}
		}
	if (titleTag) {
		memset(buffer, '\0', size);
		uint16_t i = 0;
		uint16_t offset = 0;
		for (; i < 127; i++) {
			if (abuff[i] >= ' ') {
				buffer[offset] = abuff[i];
				offset++;
			}
			else {
				buffer[offset++] = ' ';
				buffer[offset++] = '-';
				buffer[offset++] = ' ';
				break;
			}
		}
		if (artistTag) {
			for (; i < 256; i++) {
				if (abuff[i] >= ' ') {
					buffer[offset] = abuff[i];
					offset++;
				}
			}
		}
	}
	return sz;
	};

#endif // !__MP3_HEADER_H__
