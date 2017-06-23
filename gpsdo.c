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


/* |                                                                   |
   |  TinyBeacon project                                               |
   |                                                                   |
   |  - VHF/UHF Beacon (design available for 50, 144, 220 & 440 MHz)   |
   |  - Compact design / Credit card size                              |
   |  - QRP, 5W output power                                           |
   |  - 10 MHz oscillator stabilized by GPS (GPSDO)                    |
   |  - DC-DC Power supply within 10-15V, 1.5A max                     |
   |  - Compatible with WSPR & PI4 protocols                           |
   |                                                                   |
   |                                                                   |
   |  IO Mapping uController, rev.C                                    |
   |                                                                   |
   |  - PC4/SDA  (pin 27) | I2C SDA                                    |
   |  - PC5/SCL  (pin 28) | I2C SCL                                    |
   |  - PD0/RXD  (pin 30) | USART RX                                   |
   |  - PD1/TXD  (pin 31) | USART TX                                   |
   |  - PD5      (pin  9) | GPS INT                                    |
   |  - PD6      (pin 10) | PA EN                                      |
   |  - PD7      (pin 11) | INFO LED                                   |
   |  - PB0      (pin 12) | PLL LOCK                                   |
   |  - PB2      (pin 14) | PLL EN                                     |
   |                                                                   | */

#include "config.h"

#include "twi.h"
#include "gps.h"
#include "pll.h"
#include "usart.h"

#include "morse.h"
#include "pi4.h"
#include "wspr.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


void timeAlignPI4() {
    /* Get GPS data for the next time sync */
    gpsGetTime();

    /* Align on an minute for the next message */
    gpsTimeAling1Mb();
}


void timeAlignWSPR() {
    /* Get GPS data for the next time sync */
    gpsGetTime();

    /* Align on odd minute for the next message */
    gpsTimeAling2Mb();
}


void pi4sequence() {
    /* 1st part : Send PI4 message, 25 sec */
    pi4Send();

    /* 2nd part : Send morse message */
    morse2TonesSendMessage();

    /* 3th part : Send a carrier, 10 sec, same frequency */
    pllUpdate(1);
    pllPA(1);
    pllRfOutput(1);
    _delay_ms(10000);
    pllRfOutput(0);
    pllPA(0);
}


int main (void) {
    /* CKDIV8 fuse is set -- Frequency is divided by 8 at startup : 2.5MHz */
    cli();
    CLKPR = _BV(CLKPCE);  // Enable change of CLKPS bits
    CLKPR = 0;            // Set prescaler to 0 = Restore system clock to 10 MHz
    sei();

    /* LED : Set pin 11 of PORT-PD7 for output*/
    DDRD |= _BV(DDD7);

    /* For now, used for DEBUG purpose only. Future : CLI for freq settings & modes */
    usartInit();
    _delay_ms(10);

    /* Peform I2C modules init */
    twi_init();
    _delay_ms(10);

    /* Prepare the message to encode for PI4 message */
    pi4Encode();

    /* Prepare the message to encode for WSPR message */
    wsprEncode();

    /* uBlox : GPS IO init & Set the default I2C address of the GPS */
    gpsInit(0x42);  // I2C Have to be init before the PLL !

    /* uBlox : Rstrict DDC port only */
    gpsSet_CFG_PRT();

    /* uBlox : Wait on a full GPS sync (+ info req. for message encoding)*/
    gpsGetPVT();
    gpsExtractStrings();
    gpsGetTime();

    /* uBlox : 10MHz timing setup */
    gpsSet_CFG_TP5();

    /* uBlox : Refresh rate for internal GPSDO alignment */
    gpsSet_CFG_RATE();

    /* ADF4355 PLL Init, conf & settings */
    pllInit(0x60);  // 0x60 used only for Si5351 (I2C addr.)

    /* End of init sequence : Turn on the LED (pin 11) */
    PORTD |= _BV(PORTD7);

    /* Start with a unsync TX, boring to wait a full sync... */
    pi4Send();
    wsprSend();

    /* Loop sequence :
       - PI4 + Morse + Tone (1 minute)
       - PI4 + Morse + Tone (1 minute)
       - WSPR (2 minutes)
    */
    while(1) {
        timeAlignPI4();
        pi4sequence();

        timeAlignPI4();
        pi4sequence();

        timeAlignWSPR();
        wsprSend();
    }

    /* This case never happens :) Useless without powermanagement... */
    return 0;
}

/* === Si5351 DEBUG 60sec (2500 x 2 x 12ms)
pllSetFreq(144430000000000,0);
pllSetFreq(144435000000000,1);
pllUpdate(0);
pllRfOutput(1);
pllPA(1);

for (uint32_t i=0; i<2500; i++) { //while(1) {
  pllUpdate(0);
  //pllRfOutput(0);
  //_delay_ms(1000);

  pllUpdate(1);
  //pllRfOutput(1);
  //_delay_ms(1000);
}

pllRfOutput(0);
pllPA(0);
*/

/* === ADF4355 DEBUG 60sec
pllSetFreq(222295000000000,0); 
pllUpdate(0);

pllRfOutput(1);
pllPA(1);
_delay_ms(667);
pllRfOutput(0);
pllPA(0);

for (uint32_t i=0; i<60000; i++) { //while(1) {
  pllUpdate(0);
}

pllRfOutput(1);
pllPA(1);
_delay_ms(667);
pllRfOutput(0);
pllPA(0);
*/

/* TEST 28.8MHz clock 
pllSetFreq(28800000000000,0);
pllUpdate(0);
pllRfOutput(1);
pllPA(1);
while(1) { }
*/