/*
 * FreeBSD License
 * Copyright (c) 2015, Guenael
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/* Code for callsign packing from : https://github.com/DanAnkers/WsprryPi/blob/master/wspr.c */


#include "config.h"
#include "wspr.h"

#include "pll.h"

#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include <avr/pgmspace.h>


#define WSPR_SYMBOLS_LENGTH       162       // The number of symbols in the WSPR transmission
#define WSPR_SYMBOLS              162       // The number of symbols in the WSPR transmission
#define WSPR_BAUD_RATE            (375.0 / 256.0)                 // 1.46484375 bauds
#define WSPR_SYMBOL_DURATION      (1000.0 / WSPR_BAUD_RATE)       // 256 / 375 = 682.67ms


/* WSPR output symbols */
static uint8_t Symbols[WSPR_SYMBOLS];

/* Cross Vector */
static const uint8_t PROGMEM wsprVector[] = {
    1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,0,0,
    0,0,1,0,0,1,0,1,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,0,1,0,
    0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,
    0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,1,
    0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
    0,0
};


uint32_t wsprParity(uint64_t Value) {
    uint32_t Even=0;

    for (uint8_t BitNo=0; BitNo<=31; BitNo++)
        if ((Value >> BitNo) & 0x01)
            Even = 1-Even;

    return Even;
}


void wsprEncode() {
    char     callsign[]=WSPR_CALLSIGN;
    char     locator[]=WSPR_LOCATOR;
    uint64_t power=WSPR_POWER;

    uint64_t n=0, m=0;
    uint8_t  packed[11]= {0};

    /* Callsign encoding */
    n =          (callsign[0] >= '0' && callsign[0] <= '9' ? callsign[0] - '0' : callsign[0] == ' ' ? 36 : callsign[0] - 'A' + 10);
    n = n * 36 + (callsign[1] >= '0' && callsign[1] <= '9' ? callsign[1] - '0' : callsign[1] == ' ' ? 36 : callsign[1] - 'A' + 10);
    n = n * 10 + (callsign[2] - '0'); // only number (0-9)
    n = n * 27 + (callsign[3] == ' ' ? 26 : callsign[3] - 'A');   // only space or letter
    n = n * 27 + (callsign[4] == ' ' ? 26 : callsign[4] - 'A');
    n = n * 27 + (callsign[5] == ' ' ? 26 : callsign[5] - 'A');

    /* Locator encoding */
    m = (179 - 10 * (locator[0] - 65) - (locator[2] - 48)) * 180 + 10 * (locator[1] - 65) + locator[3] - 48;

    /* Power encoding */
    m = m * 128 + power + 64;

    /* Message packing */
    packed[0] = n >> 20;
    packed[1] = n >> 12;
    packed[2] = n >> 4;
    packed[3] = ((n & 0x0F) << 4) | ((m >> 18) & 0x0F);
    packed[4] = m >> 10;
    packed[5] = m >> 2;
    packed[6] = (m & 0x03) <<6;

    /* Convolutional encoding */
    uint8_t ConvEnc[WSPR_SYMBOLS]; // FIX
    uint32_t N=0;
    uint8_t t=0;

    for (int8_t j=0; j<11; j++) {
        for (uint8_t i=0; i<8; i++) {
            N <<= 1;
            if ( packed[j] & 1<<(7-i) )
                N |= 1;

            ConvEnc[t++] = wsprParity(N & 0xF2D05351);  // Poly1
            ConvEnc[t++] = wsprParity(N & 0xE4613C47);  // Poly2
        }
    }

    /* Interleaving */
    uint32_t Interleaved[WSPR_SYMBOLS]; // FIX mem fill 0
    uint8_t P=0;
    uint8_t R=0;
    memset (Interleaved, 0x00, WSPR_SYMBOLS);
    for (uint8_t i=0; i<255; i++) {
        for (uint8_t BitNo=0; BitNo<=7; BitNo++) {
            if ((i >> BitNo) & 1)
                R |= 1 << (7-BitNo);
            else
                R &= ~(1 << (7-BitNo));
        }

        if ((P<WSPR_SYMBOLS) && (R<WSPR_SYMBOLS))
            Interleaved[R] = ConvEnc[P++];
    }

    /*  Merge With Sync Vector */
    for (uint8_t i=0; i<WSPR_SYMBOLS; i++)
        Symbols[i] = pgm_read_byte(&wsprVector[i]) + (2*Interleaved[i]);
    //Symbols[i] = pgm_read_byte(&wsprVector[i]) | (Interleaved[i] << 1);
}


void wsprSetFreqs(float carrierFreq) {
    uint64_t baseFreq = (uint64_t)carrierFreq * 1000000ULL;

    pllSetFreq(baseFreq, 0);
    pllSetFreq(baseFreq + 1465000ULL, 1);
    pllSetFreq(baseFreq + 3515625ULL, 2);
    pllSetFreq(baseFreq + 5859375ULL, 3);
    pllUpdate(0);
    _delay_ms(10);
}


void wsprSend() {
    wsprSetFreqs(WSPR_FREQUENCY);

    // WSPR protocol start after 2 seconds
    _delay_ms(2000);

    pllPA(1);
    pllRfOutput(1);

    // Send WSPR message
    for (int i=0; i<WSPR_SYMBOLS_LENGTH; i++) {
        pllUpdateTiny( Symbols[i] );
        _delay_ms(WSPR_SYMBOL_DURATION - 1.0);  // FIXME : Timing adjustment ! (-12 Si, -1 ADI)
    }

    pllRfOutput(0);
    pllPA(0);
}
