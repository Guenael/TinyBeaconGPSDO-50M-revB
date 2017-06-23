/*
 * FreeBSD License
 * Copyright (c) 2016, Guenael
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


/* Protocol description & source : http://www.rudius.net/oz2m/ngnb/pi4.htm */


#include "config.h"
#include "pi4.h"

#include "pll.h"

#include <string.h>
#include <util/delay.h>
#include <avr/pgmspace.h>


#define PI4_SYMBOL_DURATION      166.667      // ms
#define PI4_SYMBOLS              146          // The number of symbols in the PI4 transmission
#define PI4_MSG_LENGTH           8            // Maximum message length allowed


/* PI4 output symbols */
static uint8_t Symbols[PI4_SYMBOLS];

/* Encoding stuff */
static const uint8_t PI4Chars[]          = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ /";
static const uint8_t PI4Vector[] PROGMEM = {
    0,0,1,0,0,1,1,1,1,0,1,0,1,0,1,0,0,1,0,0,0,1,0,0,0,1,1,0,0,1,
    1,1,1,0,0,1,1,1,1,1,0,0,1,1,0,1,1,1,1,0,1,0,1,1,0,1,1,0,1,0,
    0,0,0,0,1,1,1,1,1,0,1,0,1,0,0,0,0,0,1,1,1,1,1,0,1,0,0,1,0,0,
    1,0,1,0,0,0,0,1,0,0,1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,1,1,1,
    0,1,1,1,0,1,1,0,1,0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,1,1
};


uint32_t Parity(uint64_t Value) {
    uint32_t Even=0;

    for (uint8_t BitNo=0; BitNo<=31; BitNo++)
        if ((Value >> BitNo) & 0x01)
            Even = 1-Even;

    return Even;
}


uint8_t* strSearch(const uint8_t* s, uint32_t c) {
    const uint8_t ch = c;

    for (; *s != ch; s++)
        if (*s == '\0')
            return 0;
    return (uint8_t*)s;
}


void strCopyS(const char* src, char* dest, uint8_t length) {
    while(length--)
        *dest++ = *src++;
}


void PI4MakeMessage(char *msg) {
    /* Source encoding */
    uint64_t SourceEnc = 0;
    for (uint8_t i=0; i<PI4_MSG_LENGTH; i++)
        SourceEnc = SourceEnc*38 + (uint64_t)(strSearch(PI4Chars, msg[i])-PI4Chars);  // FIXME : soustraction pourrait etre fait dans la fct.

    /* Convolutional encoding */
    uint32_t n=0;
    uint8_t t=0;
    uint8_t ConvEnc[PI4_SYMBOLS] = {0}; // FIX
    memset (ConvEnc, 0x00, PI4_SYMBOLS);
    for (uint8_t j=0; j<PI4_SYMBOLS/2; j++) {
        n <<= 1;
        if (SourceEnc & 0x20000000000LLU)
            n |= 1;
        SourceEnc <<= 1;

        ConvEnc[t++] = Parity(n & 0xF2D05351);  // Poly1
        ConvEnc[t++] = Parity(n & 0xE4613C47);  // Poly2
    }

    /* Interleaving */
    uint8_t P=0;
    uint8_t R=0;
    uint32_t Interleaved[PI4_SYMBOLS] = {0};                           // FIXME mem fill 0
    memset (Interleaved, 0x00, PI4_SYMBOLS);

    for (uint16_t i=0; i<=255; i++) {                                  // FIXME/CHECK
        for (uint8_t BitNo=0; BitNo<=7; BitNo++) {
            if ((i >> BitNo) & 1)
                R |= 1 << (7-BitNo);
            else
                R &= ~(1 << (7-BitNo));
        }

        if ((P<PI4_SYMBOLS) && (R<PI4_SYMBOLS))
            Interleaved[R] = ConvEnc[P++];
    }

    /*  Merge With Sync Vector */
    for (uint8_t i=0; i<PI4_SYMBOLS; i++)
        //Symbols[i] = PI4Vector[i] + (2*Interleaved[i]);
        Symbols[i] = pgm_read_byte(&PI4Vector[i]) | (Interleaved[i] << 1);
}


void pi4SetFreqs(float carrierFreq) {
    uint64_t baseFreq = (uint64_t)carrierFreq * 1000000ULL;

    pllSetFreq(baseFreq - 117187500ULL, 0);
    pllSetFreq(baseFreq + 117187500ULL, 1);
    pllSetFreq(baseFreq + 351562500ULL, 2);
    pllSetFreq(baseFreq + 585937500ULL, 3);
    pllSetFreq(baseFreq, 4);
    pllUpdate(4);
}


void pi4Encode() { // FIXME useless function...
    /* PI4 Message encoding - Part 1 */
    PI4MakeMessage(PI4_MESSAGE);
}


void pi4Send() {
    pi4SetFreqs(PI4_FREQUENCY);

    pllPA(1);
    pllRfOutput(1);

    // Send PI4 message
    for (int i=0; i<PI4_SYMBOLS; i++) {
        pllUpdate( Symbols[i] );
        _delay_ms(PI4_SYMBOL_DURATION - 1.0);  // FIXME : Timing adjustment ! (-12 Si, -1 ADI)
    }

    pllRfOutput(0);
    pllPA(0);

    _delay_ms(667); // Align on 25 sec
}
