#ifndef _LIXPLAYER_H_
#define _LIXPLAYER_H_

#include <Arduino.h>
#include <SD.h>

//uncomment MP3_Header.h for PCM Playback
#include "MP3_Header.h"
#ifndef __MP3_HEADER_H__
#include "LixWAVE.h"
#else
#include "Adafruit_MP3.h"
#endif

#define ROLLOVER(x, bottom, top) (((x) < (bottom)) ? (top) : (((x) == (top)) ? (bottom) : (x)));

#ifdef __MP3_HEADER_H__
#define PLAYER_FILELENGTHMAX	256
#define PLAYER_OFILEMAX			13
#else
#define PLAYER_FILELENGTHMAX	129
#define PLAYER_OFILEMAX			13
#define PLAYER_FILEPATHLENGTH	(PLAYER_FILELENGTHMAX + PLAYER_OFILEMAX)
#endif

enum PlayerRepeatType
{
	Repeat_OFF = 0,
	Repeat_Single = 1,
	Repeat_All = 2
};
enum PlayerRandomType
{
	Random_OFF = 0,
	Random_All = 1,
};

class LixPlayer
{
public:
	void begin(void) {
		isstopped = true;
		readFileCount();
		_playlistIndex = readPlaylist(0);
		readHeader();
	};

	uint8_t checkBuffer(void) {
		if (isstopped || _isPaused() || seeking) return false;
		_check(file);
	}

	void play(void) {
		if (file) file.close();
		delay(100);
		file = SD.open(currentFilePath);

		if (file && !file.isDirectory()) {
			Serial.print("Playing File:\t"); Serial.println(currentFilename);
			isstopped = false;
			if (!_playFile(file, file.size())) {
				Serial.println("Error Playing File!");
			}
		}
	};
	void stop(void) {
		isstopped = true;
		_stop();
		if (file) file.close();
		resetRandom();
		_playlistIndex = readPlaylist(0);
	};
	void pause(void) {
		if (isstopped) play();
		else _pause();
	};
	void seek(int32_t amount) {
		seeking = true;
		_seek(amount);
		seeking = false;
	};
	void seekTo(uint32_t pos) {
		seeking = true;
		_seekTo(pos);
		seeking = false;
	}

	uint16_t next(uint16_t fileIndex = (UINT16_MAX - 1)) {
		uint8_t isst = false;
		if (fileIndex < (UINT16_MAX - 1)) {
			_playlistIndex = Min(fileCount - 1, fileIndex);
			isst = true;
		}
		else {
			if (_randomMode == Random_All) {
				rd_lastFileList[rd_playCount] = _playlistIndex;
				rd_playCount++;
				int16_t rd = getRandom();
				//SERIAL.println("RD INFO");
				//SERIAL.println(rd);
				//SERIAL.println(rd_playCount);
				//SERIAL.println("-----------------------------------");
				//for (uint16_t i = 0; i < rd_playCount; i++) {
				//	SERIAL.println(rd_lastFileList[i], DEC);
				//}
				//SERIAL.println("-----------------------------------");
				if (rd == -1) {
					checkRepeat(true);
					return _playlistIndex;
				}
				_playlistIndex = (uint16_t)rd;
			}
			else {
				_playlistIndex++;
			}
		}

		if (isst) isst = isstopped;
		isstopped = true;
		_stop();
		if (file) file.close();
		_playlistIndex = readPlaylist(_playlistIndex);

		if (!_isPaused() && !isst) play();
		else readHeader();

		return _playlistIndex;
	};
	uint16_t prev(void) {
		isstopped = true;
		uint16_t pli = _playlistIndex;
		if (position() < 10) pli--;
		_stop();
		if (file) file.close();
		_playlistIndex = readPlaylist(pli);
		if (!_isPaused()) play();
		else readHeader();

		return _playlistIndex;
	};

	char *CurrentFilename(void) {
		return currentFilename;
	};

	uint8_t repeatMode(uint8_t mode = 3) {
		if (mode < 3) _repeatMode = (PlayerRepeatType)mode;
		else return _repeatMode;
	};
	uint8_t randomMode(uint8_t mode = 2) {
		if (mode < 2) {
			_randomMode = (PlayerRandomType)constrain(mode, 0, 1);
			resetRandom();
		}
		else return _randomMode;
	};

	uint8_t isStopped(void) {
		return isstopped;
	};
	uint8_t isPaused(void) {
		return _isPaused();
	}

	uint32_t position(void) {
		return _position();// ((_PCMfilePos / _PCMbytesPerSample) / _PCMsamplerate);
	};
	uint32_t restDuration(void) {
		return _restDuration();
		//if (isstopped) return _PCMduration;
		//else return (_PCMduration - ((_PCMfilePos / _PCMbytesPerSample) / _PCMsamplerate));
	};
	uint32_t duration(void) {
		_duration();
		//return _PCMduration;
	};

	uint16_t playlistIndex(void) {
		return _playlistIndex;
	};
	uint16_t playlistLength(void) {
		return fileCount;
	};
#ifdef __LIX_WAVE__
	void printPlaylist(Print &p) {
		_printplaylist(p);
	};
#else
	void printPlaylist(Print &p) {
	}
#endif
private:
#ifdef __LIX_WAVE__
	void _readHeader(File &file, uint32_t size) {
		PCM_readHeader(file, file.size());
	};
	uint8_t _playFile(File &file, uint32_t size) {
		return PCM_playFile(file, size);
	};
	void _stop() {
	};
	void _pause() {
	};

	uint8_t _isPaused(void) {
		return _PCMisPause;
	}

	uint32_t _position(void) {
		return ((_PCMfilePos / _PCMbytesPerSample) / _PCMsamplerate);
	};
	uint32_t _restDuration(void) {
		if (isstopped) return _PCMduration;
		else return (_PCMduration - ((_PCMfilePos / _PCMbytesPerSample) / _PCMsamplerate));
	};
	uint32_t _duration(void) {
		return _PCMduration;
	};
	void _printplaylist(Print &p) {
		File f;
		f = SD.open("list.txt");
		if (f) {
			const size_t siz = PLAYER_OFILEMAX + PLAYER_FILELENGTHMAX + 2;
			uint16_t count = 0;
			char temp[siz];
			p.print('{');
			while (f.available()) {
				f.readBytesUntil('\t', temp, siz);
				memset(temp, '\0', siz);
				size_t l = f.readBytesUntil('\n', temp, siz);
				p.print('\"');
				p.print(count);
				p.print("\":\"");
				p.write(temp, l);
				p.print('\"');
				count++;
				if (count < fileCount) p.print(',');
			}
			p.print('}');
		}
		p.println();
		f.close();
	}
	void _seek(int32_t amount) {
		amount = file.position() + (((_PCMsamplerate * _PCMbytesPerSample)) * amount);
		amount = constrain(amount, PCM_HEADERSIZE, file.size() - (PCM_BUFFERSIZE * 2));
		file.seek((uint32_t)amount);
		if (file.available()) {
			file.read(_PCMbuffer, PCM_BUFFERSIZE);
			file.read(_PCMbuffer1, PCM_BUFFERSIZE);
			_PCMbuffindex = 0;
			_PCMrelobuff = 0;
			_PCMbufftouse = 0;
			_PCMbuffindex = 0;
			_PCMfilePos = file.position() - ((PCM_BUFFERSIZE * 2) + PCM_HEADERSIZE);
		}
	};
	void _seekTo(uint32_t pos) {
		pos = ((_PCMsamplerate * _PCMbytesPerSample) * pos) + PCM_HEADERSIZE;
		pos = constrain(pos, PCM_HEADERSIZE, file.size() - (PCM_BUFFERSIZE * 2));
		file.seek(pos);
		if (file.available()) {
			file.read(_PCMbuffer, PCM_BUFFERSIZE);
			file.read(_PCMbuffer1, PCM_BUFFERSIZE);
			_PCMbuffindex = 0;
			_PCMrelobuff = 0;
			_PCMbufftouse = 0;
			_PCMbuffindex = 0;

			_PCMfilePos = pos - PCM_HEADERSIZE;
		}
	}
	uint8_t _check(File &file) {
		uint8_t i = PCM_check(file);
		if (i == PCM_SONGEND)
		{
			checkEndOfSong();
		}
		return i == PCM_FILLING;
	};
#else
	LixMP3 mp3 = LixMP3();
	uint8_t _playFile(File &file, uint32_t size) {
		return false;
	};
	void _stop() {
	};
	void _pause() {
	};

	uint8_t _isPaused(void) {
		return false;
	}

	uint32_t _position(void) {
		return 0;
	};
	uint32_t _restDuration(void) {
		if (isstopped) return 0;
		else return 0;
	};
	uint32_t _duration(void) {
		return 0;
	};
	void _seek(int32_t amount) {
	};
	void _seekTo(uint32_t pos) {
	}
	uint8_t _check(File &file) {
		return 0;
	};
	void _readHeader(File &file, uint32_t size) {
	};
	void _printplaylist(Print &p) {
	};
#endif
	char currentFilename[PLAYER_FILELENGTHMAX];
#ifdef __LIX_WAVE__
	char realCurrentFilename[PLAYER_OFILEMAX];
	char currentFilePath[PLAYER_FILEPATHLENGTH];
#else
	char currentFilePath[PLAYER_FILELENGTHMAX];
#endif

	uint16_t _playlistIndex = 0;
	uint16_t fileCount = 0;

	uint16_t rd_lastFileList[256];
	uint16_t rd_playCount = 0;

	File file;
	PlayerRepeatType _repeatMode = Repeat_OFF;
	PlayerRandomType _randomMode = Random_OFF;
	uint8_t isstopped = true;
	uint8_t seeking = false;

	void checkRepeat(uint8_t endOfPlaylist) {
		Serial.println("SONGEND");
		switch (_repeatMode)
		{
		case Repeat_OFF:
			if (endOfPlaylist) stop();
			else next();
			break;
		case Repeat_Single:
			play();
			break;
		case Repeat_All:
			if (endOfPlaylist) {
				stop();
				play();
			}
			else next();
			break;
		}
	};
	uint8_t checkEndOfSong(void) {
		if (_randomMode == Random_All) {
			next();
		}
		else {
			checkRepeat(_playlistIndex + 1 >= fileCount);
		}
	}

	int16_t getRandom(void) {
		if (rd_playCount >= fileCount) return -1;
		for (uint16_t o = INT16_MAX; o > 0; o++) {
			uint16_t nextrd = random(0, fileCount);
			uint8_t c = false;
			uint16_t i = 0;
			for (i = 0; i <= rd_playCount; i++) {
				if (rd_lastFileList[i] == nextrd) {
					c = true;
					break;
				}
			}
			if (!c)	return nextrd;
		}
		return -1;
	};
	void resetRandom(void) {
		for (uint8_t i = 0; i < 64; i++) rd_lastFileList[i] = UINT16_MAX;
		randomSeed(analogRead(A11));
		rd_playCount = 0;
		//SERIAL.println("Reset Random");
	};

	void readHeader(void) {
		if (file) file.close();
		file = SD.open(currentFilePath);
		_readHeader(file, file.size());
		if (file) file.close();
	}
#ifdef __LIX_WAVE__
	void readFileCount(void) {
		File f;
		f = SD.open("list.txt"); //SD.open("list.txt");
		uint16_t cnt = 0;
		if (f) {
			uint32_t mil = millis();
			while (millis() - mil < 1000) {
				if (!f.available()) break;
				char c = (char)f.read();
				if (c == '\n') cnt++;
			}
			Serial.print("Files: "); Serial.println(cnt);
		}
		f.close();
		//rd_lastFileList = (uint16_t*)calloc(cnt, sizeof(uint16_t));

		fileCount = cnt;
	};
	uint16_t readPlaylist(int16_t index) {
		if (index < 0) index = fileCount - 1;
		else if (index >= fileCount) index = 0;
		File f;
		f = SD.open("list.txt");
		if (f) {
			uint16_t cnt = 0;
			uint32_t mil = millis();
			while (millis() - mil < 1000) {
				if (f.available()) {
					memset(realCurrentFilename, '\0', PLAYER_OFILEMAX);
					memset(currentFilename, '\0', PLAYER_FILELENGTHMAX);
					f.readBytesUntil('\t', realCurrentFilename, PLAYER_OFILEMAX);
					f.readBytesUntil('\n', currentFilename, PLAYER_FILELENGTHMAX);
					if (cnt == index) break;
					cnt++;
				}
				else break;
			}
			memset(currentFilePath, '\0', PLAYER_FILEPATHLENGTH);
			strcpy(currentFilePath, "/music/");
			strcat(currentFilePath, realCurrentFilename);
		}
		f.close();
		return index;
	};
#else
	void readFileCount(void) {
		uint16_t cnt = 0;
		File dir = SD.open("/music");
		while (true) {
			File entry = dir.openNextFile();
			if (!entry) {
				break;
			}
			if (!entry.isDirectory()) {
				Serial.print(entry.name());
				// files have sizes, directories do not
				Serial.print("\t");
				Serial.println(entry.size(), DEC);
				cnt++;
			}
			entry.close();
		}
		dir.close();
		fileCount = cnt;
	};
	uint16_t readPlaylist(int16_t index) {
		if (index < 0) index = fileCount - 1;
		else if (index >= fileCount) index = 0;

		uint16_t cnt = 0;
		File dir = SD.open("/music");
		memset(currentFilePath, '\0', PLAYER_OFILEMAX);
		memset(currentFilename, '\0', PLAYER_FILELENGTHMAX);
		while (true) {
			File entry = dir.openNextFile();
			if (!entry) {
				break;
			}
			if (!entry.isDirectory()) {
				Serial.print(entry.name());
				// files have sizes, directories do not
				Serial.print("\t");
				Serial.println(entry.size(), DEC);
				cnt++;
				if (cnt == index) {
					strcpy(currentFilePath, entry.name());
					ParseID3Header(entry, currentFilename);
					break;
				}
			}
			entry.close();
		}
		dir.close();

		return index;
	};
#endif // __LIX_WAVE__
};

#endif

//#else
//	pinMode(4, OUTPUT);
//	digitalWrite(4, HIGH);
//
//	if (Ethernet.begin(mac) == 0) {
//		SERIAL.println("Failed to configure Ethernet using DHCP");
//		Ethernet.begin(mac, ip);
//	}
//	else {
//		if (client.connect(server, 8080)) {
//			//printIPAddress();
//			SERIAL.println("connected");
//
//			client.println("GET /due/test.wav HTTP/1.1");
//			client.println("Host: www.lixpi.ml");
//			//client.println("Connection: close");
//			client.println();
//			while (!client.available());
//
//			bool currentLineIsBlank = false;
//			while (client.available()) {
//				char c = client.read();
//				if (c == '\n' && currentLineIsBlank) break;
//				if (c == '\n') currentLineIsBlank = true;
//				else if (c != '\r') currentLineIsBlank = false;
//			}
//			/*for (int i = 0; i < 500; i++) {
//			char c = client.read();
//			SERIAL.write(c);
//			}
//			return;*/
//			uint8_t header[PCM_HEADERSIZE];
//			client.read(header, PCM_HEADERSIZE);
//			_PCMparseHeader(header, client.available());
//
//			analogWriteResolution(12);
//
//			client.read(_PCMbuffer, PCM_BUFFERSIZE);
//			client.read(_PCMbuffer1, PCM_BUFFERSIZE);
//
//			_PCMfilePos = 0;
//			_PCMbufftouse = 0;
//			_PCMrelobuff = 0;
//			_PCMbuffindex = 0;
//			_PCMisPause = false;
//			_PCMisPlaying = true;
//
//			PCM_TIMER.attachInterrupt(_PCMisr16).setFrequency(44100).start();
//		}
//		else {
//			SERIAL.println("connection failed");
//		}
//	}
//#endif
//#ifdef ethernet_h
//	if (client.available()) {
//		if (_PCMrelobuff > 0) {
//			switch (_PCMrelobuff)
//			{
//			case 1:
//				//SERIAL.println("reload buff 1");
//				client.read(_PCMbuffer, PCM_BUFFERSIZE);
//				break;
//			case 2:
//				//SERIAL.println("reload buff 2");
//				client.read(_PCMbuffer1, PCM_BUFFERSIZE);
//				break;
//			}
//			_PCMrelobuff = 0;
//		}
//	}
//	if (!client.connected()) {
//		SERIAL.println();
//		SERIAL.println("disconnecting.");
//		client.stop();
//		PCM_TIMER.stop();
//		while (1);
//	}
//
//#else