/*
  xdrv_42_i2s_audio.ino - audio dac support for Tasmota

  Copyright (C) 2021  Gerhard Mutz and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if  (defined(USE_I2S_AUDIO) || defined(USE_TTGO_WATCH) || defined(USE_M5STACK_CORE2))
#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include <ESP8266SAM.h>
#include "AudioFileSourceFS.h"
#ifdef SAY_TIME
#include "AudioGeneratorTalkie.h"
#endif
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorAAC.h"

#undef AUDIO_PWR_ON
#undef AUDIO_PWR_OFF
#define AUDIO_PWR_ON
#define AUDIO_PWR_OFF

#ifdef USE_TTGO_WATCH
#undef AUDIO_PWR_ON
#undef AUDIO_PWR_OFF
#define AUDIO_PWR_ON TTGO_audio_power(true);
#define AUDIO_PWR_OFF TTGO_audio_power(false);
#endif // USE_TTGO_WATCH

#ifdef USE_M5STACK_CORE2
#undef AUDIO_PWR_ON
#undef AUDIO_PWR_OFF
#define AUDIO_PWR_ON CORE2_audio_power(true);
#define AUDIO_PWR_OFF CORE2_audio_power(false);
#undef DAC_IIS_BCK
#undef DAC_IIS_WS
#undef DAC_IIS_DOUT
#define DAC_IIS_BCK       12
#define DAC_IIS_WS        0
#define DAC_IIS_DOUT      2
#endif // USE_M5STACK_CORE2


#define EXTERNAL_DAC_PLAY   1

#define XDRV_42           42

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceFS *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;
AudioGeneratorMP3 *decoder = NULL;
void *mp3ram = NULL;


extern FS *ufsp;

#ifdef ESP8266
const int preallocateBufferSize = 5*1024;
const int preallocateCodecSize = 29192; // MP3 codec max mem needed
#endif  // ESP8266
#ifdef ESP32
const int preallocateBufferSize = 16*1024;
const int preallocateCodecSize = 29192; // MP3 codec max mem needed
//const int preallocateCodecSize = 85332; // AAC+SBR codec max mem needed
#endif  // ESP32

#ifdef USE_WEBRADIO
AudioFileSourceICYStream *ifile = NULL;
AudioFileSourceBuffer *buff = NULL;
char wr_title[64];
//char status[64];

void *preallocateBuffer = NULL;
void *preallocateCodec = NULL;
uint32_t retryms = 0;
#endif // USE_WEBRADIO

#ifdef SAY_TIME
AudioGeneratorTalkie *talkie = nullptr;
#endif // SAY_TIME

//! MAX98357A + INMP441 DOUBLE I2S BOARD
#ifdef ESP8266
#undef DAC_IIS_BCK
#undef DAC_IIS_WS
#undef DAC_IIS_DOUT
#define DAC_IIS_BCK       15
#define DAC_IIS_WS        2
#define DAC_IIS_DOUT      3
#endif  // ESP8266

// defaults to TTGO WATCH
#ifdef ESP32
#ifndef DAC_IIS_BCK
#undef DAC_IIS_BCK
#define DAC_IIS_BCK       26
#endif // DAC_IIS_BCK

#ifndef DAC_IIS_WS
#undef DAC_IIS_WS
#define DAC_IIS_WS        25
#endif // DAC_IIS_WS

#ifndef DAC_IIS_DOUT
#undef DAC_IIS_DOUT
#define DAC_IIS_DOUT      33
#endif // DAC_IIS_DOUT

#ifndef DAC_IIS_DIN
#undef DAC_IIS_DIN
#define DAC_IIS_DIN      34
#endif // DAC_IIS_DIN

#endif  // ESP32

#ifdef SAY_TIME
long timezone = 2;
byte daysavetime = 1;

uint8_t spTHE[]       PROGMEM = {0x08,0xE8,0x3E,0x55,0x01,0xC3,0x86,0x27,0xAF,0x72,0x0D,0x4D,0x97,0xD5,0xBC,0x64,0x3C,0xF2,0x5C,0x51,0xF1,0x93,0x36,0x8F,0x4F,0x59,0x2A,0x42,0x7A,0x32,0xC3,0x64,0xFF,0x3F};
uint8_t spTIME[]      PROGMEM = {0x0E,0x28,0xAC,0x2D,0x01,0x5D,0xB6,0x0D,0x33,0xF3,0x54,0xB3,0x60,0xBA,0x8C,0x54,0x5C,0xCD,0x2D,0xD4,0x32,0x73,0x0F,0x8E,0x34,0x33,0xCB,0x4A,0x25,0xD4,0x25,0x83,0x2C,0x2B,0xD5,0x50,0x97,0x08,0x32,0xEC,0xD4,0xDC,0x4C,0x33,0xC8,0x70,0x73,0x0F,0x33,0xCD,0x20,0xC3,0xCB,0x43,0xDD,0x3C,0xCD,0x8C,0x20,0x77,0x89,0xF4,0x94,0xB2,0xE2,0xE2,0x35,0x22,0x5D,0xD6,0x4A,0x8A,0x96,0xCC,0x36,0x25,0x2D,0xC9,0x9A,0x7B,0xC2,0x18,0x87,0x24,0x4B,0x1C,0xC9,0x50,0x19,0x92,0x2C,0x71,0x34,0x4B,0x45,0x8A,0x8B,0xC4,0x96,0xB6,0x5A,0x29,0x2A,0x92,0x5A,0xCA,0x53,0x96,0x20,0x05,0x09,0xF5,0x92,0x5D,0xBC,0xE8,0x58,0x4A,0xDD,0xAE,0x73,0xBD,0x65,0x4B,0x8D,0x78,0xCA,0x2B,0x4E,0xD8,0xD9,0xED,0x22,0x20,0x06,0x75,0x00,0x00,0x80,0xFF,0x07};
uint8_t spIS[]        PROGMEM = {0x21,0x18,0x96,0x38,0xB7,0x14,0x8D,0x60,0x3A,0xA6,0xE8,0x51,0xB4,0xDC,0x2E,0x48,0x7B,0x5A,0xF1,0x70,0x1B,0xA3,0xEC,0x09,0xC6,0xCB,0xEB,0x92,0x3D,0xA7,0x69,0x1F,0xAF,0x71,0x89,0x9C,0xA2,0xB3,0xFC,0xCA,0x35,0x72,0x9A,0xD1,0xF0,0xAB,0x12,0xB3,0x2B,0xC6,0xCD,0x4F,0xCC,0x32,0x26,0x19,0x07,0xDF,0x0B,0x8F,0xB8,0xA4,0xED,0x7C,0xCF,0x23,0x62,0x8B,0x8E,0xF1,0x23,0x0A,0x8B,0x6E,0xCB,0xCE,0xEF,0x54,0x44,0x3C,0xDC,0x08,0x60,0x0B,0x37,0x01,0x1C,0x53,0x26,0x80,0x15,0x4E,0x14,0xB0,0x54,0x2B,0x02,0xA4,0x69,0xFF,0x7F};
uint8_t spA_M_[]      PROGMEM = {0xCD,0xEF,0x86,0xAB,0x57,0x6D,0x0F,0xAF,0x71,0xAD,0x49,0x55,0x3C,0xFC,0x2E,0xC5,0xB7,0x5C,0xF1,0xF2,0x87,0x66,0xDD,0x4E,0xC5,0xC3,0xEF,0x92,0xE2,0x3A,0x65,0xB7,0xA0,0x09,0xAA,0x1B,0x97,0x54,0x82,0x2E,0x28,0x77,0x5C,0x52,0x09,0x1A,0xA3,0xB8,0x76,0x49,0x25,0x68,0x8C,0x73,0xDB,0x24,0x95,0xA0,0x32,0xA9,0x6B,0xA7,0xD9,0x82,0x26,0xA9,0x76,0x42,0xD6,0x08,0xBA,0xE1,0xE8,0x0E,0x5A,0x2B,0xEA,0x9E,0x3D,0x27,0x18,0xAD,0xA8,0x07,0xF1,0x98,0x90,0x35,0xA2,0x96,0x44,0xA3,0x5D,0x66,0x8B,0x6B,0x12,0xCD,0x32,0x85,0x25,0xC9,0x81,0x2D,0xC3,0x64,0x85,0x34,0x58,0x89,0x94,0x52,0x1C,0x52,0x2F,0x35,0xDA,0xC7,0x51,0x48,0x23,0x97,0xCC,0x2C,0x97,0x2E,0xF3,0x5C,0xF3,0xA2,0x14,0xBA,0x2C,0x48,0xCE,0xCA,0x76,0xE8,0x32,0x2F,0x34,0xB2,0xDB,0x85,0xC9,0x83,0x90,0xA8,0x2C,0x57,0x26,0x8F,0x9C,0xBD,0xA2,0x53,0xD9,0xC2,0x54,0x59,0x28,0x99,0x4B,0x2C,0x5D,0xFF,0x3F};
uint8_t spP_M_[]      PROGMEM = {0x0E,0x98,0x41,0x54,0x00,0x43,0xA0,0x05,0xAB,0x42,0x8E,0x1D,0xA3,0x15,0xEC,0x4E,0x58,0xF7,0x92,0x66,0x70,0x1B,0x66,0xDB,0x73,0x99,0xC1,0xEB,0x98,0xED,0xD6,0x25,0x25,0x6F,0x70,0x92,0xDD,0x64,0xD8,0xFC,0x61,0xD0,0x66,0x83,0xD6,0x0A,0x86,0x23,0xAB,0x69,0xDA,0x2B,0x18,0x9E,0x3D,0x37,0x69,0x9D,0xA8,0x07,0x71,0x9F,0xA0,0xBD,0xA2,0x16,0xD5,0x7C,0x54,0xF6,0x88,0x6B,0x54,0x8B,0x34,0x49,0x2D,0x29,0x49,0x3C,0x34,0x64,0xA5,0x24,0x1B,0x36,0xD7,0x72,0x13,0x92,0xA4,0xC4,0x2D,0xC3,0xB3,0x4B,0xA3,0x62,0x0F,0x2B,0x37,0x6E,0x8B,0x5A,0xD4,0x3D,0xDD,0x9A,0x2D,0x50,0x93,0xF6,0x4C,0xAA,0xB6,0xC4,0x85,0x3B,0xB2,0xB1,0xD8,0x93,0x20,0x4D,0x8F,0x24,0xFF,0x0F};
uint8_t spOH[]        PROGMEM = {0xC6,0xC9,0x71,0x5A,0xA2,0x92,0x14,0x2F,0x6E,0x97,0x9C,0x46,0x9D,0xDC,0xB0,0x4D,0x62,0x1B,0x55,0x70,0xDD,0x55,0xBE,0x0E,0x36,0xC1,0x33,0x37,0xA9,0xA7,0x51,0x1B,0xCF,0x3C,0xA5,0x9E,0x44,0xAC,0x3C,0x7D,0x98,0x7B,0x52,0x96,0x72,0x65,0x4B,0xF6,0x1A,0xD9,0xCA,0xF5,0x91,0x2D,0xA2,0x2A,0x4B,0xF7,0xFF,0x01};
uint8_t spOCLOCK[]    PROGMEM = {0x21,0x4E,0x3D,0xB8,0x2B,0x19,0xBB,0x24,0x0E,0xE5,0xEC,0x60,0xE4,0xF2,0x90,0x13,0xD4,0x2A,0x11,0x80,0x00,0x42,0x69,0x26,0x40,0xD0,0x2B,0x04,0x68,0xE0,0x4D,0x00,0x3A,0x35,0x35,0x33,0xB6,0x51,0xD9,0x64,0x34,0x82,0xB4,0x9A,0x63,0x92,0x55,0x89,0x52,0x5B,0xCA,0x2E,0x34,0x25,0x4E,0x63,0x28,0x3A,0x50,0x95,0x26,0x8D,0xE6,0xAA,0x64,0x58,0xEA,0x92,0xCE,0xC2,0x46,0x15,0x9B,0x86,0xCD,0x2A,0x2E,0x37,0x00,0x00,0x00,0x0C,0xC8,0xDD,0x05,0x01,0xB9,0x33,0x21,0xA0,0x74,0xD7,0xFF,0x07};
uint8_t spONE[]       PROGMEM = {0xCC,0x67,0x75,0x42,0x59,0x5D,0x3A,0x4F,0x9D,0x36,0x63,0xB7,0x59,0xDC,0x30,0x5B,0x5C,0x23,0x61,0xF3,0xE2,0x1C,0xF1,0xF0,0x98,0xC3,0x4B,0x7D,0x39,0xCA,0x1D,0x2C,0x2F,0xB7,0x15,0xEF,0x70,0x79,0xBC,0xD2,0x46,0x7C,0x52,0xE5,0xF1,0x4A,0x6A,0xB3,0x71,0x47,0xC3,0x2D,0x39,0x34,0x4B,0x23,0x35,0xB7,0x7A,0x55,0x33,0x8F,0x59,0xDC,0xA2,0x44,0xB5,0xBC,0x66,0x72,0x8B,0x64,0xF5,0xF6,0x98,0xC1,0x4D,0x42,0xD4,0x27,0x62,0x38,0x2F,0x4A,0xB6,0x9C,0x88,0x68,0xBC,0xA6,0x95,0xF8,0x5C,0xA1,0x09,0x86,0x77,0x91,0x11,0x5B,0xFF,0x0F};
uint8_t spTWO[]       PROGMEM = {0x0E,0x38,0x6E,0x25,0x00,0xA3,0x0D,0x3A,0xA0,0x37,0xC5,0xA0,0x05,0x9E,0x56,0x35,0x86,0xAA,0x5E,0x8C,0xA4,0x82,0xB2,0xD7,0x74,0x31,0x22,0x69,0xAD,0x1C,0xD3,0xC1,0xD0,0xFA,0x28,0x2B,0x2D,0x47,0xC3,0x1B,0xC2,0xC4,0xAE,0xC6,0xCD,0x9C,0x48,0x53,0x9A,0xFF,0x0F};
uint8_t spTHREE[]     PROGMEM = {0x02,0xD8,0x2E,0x9C,0x01,0xDB,0xA6,0x33,0x60,0xFB,0x30,0x01,0xEC,0x20,0x12,0x8C,0xE4,0xD8,0xCA,0x32,0x96,0x73,0x63,0x41,0x39,0x89,0x98,0xC1,0x4D,0x0D,0xED,0xB0,0x2A,0x05,0x37,0x0F,0xB4,0xA5,0xAE,0x5C,0xDC,0x36,0xD0,0x83,0x2F,0x4A,0x71,0x7B,0x03,0xF7,0x38,0x59,0xCD,0xED,0x1E,0xB4,0x6B,0x14,0x35,0xB7,0x6B,0x94,0x99,0x91,0xD5,0xDC,0x26,0x48,0x77,0x4B,0x66,0x71,0x1B,0x21,0xDB,0x2D,0x8A,0xC9,0x6D,0x88,0xFC,0x26,0x28,0x3A,0xB7,0x21,0xF4,0x1F,0xA3,0x65,0xBC,0x02,0x38,0xBB,0x3D,0x8E,0xF0,0x2B,0xE2,0x08,0xB7,0x34,0xFF,0x0F};
uint8_t spFOUR[]      PROGMEM = {0x0C,0x18,0xB6,0x9A,0x01,0xC3,0x75,0x09,0x60,0xD8,0x0E,0x09,0x30,0xA0,0x9B,0xB6,0xA0,0xBB,0xB0,0xAA,0x16,0x4E,0x82,0xEB,0xEA,0xA9,0xFA,0x59,0x49,0x9E,0x59,0x23,0x9A,0x27,0x3B,0x78,0x66,0xAE,0x4A,0x9C,0x9C,0xE0,0x99,0xD3,0x2A,0xBD,0x72,0x92,0xEF,0xE6,0x88,0xE4,0x45,0x4D,0x7E,0x98,0x2D,0x62,0x67,0x37,0xF9,0xA1,0x37,0xA7,0x6C,0x94,0xE4,0xC7,0x1E,0xDC,0x3C,0xA5,0x83,0x1F,0x8B,0xEB,0x52,0x0E,0x0E,0x7E,0x2E,0x4E,0xC7,0x31,0xD2,0x79,0xA5,0x3A,0x0D,0xD9,0xC4,0xFF,0x07};
uint8_t spFIVE[]      PROGMEM = {0x02,0xE8,0x3E,0x8C,0x01,0xDD,0x65,0x08,0x60,0x98,0x4C,0x06,0x34,0x93,0xCE,0x80,0xE6,0xDA,0x9A,0x14,0x6B,0xAA,0x47,0xD1,0x5E,0x56,0xAA,0x6D,0x56,0xCD,0x78,0xD9,0xA9,0x1C,0x67,0x05,0x83,0xE1,0xA4,0xBA,0x38,0xEE,0x16,0x86,0x9B,0xFA,0x60,0x87,0x5B,0x18,0x6E,0xEE,0x8B,0x1D,0x6E,0x61,0xB9,0x69,0x36,0x65,0xBA,0x8D,0xE5,0xE5,0x3E,0x1C,0xE9,0x0E,0x96,0x9B,0x5B,0xAB,0x95,0x2B,0x58,0x6E,0xCE,0xE5,0x3A,0x6A,0xF3,0xB8,0x35,0x84,0x7B,0x05,0xA3,0xE3,0x36,0xEF,0x92,0x19,0xB4,0x86,0xDB,0xB4,0x69,0xB4,0xD1,0x2A,0x4E,0x65,0x9A,0x99,0xCE,0x28,0xD9,0x85,0x71,0x4C,0x18,0x6D,0x67,0x47,0xC6,0x5E,0x53,0x4A,0x9C,0xB5,0xE2,0x85,0x45,0x26,0xFE,0x7F};
uint8_t spSIX[]       PROGMEM = {0x0E,0xD8,0xAE,0xDD,0x03,0x0E,0x38,0xA6,0xD2,0x01,0xD3,0xB4,0x2C,0xAD,0x6A,0x35,0x9D,0xB1,0x7D,0xDC,0xEE,0xC4,0x65,0xD7,0xF1,0x72,0x47,0x24,0xB3,0x19,0xD9,0xD9,0x05,0x70,0x40,0x49,0xEA,0x02,0x98,0xBE,0x42,0x01,0xDF,0xA4,0x69,0x40,0x00,0xDF,0x95,0xFC,0x3F};
uint8_t spSEVEN[]     PROGMEM = {0x02,0xB8,0x3A,0x8C,0x01,0xDF,0xA4,0x73,0x40,0x01,0x47,0xB9,0x2F,0x33,0x3B,0x73,0x5F,0x53,0x7C,0xEC,0x9A,0xC5,0x63,0xD5,0xD1,0x75,0xAE,0x5B,0xFC,0x64,0x5C,0x35,0x87,0x91,0xF1,0x83,0x36,0xB5,0x68,0x55,0xC5,0x6F,0xDA,0x45,0x2D,0x1C,0x2D,0xB7,0x38,0x37,0x9F,0x60,0x3C,0xBC,0x9A,0x85,0xA3,0x25,0x66,0xF7,0x8A,0x57,0x1C,0xA9,0x67,0x56,0xCA,0x5E,0xF0,0xB2,0x16,0xB2,0xF1,0x89,0xCE,0x8B,0x92,0x25,0xC7,0x2B,0x33,0xCF,0x48,0xB1,0x99,0xB4,0xF3,0xFF};
uint8_t spEIGHT[]     PROGMEM = {0xC3,0x6C,0x86,0xB3,0x27,0x6D,0x0F,0xA7,0x48,0x99,0x4E,0x55,0x3C,0xBC,0x22,0x65,0x36,0x4D,0xD1,0xF0,0x32,0xD3,0xBE,0x34,0xDA,0xC3,0xEB,0x82,0xE2,0xDA,0x65,0x35,0xAF,0x31,0xF2,0x6B,0x97,0x95,0xBC,0x86,0xD8,0x6F,0x82,0xA6,0x73,0x0B,0xC6,0x9E,0x72,0x99,0xCC,0xCB,0x02,0xAD,0x3C,0x9A,0x10,0x60,0xAB,0x62,0x05,0x2C,0x37,0x84,0x00,0xA9,0x73,0x00,0x00,0xFE,0x1F};
uint8_t spNINE[]      PROGMEM = {0xCC,0xA1,0x26,0xBB,0x83,0x93,0x18,0xCF,0x4A,0xAD,0x2E,0x31,0xED,0x3C,0xA7,0x24,0x26,0xC3,0x54,0xF1,0x92,0x64,0x8B,0x8A,0x98,0xCB,0x2B,0x2E,0x34,0x53,0x2D,0x0E,0x2F,0x57,0xB3,0x0C,0x0D,0x3C,0xBC,0x3C,0x4C,0x4B,0xCA,0xF4,0xF0,0x72,0x0F,0x6E,0x49,0x53,0xCD,0xCB,0x53,0x2D,0x35,0x4D,0x0F,0x2F,0x0F,0xD7,0x0C,0x0D,0x3D,0xBC,0xDC,0x4D,0xD3,0xDD,0xC2,0xF0,0x72,0x52,0x4F,0x57,0x9B,0xC3,0xAB,0x89,0xBD,0x42,0x2D,0x0F,0xAF,0x5A,0xD1,0x71,0x91,0x55,0xBC,0x2C,0xC5,0x3B,0xD8,0x65,0xF2,0x82,0x94,0x18,0x4E,0x3B,0xC1,0x73,0x42,0x32,0x33,0x15,0x45,0x4F,0x79,0x52,0x6A,0x55,0xA6,0xA3,0xFF,0x07};
uint8_t spTEN[]       PROGMEM = {0x0E,0xD8,0xB1,0xDD,0x01,0x3D,0xA8,0x24,0x7B,0x04,0x27,0x76,0x77,0xDC,0xEC,0xC2,0xC5,0x23,0x84,0xCD,0x72,0x9A,0x51,0xF7,0x62,0x45,0xC7,0xEB,0x4E,0x35,0x4A,0x14,0x2D,0xBF,0x45,0xB6,0x0A,0x75,0xB8,0xFC,0x16,0xD9,0x2A,0xD9,0xD6,0x0A,0x5A,0x10,0xCD,0xA2,0x48,0x23,0xA8,0x81,0x35,0x4B,0x2C,0xA7,0x20,0x69,0x0A,0xAF,0xB6,0x15,0x82,0xA4,0x29,0x3C,0xC7,0x52,0x08,0xA2,0x22,0xCF,0x68,0x4B,0x2E,0xF0,0x8A,0xBD,0xA3,0x2C,0xAB,0x40,0x1B,0xCE,0xAA,0xB2,0x6C,0x82,0x40,0x4D,0x7D,0xC2,0x89,0x88,0x8A,0x61,0xCC,0x74,0xD5,0xFF,0x0F};
uint8_t spELEVEN[]    PROGMEM = {0xC3,0xCD,0x76,0x5C,0xAE,0x14,0x0F,0x37,0x9B,0x71,0xDE,0x92,0x55,0xBC,0x2C,0x27,0x70,0xD3,0x76,0xF0,0x83,0x5E,0xA3,0x5E,0x5A,0xC1,0xF7,0x61,0x58,0xA7,0x19,0x35,0x3F,0x99,0x31,0xDE,0x52,0x74,0xFC,0xA2,0x26,0x64,0x4B,0xD1,0xF1,0xAB,0xAE,0xD0,0x2D,0xC5,0xC7,0x2F,0x36,0xDD,0x27,0x15,0x0F,0x3F,0xD9,0x08,0x9F,0x62,0xE4,0xC2,0x2C,0xD4,0xD8,0xD3,0x89,0x0B,0x1B,0x57,0x11,0x0B,0x3B,0xC5,0xCF,0xD6,0xCC,0xC6,0x64,0x35,0xAF,0x18,0x73,0x1F,0xA1,0x5D,0xBC,0x62,0x45,0xB3,0x45,0x51,0xF0,0xA2,0x62,0xAB,0x4A,0x5B,0xC9,0x4B,0x8A,0x2D,0xB3,0x6C,0x06,0x2F,0x29,0xB2,0xAC,0x8A,0x18,0xBC,0x28,0xD9,0xAA,0xD2,0x92,0xF1,0xBC,0xE0,0x98,0x8C,0x48,0xCC,0x17,0x52,0xA3,0x27,0x6D,0x93,0xD0,0x4B,0x8E,0x0E,0x77,0x02,0x00,0xFF,0x0F};
uint8_t spTWELVE[]    PROGMEM = {0x06,0x28,0x46,0xD3,0x01,0x25,0x06,0x13,0x20,0xBA,0x70,0x70,0xB6,0x79,0xCA,0x36,0xAE,0x28,0x38,0xE1,0x29,0xC5,0x35,0xA3,0xE6,0xC4,0x16,0x6A,0x53,0x8C,0x97,0x9B,0x72,0x86,0x4F,0x28,0x1A,0x6E,0x0A,0x59,0x36,0xAE,0x68,0xF8,0x29,0x67,0xFA,0x06,0xA3,0x16,0xC4,0x96,0xE6,0x53,0xAC,0x5A,0x9C,0x56,0x72,0x77,0x31,0x4E,0x49,0x5C,0x8D,0x5B,0x29,0x3B,0x24,0x61,0x1E,0x6C,0x9B,0x6C,0x97,0xF8,0xA7,0x34,0x19,0x92,0x4C,0x62,0x9E,0x72,0x65,0x58,0x12,0xB1,0x7E,0x09,0xD5,0x2E,0x53,0xC5,0xBA,0x36,0x6B,0xB9,0x2D,0x17,0x05,0xEE,0x9A,0x6E,0x8E,0x05,0x50,0x6C,0x19,0x07,0x18,0x50,0xBD,0x3B,0x01,0x92,0x08,0x41,0x40,0x10,0xA6,0xFF,0x0F};
uint8_t spTHIRTEEN[]  PROGMEM = {0x08,0xE8,0x2C,0x15,0x01,0x43,0x07,0x13,0xE0,0x98,0xB4,0xA6,0x35,0xA9,0x1E,0xDE,0x56,0x8E,0x53,0x9C,0x7A,0xE7,0xCA,0x5E,0x76,0x8D,0x94,0xE5,0x2B,0xAB,0xD9,0xB5,0x62,0xA4,0x9C,0xE4,0xE6,0xB4,0x41,0x1E,0x7C,0xB6,0x93,0xD7,0x16,0x99,0x5A,0xCD,0x61,0x76,0x55,0xC2,0x91,0x61,0x1B,0xC0,0x01,0x5D,0x85,0x05,0xE0,0x68,0x51,0x07,0x1C,0xA9,0x64,0x80,0x1D,0x4C,0x9C,0x95,0x88,0xD4,0x04,0x3B,0x4D,0x4E,0x21,0x5C,0x93,0xA8,0x26,0xB9,0x05,0x4B,0x6E,0xA0,0xE2,0xE4,0x57,0xC2,0xB9,0xC1,0xB2,0x93,0x5F,0x09,0xD7,0x24,0xCB,0x4E,0x41,0x25,0x54,0x1D,0x62,0x3B,0x05,0x8D,0x52,0x57,0xAA,0xAD,0x10,0x24,0x26,0xE3,0xE1,0x36,0x5D,0x10,0x85,0xB4,0x97,0x85,0x72,0x41,0x14,0x52,0x5E,0x1A,0xCA,0xF9,0x91,0x6B,0x7A,0x5B,0xC4,0xE0,0x17,0x2D,0x54,0x1D,0x92,0x8C,0x1F,0x25,0x4B,0x8F,0xB2,0x16,0x41,0xA1,0x4A,0x3E,0xE6,0xFA,0xFF,0x01};
uint8_t spFOURTEEN[]  PROGMEM = {0x0C,0x58,0xAE,0x5C,0x01,0xD9,0x87,0x07,0x51,0xB7,0x25,0xB3,0x8A,0x15,0x2C,0xF7,0x1C,0x35,0x87,0x4D,0xB2,0xDD,0x53,0xCE,0x28,0x2B,0xC9,0x0E,0x97,0x2D,0xBD,0x2A,0x17,0x27,0x76,0x8E,0xD2,0x9A,0x6C,0x80,0x94,0x71,0x00,0x00,0x02,0xB0,0x58,0x58,0x00,0x9E,0x0B,0x0A,0xC0,0xB2,0xCE,0xC1,0xC8,0x98,0x7A,0x52,0x95,0x24,0x2B,0x11,0xED,0x36,0xD4,0x92,0xDC,0x4C,0xB5,0xC7,0xC8,0x53,0xF1,0x2A,0xE5,0x1A,0x17,0x55,0xC5,0xAF,0x94,0xBB,0xCD,0x1C,0x26,0xBF,0x52,0x9A,0x72,0x53,0x98,0xFC,0xC2,0x68,0xD2,0x4D,0x61,0xF0,0xA3,0x90,0xB6,0xD6,0x50,0xC1,0x8F,0x42,0xDA,0x4A,0x43,0x39,0x3F,0x48,0x2D,0x6B,0x33,0xF9,0xFF};
uint8_t spFIFTEEN[]   PROGMEM = {0x08,0xE8,0x2A,0x0D,0x01,0xDD,0xBA,0x31,0x60,0x6A,0xF7,0xA0,0xAE,0x54,0xAA,0x5A,0x76,0x97,0xD9,0x34,0x69,0xEF,0x32,0x1E,0x66,0xE1,0xE2,0xB3,0x43,0xA9,0x18,0x55,0x92,0x4E,0x37,0x2D,0x67,0x6F,0xDF,0xA2,0x5A,0xB6,0x04,0x30,0x55,0xA8,0x00,0x86,0x09,0xE7,0x00,0x01,0x16,0x17,0x05,0x70,0x40,0x57,0xE5,0x01,0xF8,0x21,0x34,0x00,0xD3,0x19,0x33,0x80,0x89,0x9A,0x62,0x34,0x4C,0xD5,0x49,0xAE,0x8B,0x53,0x09,0xF7,0x26,0xD9,0x6A,0x7E,0x23,0x5C,0x13,0x12,0xB3,0x04,0x9D,0x50,0x4F,0xB1,0xAD,0x14,0x15,0xC2,0xD3,0xA1,0xB6,0x42,0x94,0xA8,0x8C,0x87,0xDB,0x74,0xB1,0x70,0x59,0xE1,0x2E,0xC9,0xC5,0x81,0x5B,0x55,0xA4,0x4C,0x17,0x47,0xC1,0x6D,0xE3,0x81,0x53,0x9C,0x84,0x6A,0x46,0xD9,0x4C,0x51,0x31,0x42,0xD9,0x66,0xC9,0x44,0x85,0x29,0x6A,0x9B,0xAD,0xFF,0x07};
uint8_t spSIXTEEN[]   PROGMEM = {0x0A,0x58,0x5A,0x5D,0x00,0x93,0x97,0x0B,0x60,0xA9,0x48,0x05,0x0C,0x15,0xAE,0x80,0xAD,0x3D,0x14,0x30,0x7D,0xD9,0x50,0x92,0x92,0xAC,0x0D,0xC5,0xCD,0x2A,0x82,0xAA,0x3B,0x98,0x04,0xB3,0x4A,0xC8,0x9A,0x90,0x05,0x09,0x68,0x51,0xD4,0x01,0x23,0x9F,0x1A,0x60,0xA9,0x12,0x03,0xDC,0x50,0x81,0x80,0x22,0xDC,0x20,0x00,0xCB,0x06,0x3A,0x60,0x16,0xE3,0x64,0x64,0x42,0xDD,0xCD,0x6A,0x8A,0x5D,0x28,0x75,0x07,0xA9,0x2A,0x5E,0x65,0x34,0xED,0x64,0xBB,0xF8,0x85,0xF2,0x94,0x8B,0xAD,0xE4,0x37,0x4A,0x5B,0x21,0xB6,0x52,0x50,0x19,0xAD,0xA7,0xD8,0x4A,0x41,0x14,0xDA,0x5E,0x12,0x3A,0x04,0x91,0x4B,0x7B,0x69,0xA8,0x10,0x24,0x2E,0xE5,0xA3,0x81,0x52,0x90,0x94,0x5A,0x55,0x98,0x32,0x41,0x50,0xCC,0x93,0x2E,0x47,0x85,0x89,0x1B,0x5B,0x5A,0x62,0x04,0x44,0xE3,0x02,0x80,0x80,0x64,0xDD,0xFF,0x1F};
uint8_t spSEVENTEEN[] PROGMEM = {0x02,0x98,0x3A,0x42,0x00,0x5B,0xA6,0x09,0x60,0xDB,0x52,0x06,0x1C,0x93,0x29,0x80,0xA9,0x52,0x87,0x9A,0xB5,0x99,0x4F,0xC8,0x3E,0x46,0xD6,0x5E,0x7E,0x66,0xFB,0x98,0xC5,0x5A,0xC6,0x9A,0x9C,0x63,0x15,0x6B,0x11,0x13,0x8A,0x9C,0x97,0xB9,0x9A,0x5A,0x39,0x71,0xEE,0xD2,0x29,0xC2,0xA6,0xB8,0x58,0x59,0x99,0x56,0x14,0xA3,0xE1,0x26,0x19,0x19,0xE3,0x8C,0x93,0x17,0xB4,0x46,0xB5,0x88,0x71,0x9E,0x97,0x9E,0xB1,0x2C,0xC5,0xF8,0x56,0xC4,0x58,0xA3,0x1C,0xE1,0x33,0x9D,0x13,0x41,0x8A,0x43,0x58,0xAD,0x95,0xA9,0xDB,0x36,0xC0,0xD1,0xC9,0x0E,0x58,0x4E,0x45,0x01,0x23,0xA9,0x04,0x37,0x13,0xAE,0x4D,0x65,0x52,0x82,0xCA,0xA9,0x37,0x99,0x4D,0x89,0xBA,0xC0,0xBC,0x14,0x36,0x25,0xEA,0x1C,0x73,0x52,0x1D,0x97,0xB8,0x33,0xAC,0x0E,0x75,0x9C,0xE2,0xCE,0xB0,0xDA,0xC3,0x51,0x4A,0x1A,0xA5,0xCA,0x70,0x5B,0x21,0xCE,0x4C,0x26,0xD2,0x6C,0xBA,0x38,0x71,0x2E,0x1F,0x2D,0xED,0xE2,0x24,0xB8,0xBC,0x3D,0x52,0x88,0xAB,0x50,0x8E,0xA8,0x48,0x22,0x4E,0x42,0xA0,0x26,0x55,0xFD,0x3F};
uint8_t spEIGHTEEN[]  PROGMEM = {0x2E,0x9C,0xD1,0x4D,0x54,0xEC,0x2C,0xBF,0x1B,0x8A,0x99,0x70,0x7C,0xFC,0x2E,0x29,0x6F,0x52,0xF6,0xF1,0xBA,0x20,0xBF,0x36,0xD9,0xCD,0xED,0x0C,0xF3,0x27,0x64,0x17,0x73,0x2B,0xA2,0x99,0x90,0x65,0xEC,0xED,0x40,0x73,0x32,0x12,0xB1,0xAF,0x30,0x35,0x0B,0xC7,0x00,0xE0,0x80,0xAE,0xDD,0x1C,0x70,0x43,0xAA,0x03,0x86,0x51,0x36,0xC0,0x30,0x64,0xCE,0x4C,0x98,0xFB,0x5C,0x65,0x07,0xAF,0x10,0xEA,0x0B,0x66,0x1B,0xFC,0x46,0xA8,0x3E,0x09,0x4D,0x08,0x2A,0xA6,0x3E,0x67,0x36,0x21,0x2A,0x98,0x67,0x9D,0x15,0xA7,0xA8,0x60,0xEE,0xB6,0x94,0x99,0xA2,0x4A,0x78,0x22,0xC2,0xA6,0x8B,0x8C,0x8E,0xCC,0x4C,0x8A,0x2E,0x8A,0x4C,0xD3,0x57,0x03,0x87,0x28,0x71,0x09,0x1F,0x2B,0xE4,0xA2,0xC4,0xC5,0x6D,0xAD,0x54,0x88,0xB2,0x63,0xC9,0xF2,0x50,0x2E,0x8A,0x4A,0x38,0x4A,0xEC,0x88,0x28,0x08,0xE3,0x28,0x49,0xF3,0xFF};
uint8_t spNINETEEN[]  PROGMEM = {0xC2,0xEA,0x8A,0x95,0x2B,0x6A,0x05,0x3F,0x71,0x71,0x5F,0x0D,0x12,0xFC,0x28,0x25,0x62,0x35,0xF0,0xF0,0xB3,0x48,0x1E,0x0F,0xC9,0xCB,0x2F,0x45,0x7C,0x2C,0x25,0x1F,0xBF,0x14,0xB3,0x2C,0xB5,0x75,0xFC,0x5A,0x5C,0xA3,0x5D,0xE1,0xF1,0x7A,0x76,0xB3,0x4E,0x45,0xC7,0xED,0x96,0x23,0x3B,0x18,0x37,0x7B,0x18,0xCC,0x09,0x51,0x13,0x4C,0xAB,0x6C,0x4C,0x4B,0x96,0xD2,0x49,0xAA,0x36,0x0B,0xC5,0xC2,0x20,0x26,0x27,0x35,0x63,0x09,0x3D,0x30,0x8B,0xF0,0x48,0x5C,0xCA,0x61,0xDD,0xCB,0xCD,0x91,0x03,0x8E,0x4B,0x76,0xC0,0xCC,0x4D,0x06,0x98,0x31,0x31,0x98,0x99,0x70,0x6D,0x2A,0xA3,0xE4,0x16,0xCA,0xBD,0xCE,0x5C,0x92,0x57,0x28,0xCF,0x09,0x69,0x2E,0x7E,0xA5,0x3C,0x63,0xA2,0x30,0x05,0x95,0xD2,0x74,0x98,0xCD,0x14,0x54,0xCA,0x53,0xA9,0x96,0x52,0x50,0x28,0x6F,0xBA,0xCB,0x0C,0x41,0x50,0xDE,0x65,0x2E,0xD3,0x05,0x89,0x4B,0x7B,0x6B,0x20,0x17,0x44,0xAE,0xED,0x23,0x81,0x52,0x90,0x85,0x73,0x57,0xD0,0x72,0x41,0xB1,0x02,0xDE,0x2E,0xDB,0x04,0x89,0x05,0x79,0xBB,0x62,0xE5,0x76,0x11,0xCA,0x61,0x0E,0xFF,0x1F};
uint8_t spTWENTY[]    PROGMEM = {0x01,0x98,0xD1,0xC2,0x00,0xCD,0xA4,0x32,0x20,0x79,0x13,0x04,0x28,0xE7,0x92,0xDC,0x70,0xCC,0x5D,0xDB,0x76,0xF3,0xD2,0x32,0x0B,0x0B,0x5B,0xC3,0x2B,0xCD,0xD4,0xDD,0x23,0x35,0xAF,0x44,0xE1,0xF0,0xB0,0x6D,0x3C,0xA9,0xAD,0x3D,0x35,0x0E,0xF1,0x0C,0x8B,0x28,0xF7,0x34,0x01,0x68,0x22,0xCD,0x00,0xC7,0xA4,0x04,0xBB,0x32,0xD6,0xAC,0x56,0x9C,0xDC,0xCA,0x28,0x66,0x53,0x51,0x70,0x2B,0xA5,0xBC,0x0D,0x9A,0xC1,0xEB,0x14,0x73,0x37,0x29,0x19,0xAF,0x33,0x8C,0x3B,0xA7,0x24,0xBC,0x42,0xB0,0xB7,0x59,0x09,0x09,0x3C,0x96,0xE9,0xF4,0x58,0xFF,0x0F};
uint8_t spTHIRTY[]    PROGMEM = {0x08,0x98,0xD6,0x15,0x01,0x43,0xBB,0x0A,0x20,0x1B,0x8B,0xE5,0x16,0xA3,0x1E,0xB6,0xB6,0x96,0x97,0x3C,0x57,0xD4,0x2A,0x5E,0x7E,0x4E,0xD8,0xE1,0x6B,0x7B,0xF8,0x39,0x63,0x0D,0x9F,0x95,0xE1,0xE7,0x4C,0x76,0xBC,0x91,0x5B,0x90,0x13,0xC6,0x68,0x57,0x4E,0x41,0x8B,0x10,0x5E,0x1D,0xA9,0x44,0xD3,0xBA,0x47,0xB8,0xDD,0xE4,0x35,0x86,0x11,0x93,0x94,0x92,0x5F,0x29,0xC7,0x4C,0x30,0x0C,0x41,0xC5,0x1C,0x3B,0x2E,0xD3,0x05,0x15,0x53,0x6C,0x07,0x4D,0x15,0x14,0x8C,0xB5,0xC9,0x6A,0x44,0x90,0x10,0x4E,0x9A,0xB6,0x21,0x81,0x23,0x3A,0x91,0x91,0xE8,0xFF,0x01};
uint8_t spFOURTY[]    PROGMEM = {0x04,0x18,0xB6,0x4C,0x00,0xC3,0x56,0x30,0xA0,0xE8,0xF4,0xA0,0x98,0x99,0x62,0x91,0xAE,0x83,0x6B,0x77,0x89,0x78,0x3B,0x09,0xAE,0xBD,0xA6,0x1E,0x63,0x3B,0x79,0x7E,0x71,0x5A,0x8F,0x95,0xE6,0xA5,0x4A,0x69,0xB9,0x4E,0x8A,0x5F,0x12,0x56,0xE4,0x58,0x69,0xE1,0x36,0xA1,0x69,0x2E,0x2B,0xF9,0x95,0x93,0x55,0x17,0xED,0xE4,0x37,0xC6,0xBA,0x93,0xB2,0x92,0xDF,0x19,0xD9,0x6E,0xC8,0x0A,0xFE,0x60,0xE8,0x37,0x21,0xC9,0xF9,0x8D,0x61,0x5F,0x32,0x13,0xE7,0x17,0x4C,0xD3,0xC6,0xB1,0x94,0x97,0x10,0x8F,0x8B,0xAD,0x11,0x7E,0xA1,0x9A,0x26,0x92,0xF6,0xFF,0x01};
uint8_t spFIFTY[]     PROGMEM = {0x08,0xE8,0x2E,0x84,0x00,0x23,0x84,0x13,0x60,0x38,0x95,0xA5,0x0F,0xCF,0xE2,0x79,0x8A,0x8F,0x37,0x02,0xB3,0xD5,0x2A,0x6E,0x5E,0x93,0x94,0x79,0x45,0xD9,0x05,0x5D,0x0A,0xB9,0x97,0x63,0x02,0x74,0xA7,0x82,0x80,0xEE,0xC3,0x10,0xD0,0x7D,0x28,0x03,0x6E,0x14,0x06,0x70,0xE6,0x0A,0xC9,0x9A,0x4E,0x37,0xD9,0x95,0x51,0xCE,0xBA,0xA2,0x14,0x0C,0x81,0x36,0x1B,0xB2,0x5C,0x30,0x38,0xFA,0x9C,0xC9,0x32,0x41,0xA7,0x18,0x3B,0xA2,0x48,0x04,0x05,0x51,0x4F,0x91,0x6D,0x12,0x04,0x20,0x9B,0x61,0x89,0xFF,0x1F};
uint8_t spGOOD[]      PROGMEM = {0x0A,0x28,0xCD,0x34,0x20,0xD9,0x1A,0x45,0x74,0xE4,0x66,0x24,0xAD,0xBA,0xB1,0x8C,0x9B,0x91,0xA5,0x64,0xE6,0x98,0x21,0x16,0x0B,0x96,0x9B,0x4C,0xE5,0xFF,0x01};
uint8_t spMORNING[]   PROGMEM = {0xCE,0x08,0x52,0x2A,0x35,0x5D,0x39,0x53,0x29,0x5B,0xB7,0x0A,0x15,0x0C,0xEE,0x2A,0x42,0x56,0x66,0xD2,0x55,0x2E,0x37,0x2F,0xD9,0x45,0xB3,0xD3,0xC5,0xCA,0x6D,0x27,0xD5,0xEE,0x50,0xF5,0x50,0x94,0x14,0x77,0x2D,0xD8,0x5D,0x49,0x92,0xFD,0xB1,0x64,0x2F,0xA9,0x49,0x0C,0x93,0x4B,0xAD,0x19,0x17,0x3E,0x66,0x1E,0xF1,0xA2,0x5B,0x84,0xE2,0x29,0x8F,0x8B,0x72,0x10,0xB5,0xB1,0x2E,0x4B,0xD4,0x45,0x89,0x4A,0xEC,0x5C,0x95,0x14,0x2B,0x8A,0x9C,0x34,0x52,0x5D,0xBC,0xCC,0xB5,0x3B,0x49,0x69,0x89,0x87,0xC1,0x98,0x56,0x3A,0x21,0x2B,0x82,0x67,0xCC,0x5C,0x85,0xB5,0x4A,0x8A,0xF6,0x64,0xA9,0x96,0xC4,0x69,0x3C,0x52,0x81,0x58,0x1C,0x97,0xF6,0x0E,0x1B,0xCC,0x0D,0x42,0x32,0xAA,0x65,0x12,0x67,0xD4,0x6A,0x61,0x52,0xFC,0xFF};
uint8_t spAFTERNOON[] PROGMEM = {0xC7,0xCE,0xCE,0x3A,0xCB,0x58,0x1F,0x3B,0x07,0x9D,0x28,0x71,0xB4,0xAC,0x9C,0x74,0x5A,0x42,0x55,0x33,0xB2,0x93,0x0A,0x09,0xD4,0xC5,0x9A,0xD6,0x44,0x45,0xE3,0x38,0x60,0x9A,0x32,0x05,0xF4,0x18,0x01,0x09,0xD8,0xA9,0xC2,0x00,0x5E,0xCA,0x24,0xD5,0x5B,0x9D,0x4A,0x95,0xEA,0x34,0xEE,0x63,0x92,0x5C,0x4D,0xD0,0xA4,0xEE,0x58,0x0C,0xB9,0x4D,0xCD,0x42,0xA2,0x3A,0x24,0x37,0x25,0x8A,0xA8,0x8E,0xA0,0x53,0xE4,0x28,0x23,0x26,0x13,0x72,0x91,0xA2,0x76,0xBB,0x72,0x38,0x45,0x0A,0x46,0x63,0xCA,0x69,0x27,0x39,0x58,0xB1,0x8D,0x60,0x1C,0x34,0x1B,0x34,0xC3,0x55,0x8E,0x73,0x45,0x2D,0x4F,0x4A,0x3A,0x26,0x10,0xA1,0xCA,0x2D,0xE9,0x98,0x24,0x0A,0x1E,0x6D,0x97,0x29,0xD2,0xCC,0x71,0xA2,0xDC,0x86,0xC8,0x12,0xA7,0x8E,0x08,0x85,0x22,0x8D,0x9C,0x43,0xA7,0x12,0xB2,0x2E,0x50,0x09,0xEF,0x51,0xC5,0xBA,0x28,0x58,0xAD,0xDB,0xE1,0xFF,0x03};
uint8_t spEVENING[]   PROGMEM = {0xCD,0x6D,0x98,0x73,0x47,0x65,0x0D,0x6D,0x10,0xB2,0x5D,0x93,0x35,0x94,0xC1,0xD0,0x76,0x4D,0x66,0x93,0xA7,0x04,0xBD,0x71,0xD9,0x45,0xAE,0x92,0xD5,0xAC,0x53,0x07,0x6D,0xA5,0x76,0x63,0x51,0x92,0xD4,0xA1,0x83,0xD4,0xCB,0xB2,0x51,0x88,0xCD,0xF5,0x50,0x45,0xCE,0xA2,0x2E,0x27,0x28,0x54,0x15,0x37,0x0A,0xCF,0x75,0x61,0x5D,0xA2,0xC4,0xB5,0xC7,0x44,0x55,0x8A,0x0B,0xA3,0x6E,0x17,0x95,0x21,0xA9,0x0C,0x37,0xCD,0x15,0xBA,0xD4,0x2B,0x6F,0xB3,0x54,0xE4,0xD2,0xC8,0x64,0xBC,0x4C,0x91,0x49,0x12,0xE7,0xB2,0xB1,0xD0,0x22,0x0D,0x9C,0xDD,0xAB,0x62,0xA9,0x38,0x53,0x11,0xA9,0x74,0x2C,0xD2,0xCA,0x59,0x34,0xA3,0xE5,0xFF,0x03};
uint8_t spPAUSE1[]    PROGMEM = {0x00,0x00,0x00,0x00,0xFF,0x0F};

void sayTime(int hour, int minutes, AudioGeneratorTalkie *talkie) ;

void sayTime(int hour, int minutes, AudioGeneratorTalkie *talkie) {

  if (!out) return;

  AUDIO_PWR_ON
  talkie = new AudioGeneratorTalkie();
  talkie->begin(nullptr, out);

  bool pm = (hour >= 12);
  uint8_t *spHour[] = { spTWELVE, spONE, spTWO, spTHREE, spFOUR, spFIVE, spSIX,
                        spSEVEN, spEIGHT, spNINE, spTEN, spELEVEN };
  size_t   spHourLen[] = { sizeof(spTWELVE), sizeof(spONE), sizeof(spTWO),
                           sizeof(spTHREE), sizeof(spFOUR), sizeof(spFIVE),
                           sizeof(spSIX), sizeof(spSEVEN), sizeof(spEIGHT),
                           sizeof(spNINE), sizeof(spTEN), sizeof(spELEVEN) };
  uint8_t *spMinDec[] = { spOH, spTEN, spTWENTY, spTHIRTY, spFOURTY, spFIFTY };
  size_t spMinDecLen[] = { sizeof(spOH), sizeof(spTEN), sizeof(spTWENTY),
                           sizeof(spTHIRTY), sizeof(spFOURTY), sizeof(spFIFTY) };
  uint8_t *spMinSpecial[] = { spELEVEN, spTWELVE, spTHIRTEEN, spFOURTEEN,
                              spFIFTEEN, spSIXTEEN, spSEVENTEEN, spEIGHTEEN,
                              spNINETEEN };
  size_t spMinSpecialLen[] = { sizeof(spELEVEN), sizeof(spTWELVE),
                               sizeof(spTHIRTEEN), sizeof(spFOURTEEN),
                               sizeof(spFIFTEEN), sizeof(spSIXTEEN),
                               sizeof(spSEVENTEEN), sizeof(spEIGHTEEN),
                               sizeof(spNINETEEN) };
  uint8_t *spMinLow[] = { spONE, spTWO, spTHREE, spFOUR, spFIVE, spSIX,
                          spSEVEN, spEIGHT, spNINE };
  size_t spMinLowLen[] = { sizeof(spONE), sizeof(spTWO), sizeof(spTHREE),
                           sizeof(spFOUR), sizeof(spFIVE), sizeof(spSIX),
                           sizeof(spSEVEN), sizeof(spEIGHT), sizeof(spNINE) };

  talkie->say(spTHE, sizeof(spTHE));
  talkie->say(spTIME, sizeof(spTIME));
  talkie->say(spIS, sizeof(spIS));

  hour = hour % 12;
  talkie->say(spHour[hour], spHourLen[hour]);
  if (minutes==0) {
    talkie->say(spOCLOCK, sizeof(spOCLOCK));
  } else if (minutes<=10 || minutes >=20) {
    talkie->say(spMinDec[minutes / 10], spMinDecLen[minutes /10]);
    if (minutes % 10) {
      talkie->say(spMinLow[(minutes % 10) - 1], spMinLowLen[(minutes % 10) - 1]);
    }
  } else {
    talkie->say(spMinSpecial[minutes - 11], spMinSpecialLen[minutes - 11]);
  }
  if (pm) {
    talkie->say(spP_M_, sizeof(spP_M_));
  } else {
    talkie->say(spA_M_, sizeof(spA_M_));
  }
  delete talkie;
  out->stop();
  AUDIO_PWR_OFF
}
#endif // SAY_TIME

// should be in settings
uint8_t is2_volume;

void I2S_Init(void) {

#if EXTERNAL_DAC_PLAY
    out = new AudioOutputI2S();
#ifdef ESP32
    out->SetPinout(DAC_IIS_BCK, DAC_IIS_WS, DAC_IIS_DOUT);
#endif  // ESP32
#else
    out = new AudioOutputI2S(0, 1);
#endif // EXTERNAL_DAC_PLAY

  is2_volume=10;
  out->SetGain(((float)is2_volume/100.0)*4.0);
  out->stop();
  mp3ram = nullptr;

#ifdef ESP32
  if (psramFound()) {
    mp3ram = heap_caps_malloc(preallocateCodecSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

#ifdef USE_WEBRADIO
  if (psramFound()) {
    preallocateBuffer = heap_caps_malloc(preallocateBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    preallocateCodec = heap_caps_malloc(preallocateCodecSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  } else {
    preallocateBuffer = malloc(preallocateBufferSize);
    preallocateCodec = malloc(preallocateCodecSize);
  }
  if (!preallocateBuffer || !preallocateCodec) {
    //Serial.printf_P(PSTR("FATAL ERROR:  Unable to preallocate %d bytes for app\n"), preallocateBufferSize+preallocateCodecSize);
  }
#endif // USE_WEBRADIO
#endif // ESP32
}



#ifdef ESP32
#define MODE_MIC 0
#define MODE_SPK 1
#define Speak_I2S_NUMBER I2S_NUM_0
//#define MICSRATE 44100
#define MICSRATE 16000

#include <driver/i2s.h>

uint32_t SpeakerMic(uint8_t spkr) {
  esp_err_t err = ESP_OK;

  if (out) {
    out->stop();
    delete out;
    out = nullptr;
  }

  i2s_driver_uninstall(Speak_I2S_NUMBER);
  if (spkr==MODE_SPK) {
    out = new AudioOutputI2S();
    out->SetPinout(DAC_IIS_BCK, DAC_IIS_WS, DAC_IIS_DOUT);
    out->SetGain(((float)is2_volume/100.0)*4.0);
    out->stop();
  } else {
    // config mic
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = MICSRATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        //.dma_buf_len = 128,
        .dma_buf_len = 1024,
    };
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    err += i2s_driver_install(Speak_I2S_NUMBER, &i2s_config, 0, NULL);

    i2s_pin_config_t tx_pin_config;
    tx_pin_config.bck_io_num = DAC_IIS_BCK;
    tx_pin_config.ws_io_num = DAC_IIS_WS;
    tx_pin_config.data_out_num = DAC_IIS_DOUT;
    tx_pin_config.data_in_num = DAC_IIS_DIN;
    err += i2s_set_pin(Speak_I2S_NUMBER, &tx_pin_config);

    err += i2s_set_clk(Speak_I2S_NUMBER, MICSRATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  }
  return err;
}

#define DATA_SIZE 1024

TaskHandle_t mic_task_h;
uint32_t mic_size;
uint8_t *mic_buff;
char mic_path[32];

void mic_task(void *arg){
  uint32_t data_offset = 0;
  while (1) {
      uint32_t bytes_read;
      i2s_read(Speak_I2S_NUMBER, (char *)(mic_buff + data_offset), DATA_SIZE, &bytes_read, (100 / portTICK_RATE_MS));
      if (bytes_read != DATA_SIZE) break;
      data_offset += DATA_SIZE;
      if (data_offset >= mic_size-DATA_SIZE) break;
  }
  SpeakerMic(MODE_SPK);
  SaveWav(mic_path, mic_buff, mic_size);
  free(mic_buff);
  vTaskDelete(mic_task_h);
}

uint32_t i2s_record(char *path, uint32_t secs) {
  esp_err_t err = ESP_OK;

  if (decoder || mp3) return 0;

  err = SpeakerMic(MODE_MIC);
  if (err) {
    SpeakerMic(MODE_SPK);
    return err;
  }

  mic_size = secs * MICSRATE * 2;

  mic_buff = (uint8_t*)heap_caps_malloc(mic_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!mic_buff) return 2;

  if (*path=='+') {
    path++;
    strlcpy(mic_path, path , sizeof(mic_path));
    xTaskCreatePinnedToCore(mic_task, "MIC", 4096, NULL, 3, &mic_task_h, 1);
    return 0;
  }

  uint32_t data_offset = 0;
  uint32_t stime=millis();
  while (1) {
    uint32_t bytes_read;
    i2s_read(Speak_I2S_NUMBER, (char *)(mic_buff + data_offset), DATA_SIZE, &bytes_read, (100 / portTICK_RATE_MS));
    if (bytes_read != DATA_SIZE) break;
    data_offset += DATA_SIZE;
    if (data_offset >= mic_size-DATA_SIZE) break;
    delay(0);
  }
  //AddLog(LOG_LEVEL_INFO, PSTR("rectime: %d ms"), millis()-stime);
  SpeakerMic(MODE_SPK);
  // save to path
  SaveWav(path, mic_buff, mic_size);
  free(mic_buff);
  return 0;
}

static const uint8_t wavHTemplate[] PROGMEM = { // Hardcoded simple WAV header with 0xffffffff lengths all around
    0x52, 0x49, 0x46, 0x46, 0xff, 0xff, 0xff, 0xff, 0x57, 0x41, 0x56, 0x45,
    0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x22, 0x56, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00,
    0x64, 0x61, 0x74, 0x61, 0xff, 0xff, 0xff, 0xff };

bool SaveWav(char *path, uint8_t *buff, uint32_t size) {
  File fwp = ufsp->open(path, "w");
  if (!fwp) return false;
  uint8_t wavHeader[sizeof(wavHTemplate)];
  memcpy_P(wavHeader, wavHTemplate, sizeof(wavHTemplate));

  uint8_t channels = 1;
  uint32_t hertz = MICSRATE;
  uint8_t bps = 16;

  wavHeader[22] = channels & 0xff;
  wavHeader[23] = 0;
  wavHeader[24] = hertz & 0xff;
  wavHeader[25] = (hertz >> 8) & 0xff;
  wavHeader[26] = (hertz >> 16) & 0xff;
  wavHeader[27] = (hertz >> 24) & 0xff;
  int byteRate = hertz * bps * channels / 8;
  wavHeader[28] = byteRate & 0xff;
  wavHeader[29] = (byteRate >> 8) & 0xff;
  wavHeader[30] = (byteRate >> 16) & 0xff;
  wavHeader[31] = (byteRate >> 24) & 0xff;
  wavHeader[32] = channels * bps / 8;
  wavHeader[33] = 0;
  wavHeader[34] = bps;
  wavHeader[35] = 0;

  fwp.write(wavHeader, sizeof(wavHeader));

  fwp.write(buff, size);
  fwp.close();

  return true;
}

#endif // ESP32

#ifdef ESP32
TaskHandle_t mp3_task_h;

void mp3_task(void *arg) {
  while (1) {
    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        mp3_delete();
        if (mp3_task_h) {
          vTaskDelete(mp3_task_h);
          mp3_task_h = 0;
        }
        //mp3_task_h=nullptr;
      }
      delay(1);
    }
  }
}
#endif // ESP32

#ifdef USE_WEBRADIO
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *str) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  (void) ptr;
  if (strstr_P(type, PSTR("Title"))) {
    strncpy(wr_title, str, sizeof(wr_title));
    wr_title[sizeof(wr_title)-1] = 0;
    //AddLog(LOG_LEVEL_INFO,PSTR("WR-Title: %s"),wr_title);
  } else {
    // Who knows what to do?  Not me!
  }
}

void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) code;
  (void) ptr;
  //strncpy_P(status, string, sizeof(status)-1);
  //status[sizeof(status)-1] = 0;
}

void Webradio(const char *url) {
  if (decoder || mp3) return;
  if (!out) return;
  AUDIO_PWR_ON
  ifile = new AudioFileSourceICYStream(url);
  ifile->RegisterMetadataCB(MDCallback, NULL);
  buff = new AudioFileSourceBuffer(ifile, preallocateBuffer, preallocateBufferSize);
  buff->RegisterStatusCB(StatusCallback, NULL);
  decoder = new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
  decoder->RegisterStatusCB(StatusCallback, NULL);
  decoder->begin(buff, out);
  if (!decoder->isRunning()) {
  //  Serial.printf_P(PSTR("Can't connect to URL"));
    StopPlaying();
  //  strcpy_P(status, PSTR("Unable to connect to URL"));
    retryms = millis() + 2000;
  }

  xTaskCreatePinnedToCore(mp3_task2, "MP3-2", 8192, NULL, 3, &mp3_task_h, 1);
}

void mp3_task2(void *arg){
  while (1) {
    if (decoder && decoder->isRunning()) {
      if (!decoder->loop()) {
        StopPlaying();
        //retryms = millis() + 2000;
      }
      delay(1);
    }
  }
}

void StopPlaying() {

  if (mp3_task_h) {
    vTaskDelete(mp3_task_h);
    mp3_task_h = nullptr;
  }

  if (decoder) {
    decoder->stop();
    delete decoder;
    decoder = NULL;
  }
  if (buff) {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (ifile) {
    ifile->close();
    delete ifile;
    ifile = NULL;
  }
  AUDIO_PWR_OFF
}

void Cmd_WebRadio(void) {
  if (decoder) {
    StopPlaying();
  }
  if (XdrvMailbox.data_len > 0) {
    Webradio(XdrvMailbox.data);
    ResponseCmndChar(XdrvMailbox.data);
  } else {
    ResponseCmndChar_P(PSTR("Stopped"));
  }

}

#ifdef USE_M5STACK_CORE2
void Cmd_MicRec(void) {
  if (XdrvMailbox.data_len > 0) {
    uint16 time = 10;
    char *cp = strchr(XdrvMailbox.data, ':');
    if (cp) {
      time = atoi(cp + 1);
      *cp = 0;
    }
    if (time<10) time = 10;
    if (time>30) time = 30;
    i2s_record(XdrvMailbox.data, time);
    ResponseCmndChar(XdrvMailbox.data);
  }
}
#endif // USE_M5STACK_CORE2

const char HTTP_WEBRADIO[] PROGMEM =
   "{s}" "I2S_WR-Title" "{m}%s{e}";

void I2S_WR_Show(void) {
    if (decoder) {
      WSContentSend_PD(HTTP_WEBRADIO,wr_title);
    }
}

#endif

#ifdef ESP32
void Play_mp3(const char *path) {
#if (defined(USE_SCRIPT_FATFS) && defined(USE_SCRIPT)) || defined(USE_UFILESYS)
  if (decoder || mp3) return;
  if (!out) return;

  bool I2S_Task;

  AUDIO_PWR_ON
  if (*path=='+') {
    I2S_Task = true;
    path++;
  } else {
    I2S_Task = false;
  }

  file = new AudioFileSourceFS(*ufsp, path);

  id3 = new AudioFileSourceID3(file);

  if (mp3ram) {
    mp3 = new AudioGeneratorMP3(mp3ram, preallocateCodecSize);
  } else {
    mp3 = new AudioGeneratorMP3();
  }
  mp3->begin(id3, out);

  if (I2S_Task) {
    xTaskCreatePinnedToCore(mp3_task, "MP3", 8192, NULL, 3, &mp3_task_h, 1);
  } else {
    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        break;
      }
      OsWatchLoop();
    }
    mp3_delete();
  }

#endif // USE_SCRIPT
}

void mp3_delete(void) {
  delete file;
  delete id3;
  delete mp3;
  mp3=nullptr;
  AUDIO_PWR_OFF
}
#endif // ESP32

void Say(char *text) {

  if (!out) return;

  AUDIO_PWR_ON

  out->begin();
  ESP8266SAM *sam = new ESP8266SAM;
  sam->Say(out, text);
  delete sam;
  out->stop();

  AUDIO_PWR_OFF
}


const char kI2SAudio_Commands[] PROGMEM = "I2S|"
  "Say|Gain|Time"
#ifdef ESP32
  "|Play"
#ifdef USE_WEBRADIO
  "|WR"
#endif // USE_WEBRADIO
#ifdef USE_M5STACK_CORE2
  "|REC"
#endif // USE_M5STACK_CORE2
#endif // ESP32
  ;

void (* const I2SAudio_Command[])(void) PROGMEM = {
  &Cmd_Say, &Cmd_Gain, &Cmd_Time
#ifdef ESP32
  ,&Cmd_Play
#ifdef USE_WEBRADIO
  ,&Cmd_WebRadio
#endif // USE_WEBRADIO
#ifdef USE_M5STACK_CORE2
  ,&Cmd_MicRec
#endif // USE_M5STACK_CORE2
#endif // ESP32
};



void Cmd_Play(void) {
  if (XdrvMailbox.data_len > 0) {
    Play_mp3(XdrvMailbox.data);
  }
  ResponseCmndChar(XdrvMailbox.data);
}

void Cmd_Gain(void) {
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 100)) {
    if (out) {
      is2_volume=XdrvMailbox.payload;
      out->SetGain(((float)(is2_volume-2)/100.0)*4.0);
    }
  }
  ResponseCmndNumber(is2_volume);
}

void Cmd_Say(void) {
  if (XdrvMailbox.data_len > 0) {
    Say(XdrvMailbox.data);
  }
  ResponseCmndChar(XdrvMailbox.data);
}

void Cmd_Time(void) {
#ifdef SAY_TIME
  sayTime(RtcTime.hour, RtcTime.minute, talkie);
#endif // SAY_TIME
  ResponseCmndDone();
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv42(uint8_t function) {
  bool result = false;

  switch (function) {
    case FUNC_COMMAND:
      result = DecodeCommand(kI2SAudio_Commands, I2SAudio_Command);
      break;
    case FUNC_INIT:
      I2S_Init();
      break;
#ifdef USE_WEBSERVER
#ifdef USE_WEBRADIO
    case FUNC_WEB_SENSOR:
      I2S_WR_Show();
      break;
#endif // USE_WEBRADIO
#endif // USE_WEBSERVER
  }
  return result;
}

#endif  // USE_I2S_AUDIO
