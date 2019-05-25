#include "LixPlayer.h"
#include <SSD1306AsciiWire.h>
#include <SSD1306Ascii.h>
#include <Ethernet.h>

#define chapos(buffer, c)		((int)(buffer + (strchr((buffer), (c)) - (buffer))))
#define stripos(buffer, str)	((int)(buffer + (strstr((buffer), (str)) - (buffer))))

#define SERIAL Serial

#define SSD1306_WIDTH		128
#define SSD1306_HEIGHT		32
#define OLED_DISPLAYOFF		0xAE
#define OLED_DISPLAYON		0xAF
#define I2C_ADDRESS			0x3C
#define OLED_SLEEP		    1000 * 60//60s
SSD1306AsciiWire oled;
uint32_t oledNextSleep = 0;
uint32_t oledNextDraw = 0;
uint8_t oledSleeping = false;
char timeString[9];

uint32_t nextScroll = 0;
uint8_t scrollIndex = 0;

#define PIN_BUTTON1 51
#define PIN_BUTTON2 49
#define PIN_BUTTON3 53
volatile uint32_t nextBtn = 0;
volatile uint32_t btnTimings1 = 0;
volatile uint32_t btnTimings2 = 0;
volatile uint32_t btnTimings3 = 0;
volatile uint8_t btnPressed = 0;

#define MENUE_SUB_MAX 2
enum MenueSubItem
{
	Menue_SubItem_Random = 0,
	Menue_SubItem_Repeat = 1,
	Menue_SubItem_Sinus = 2
};
enum MenueType
{
	Menue_Type_Main = 0,
	Menue_Type_Settings = 1
};

MenueType menueType = Menue_Type_Main;
MenueSubItem menueSubitem = Menue_SubItem_Random;

uint32_t frequency = 0;
LixPlayer player;

//nicer symbol font

//make seek smoother (load file after seek complete) ???

#ifdef ethernet_h_
#define use_ethernet
#include <HttpClient.h>

#define HTML_PLAYLIST	'l'
#define HTML_INFO		'j'
#define HTML_NEXT		'f'
#define HTML_PREV		'r'
#define HTML_STOP		's'
#define HTML_PAUSE		'p'
#define HTML_SEEK		't'
#define HTML_SWITCH		'x'
#define HTML_INDEX		' '
#define HTML_REPEAT		'w'
#define HTML_RANDOM		'z'
#define HTML_READ		'g'
#define HTML_WRITE		'y'

const char PROGMEM *JSON_HEADER = "application/json";

#define _HTML_BUFFER_SIZE 65
#define HTML_BUFF_POS (_HTML_BUFFER_SIZE + 1)
#define HTML_BUFF_SIZE (_HTML_BUFFER_SIZE - 1)

#define JSON_BUFF_SIZE 256
char jsonBuffer[JSON_BUFF_SIZE];

const char *indexHTML PROGMEM = ("<!DOCTYPE html><head><title>Due Sound</title><link rel='icon' href='data:;base64,iVBORw0KGgo='><script src='https://lixmod.tk/s/jm.js'></script><script src='https://lixmod.tk/s/ma.js'></script><link rel='stylesheet' href='https://lixmod.tk/s/ma.css'></head><body></body></html>");
byte mac[] = { 0x00, 0xAA, 0xBB, 0x29, 0x02, 0x92 };
IPAddress ip(192, 168, 0, 129);
EthernetServer server(80);

void printHeader(EthernetClient &client, const char * content_type = "text/html", uint32_t contentSize = 0) {
	client.println("HTTP/1.1 200 OK");
	client.print("Content-Type: ");
	client.println(content_type);
	if (contentSize > 0) {
		client.print("Content-Length: ");
		client.println(contentSize);
	}
	client.println("Connection: close\r\n");
}
void printErrorHeader(EthernetClient &client, uint16_t error = 500) {
	client.print("HTTP/1.1 ");
	client.print(error);
	client.println(" ERROR\r\n");
}
void printJson(EthernetClient &client) {
	printHeader(client, JSON_HEADER);
	//1.4ms
	memset(jsonBuffer, '\0', JSON_BUFF_SIZE);
	int cx = 1;
	jsonBuffer[0] = '{';
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"t\":%d,", player.position());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"d\":%d,", player.duration());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"p\":%d,", player.isPaused());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"s\":%d,", player.isStopped());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"i\":%d,", player.playlistIndex());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"l\":%d,", player.playlistLength());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"z\":%d,", player.randomMode());
	cx += snprintf(jsonBuffer + cx, JSON_BUFF_SIZE, "\"r\":%d}", player.repeatMode());
	client.println(jsonBuffer);
}

void getUrl(char *name, size_t size) {
	char *nmstart = jsonBuffer + 7;
	memset(name, '\0', size);
	int pos = (int)(strchr(nmstart, ' ') - nmstart);
	strncat(name, nmstart, pos);
	for (uint8_t i = 0; i < size; i++) {
		if (name[i] == '\0') break;
		if (name[i] == '_') name[i] = '/';
	}
}
uint32_t readHeader(EthernetClient &client) {
	memset(jsonBuffer, '\0', JSON_BUFF_SIZE);
	client.read((uint8_t *)jsonBuffer, HTML_BUFF_SIZE);
	//SERIAL.println(jsonBuffer);

	uint32_t contentLength = 0;
	char *thead = jsonBuffer + HTML_BUFF_POS;
	while (client.available()) {
		memset(thead, '\0', HTML_BUFF_SIZE);
		client.readBytesUntil(' ', thead, HTML_BUFF_SIZE);
		if (strstr(thead, "Length:")) {
			memset(thead, '\0', HTML_BUFF_SIZE);
			client.readBytesUntil('\r', thead, HTML_BUFF_SIZE);
			contentLength = atol(thead);
			break;
		}
	}
	char hend = '\0';
	while (client.available()) {
		char c = client.read();
		//SERIAL.write(c);
		if (c == '\r') continue;
		if (c == '\n' && hend == '\n') {
			break;
		}
		hend = c;
	}
	return contentLength;
}

void htmlWriteFile(EthernetClient &client, uint32_t contentLength) {
	char name[33];
	getUrl(name, 33);
	if (SD.exists(name)) {
		printErrorHeader(client, 409);
		return;
	}

	char *jbuf = jsonBuffer + (HTML_BUFF_POS * 2);
	size_t siz = client.readBytesUntil('\n', jbuf, 64);

	File list = SD.open("list.txt", FILE_WRITE);
	if (list) {
		char *fnm = strchr(name, '/') + 1;
		list.print(fnm);
		list.print('\t');
		list.write(jbuf, siz);
		list.print('\n');
	}
	list.close();

	File f = SD.open(name, FILE_WRITE);
	if (f) {
		uint8_t buff[1024];
		uint32_t leng = siz;
		while (client.connected()) {
			if (client.available()) {
				size_t l = client.read(buff, 1024);
				f.write(buff, l);
				leng += l;
				if (leng >= contentLength) {
					break;
				}
			}
		}
		printHeader(client);
	}
	else {
		printErrorHeader(client);
	}
	f.close();
}
void htmlReadFile(EthernetClient &client) {
	char name[33];
	getUrl(name, 33);
	File f = SD.open(name, FILE_READ);
	if (f) {
		printHeader(client, "application/octet-stream", f.size());
		uint8_t buff[1024];
		while (f.available()) {
			if (!client.connected()) break;
			size_t l = f.read(buff, 1024);
			client.write(buff, l);
			//if (f.position() > 1024 * 1000) break;
		}
	}
	else {
		printErrorHeader(client, 404);
	}
	f.close();
}
void checkEthernet() {
	EthernetClient client = server.available();
	if (client) {
		if (client.connected()) {
			uint32_t contentLength = readHeader(client);

			if (jsonBuffer[5] == ' ' || jsonBuffer[6] == '?' || (jsonBuffer[6] == ' ' || (jsonBuffer[6] >= '0' && jsonBuffer[6] <= '9'))) {
				switch (jsonBuffer[5])
				{
				case HTML_INDEX: {
					printHeader(client);
					client.println(indexHTML);
					break; }
				case HTML_WRITE: {
					htmlWriteFile(client, contentLength);
					break; }

				case HTML_READ: {
					htmlReadFile(client);
					break; }

				case HTML_SEEK: {
					//set position	0-9999
					uint32_t pos = atol(jsonBuffer + 6);
					player.seekTo(pos);
					printJson(client);
					break; }

				case HTML_SWITCH: {
					uint16_t pos = atoi(jsonBuffer + 6);
					player.next(pos);
					printJson(client);
					break;
				}

				case HTML_INFO: {
					printJson(client);
					break; }

				case HTML_STOP: {
					player.stop();
					printJson(client);
					break; }

				case HTML_PAUSE: {
					player.pause();
					printJson(client);
					break; }

				case HTML_PREV: {
					player.prev();
					printJson(client);
					break; }

				case HTML_NEXT: {
					player.next();
					printJson(client);
					break; }

				case HTML_PLAYLIST: {
					printHeader(client, JSON_HEADER);
					player.printPlaylist(client);
					break; }

				case HTML_REPEAT: {
					player.repeatMode(jsonBuffer[6] - '0');
					printJson(client);
					break; }

				case HTML_RANDOM: {
					player.randomMode(jsonBuffer[6] - '0');
					printJson(client);
					break; }
				}
			}
			client.stop();
		}
	}
}
#endif

void clearOled() {
	oled.clear();
	scrollIndex = 0;
	oledNextDraw = 0;
	nextScroll = millis() + 3000;
}
void oledOn() {
	oledSleeping = false;
	oled.ssd1306WriteCmd(OLED_DISPLAYON);
	delayMicroseconds(1000);
	clearOled();
	oledNextSleep = millis() + OLED_SLEEP;
}
void drawSettings() {
	oled.set1X();
	oled.setFont(X11fixed7x14);
	oled.setCursor(0, 0);

	switch (menueSubitem)
	{
	case Menue_SubItem_Random:
		oled.println("Random Mode");
		switch (player.randomMode())
		{
		case Random_All:
			oled.println("Random All");
			break;
		case Random_OFF:
			oled.println("Random Off");
			break;
		}
		break;
	case Menue_SubItem_Repeat:
		oled.println("Repeat Mode");
		switch (player.repeatMode())
		{
		case Repeat_OFF:
			oled.println("Repeat Off");
			break;
		case Repeat_Single:
			oled.println("Repeat Song");
			break;
		case Repeat_All:
			oled.println("Repeat All");
			break;
		}
		break;
	case Menue_SubItem_Sinus:
		oled.println("Sinus");
		oled.print(frequency);
		oled.println("Hz");
		break;
	}
}
void drawOLED() {
	oled.set1X();
	oled.setFont(Verdana12);
	oled.setCursor(0, 0);

	size_t strw = oled.strWidth(player.CurrentFilename());
	if (strw > SSD1306_WIDTH) {
		//Scroll long filename
		char name[136];
		strcpy(name, player.CurrentFilename());
		strcat(name, "   ~   ");
		size_t nsize = strlen(name);
		if (scrollIndex > 0) oled.clearToEOL();

		uint8_t i = scrollIndex;
		for (; i < nsize; i++) {
			if (name[i] == '\0') break;
			oled.print(name[i]);
			if (oled.col() >= SSD1306_WIDTH) break;
		}
		if (oled.col() <= SSD1306_WIDTH) {
			for (i = 0; i < nsize; i++) {
				if (name[i] == '\0') break;
				oled.print(name[i]);
				if (oled.col() >= SSD1306_WIDTH) break;
			}
		}
		if (strw > 130 && millis() > nextScroll) {
			scrollIndex++;
			nextScroll = millis() + 100;
		}
		if (scrollIndex > nsize) {
			scrollIndex = 0;
			nextScroll = millis() + 1500;
		}
	}
	else {
		oled.print(player.CurrentFilename());
	}

	oled.setFont(Iain5x7);
	oled.set2X();
	uint16_t w = oled.strWidth("00:00:00 ");
	oled.clearField(8, 2, 3);
	oled.setCursor(0, 2);
	oled.print(getTimeString(timeString, player.restDuration()));

	oled.setCol(w);
	oled.print("#");//@ Track number c2b6
	oled.print(player.playlistIndex() + 1);

	if (player.playlistIndex() + 1 < 10) oled.print(' ');
	if (player.isStopped()) oled.print(" []");//? Stop df9b
	else if (player.isPaused()) oled.print(" ||"); //= Pause c781
	else oled.print(" > ");//> Play c2bb

	//draw repeatmode
}
void checkOled() {
	if (millis() > oledNextSleep) {
		if (!oledSleeping) {
			oled.ssd1306WriteCmd(OLED_DISPLAYOFF);
			oledSleeping = true;
		}
	}
	else if (millis() >= oledNextDraw) {
		//timeString = getTimeString(timeString, player.restDuration());
		switch (menueType)
		{
		case Menue_Type_Main:
			drawOLED();
			break;
		case Menue_Type_Settings:
			drawSettings();
			break;
		}
		oledNextDraw = millis() + 333;
	}
}

void ISR_btn1() {
	uint8_t p = digitalRead(PIN_BUTTON1);
	//SERIAL.println(p);
	if (!p) {
		if (millis() - btnTimings1 < 500) {
			btnPressed = 1;
		}
		else btnPressed = 0;
		btnTimings1 = 0;
	}
	else {
		btnTimings1 = millis();
		btnPressed = 0;
	}
}
void ISR_btn2() {
	uint8_t p = digitalRead(PIN_BUTTON2);
	//SERIAL.println(p);
	if (!p) {
		if (millis() - btnTimings2 < 500) {
			btnPressed = 2;
		}
		else btnPressed = 0;
		btnTimings2 = 0;
	}
	else {
		btnTimings2 = millis();
		btnPressed = 0;
	}
}
void ISR_btn3() {
	uint8_t p = digitalRead(PIN_BUTTON3);
	//SERIAL.println(p);
	if (!p) {
		if (millis() - btnTimings3 < 500) {
			btnPressed = 3;
		}
		else btnPressed = 0;
		btnTimings3 = 0;
	}
	else {
		btnTimings3 = millis();
		btnPressed = 0;
	}
}
void checkSubButtons(int8_t up) {
	switch (menueSubitem)
	{
	case Menue_SubItem_Random:
		up += player.randomMode();
		if (up > Random_All) up = 0;
		else if (up < 0) up = Random_All;
		player.randomMode(up);
		break;
	case Menue_SubItem_Repeat:
		up += player.repeatMode();
		if (up > Repeat_All) up = 0;
		else if (up < 0) up = Repeat_All;
		player.repeatMode(up);
		break;
	case Menue_SubItem_Sinus:
#ifdef __LIX_WAVE__
		frequency = constrain((up * 10) + frequency, 0, 132000);
		PCM_Sinus(frequency);
#endif
		break;
	}
}
void checkButtons() {
	if (millis() > nextBtn) {
		if (btnPressed > 0) {
			switch (btnPressed)
			{
			case 1:
				if (oledSleeping) {
					oledOn();
					break;
				}
				if (menueType == Menue_Type_Main) {
					player.pause();
				}
				else {
					menueSubitem = (MenueSubItem)((menueSubitem == MENUE_SUB_MAX) ? 0 : menueSubitem + 1);
					clearOled();
				}
				break;
			case 2:
				if (menueType == Menue_Type_Main) {
					player.prev();
				}
				else { checkSubButtons(-1); }
				clearOled();
				break;
			case 3:
				if (menueType == Menue_Type_Main) {
					player.next();
				}
				else { checkSubButtons(1); }/*			else {					menueSubitem = (MenueSubItem)((menueSubitem == 0) ? MENUE_SUB_MAX : menueSubitem - 1);				}*/
				clearOled();
				break;
			}
			oledNextSleep = millis() + OLED_SLEEP;
			btnPressed = 0;
			return;
		}
		uint8_t wasPressed = false;
		if (btnTimings1 > 0 && millis() - btnTimings1 >= 1000) {
			menueSubitem = Menue_SubItem_Random;
			menueType = menueType == Menue_Type_Settings ? Menue_Type_Main : Menue_Type_Settings;

			btnPressed = 0;
			wasPressed = true;

			clearOled();
			btnTimings1 = millis();
		}
		else if (btnTimings2 > 0 && millis() - btnTimings2 >= 1000) {
			player.seek(-2);
			btnPressed = 0;
			wasPressed = true;
		}
		else if (btnTimings3 > 0 && millis() - btnTimings3 >= 1000) {
			player.seek(2);
			btnPressed = 0;
			wasPressed = true;
		}
		nextBtn = millis() + (wasPressed) ? 50 : 100;
	}
}

void setup() {
	SERIAL.begin(115200U);
	uint32_t mil = millis();
	while (!SerialUSB)
	{
		if (millis() - mil > 1000) break;
	}

	//pinMode(47, OUTPUT);
	//digitalWrite(47, HIGH);

	noInterrupts();
	pinMode(PIN_BUTTON1, INPUT);
	pinMode(PIN_BUTTON2, INPUT);
	pinMode(PIN_BUTTON3, INPUT);
	attachInterrupt(digitalPinToInterrupt(PIN_BUTTON1), ISR_btn1, CHANGE);
	attachInterrupt(digitalPinToInterrupt(PIN_BUTTON2), ISR_btn2, CHANGE);
	attachInterrupt(digitalPinToInterrupt(PIN_BUTTON3), ISR_btn3, CHANGE);
	interrupts();
	delay(10);

	btnPressed = btnTimings1 = btnTimings2 = btnTimings3 = nextBtn = 0;

	Wire.begin();
	Wire.setClock(2000000U);

	oled.begin(&Adafruit128x32, I2C_ADDRESS);

	oled.setFont(System5x7);
	oled.clear();
	delay(500);

	SERIAL.println("Initializing SD card...");
	oled.println("Initializing SD card...");
	while (!SD.begin(F_CPU / 8, 4)) {
		SERIAL.println("SD initialization failed!");
		oled.println("SD init failed!");
		delay(1000);
	}

#ifdef use_ethernet
	SERIAL.println("Initializing Ethernet...");
	oled.println("Initializing Ethernet...");
	Ethernet.begin(mac, ip);
#endif

	player.begin();

	oled.clear();

	oledNextSleep = millis() + OLED_SLEEP;
}
void loop()
{
	//HttpClient client;
	//client.get("http://lixpi.ml:8080/test8bit.wav");

	if (!player.checkBuffer()) {
		checkButtons();
		checkOled();
		checkEthernet();
	}
}
//
//#include "Adafruit_MP3.h"
//#include <SPI.h>
//#include <SD.h>
//#include "MP3_Header.h"
//
//#define DEBUG
//
//const int chipSelect = 4;
//
//File dataFile;
//LixMP3 player;
//
//// the setup routine runs once when you press reset:
//void setup() {
//	// Open serial communications and wait for port to open:
//	Serial.begin(115200);
//
//	Serial.println("Native MP3 decoding!");
//	Serial.print("Initializing SD card...");
//
//	// see if the card is present and can be initialized:
//	analogWriteResolution(12);
//
//	while (!SD.begin(16000000, chipSelect)) {
//		Serial.println("Card failed, or not present");
//		delay(2000);
//	}
//	Serial.println("card initialized.");
//
//	dataFile = SD.open("test.mp3");
//	if (!dataFile) {
//		Serial.println("could not open file!");
//		while (1);
//	}
//	//Serial.println("cb.mp3");
//	char id3tags[256];
//	uint32_t pos = ParseID3Header(dataFile, id3tags);
//	uint32_t duration = GetMP3Duration(dataFile, pos);
//	Serial.print(duration / 60); Serial.print(":");	Serial.println(duration % 60);
//	Serial.println(id3tags);
//
//	player.begin();
//
//	player.play();
//}
//
//int16_t last = INT16_MAX;
//void loop() {
//	int16_t t = player.tick(dataFile);
//	Serial.println(t);
//}
