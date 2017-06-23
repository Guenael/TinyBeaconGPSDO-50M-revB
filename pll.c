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


#include "config.h"
#include "pll.h"

#include "twi.h"

#include <avr/io.h>
#include <util/delay.h>


#define ADI


/* === ADF4355 CODE === */
#ifdef ADI

    #define COUNTER_RESET         4  //  5th bit, Register 4
    #define AUTOCAL              21  // 22th bit, Register 0
    #define RF_OUTPUT_ENABLE      6  //  7th bit, Register 6
    #define PLL_UPDATE_DELAY      2

    /* Precalculated settings for the PLL */
    static uint32_t pllGeneralSettings[13] = {
        0x00201CC0,  // Register 0
        0x0CCCCCC1,  // Register 1
        0x00000012,  // Register 2
        0x40000003,  // Register 3
        0x3000C184,  // Register 4
        0x00800025,  // Register 5
        0x35002CF6,  // Register 6
        0x12000007,  // Register 7
        0x102D0428,  // Register 8
        0x14053CF9,  // Register 9
        0x60C017FA,  // Register 10
        0x0061300B,  // Register 11
        0x0000041C   // Register 12
    };


    /* Precalculated settings for the PLL, using 4 Banks */
    static uint32_t pllCustomSettings[6][2];  // 6 Tones MAX


    void pllTransmitByte(uint8_t data) {
        /* Enable PLL LE */
        PORTB &= ~_BV(PORTB2);

        /* Start transmission */
        SPDR = data;

        /* Wait for transmission complete */
        while(!(SPSR & _BV(SPIF)));

        /* Disable PLL LE */
        PORTB |= _BV(PORTB2);
    }


    void pllTransmitWord(uint32_t data) {
        /* Enable PLL LE */
        PORTB &= ~_BV(PORTB2);

        uint8_t *p = (uint8_t*)&data;
        for (uint8_t i=0; i<4; i++) {
            /* Start transmission */
            SPDR = p[3-i];  // Little endian

            /* Wait for transmission complete */
            while(!(SPSR & _BV(SPIF)));
        }

        /* Disable PLL LE */
        PORTB |= _BV(PORTB2);
    }


    void pllInit(uint8_t addr) {
        DDRB   |= _BV(DDB3);       /* MOSI   - Enable output */
        DDRB   |= _BV(DDB5);       /* SCK    - Enable output */

        //DDRB   &= ~_BV(DDB0);      /* PLL_INTR - Set input */
        //PORTB  |= ~_BV(PORTB0);    /* Activate pull-ups in PORTB pin 12 */

        DDRB   |=  _BV(DDB2);      /* PLL_LE - Enable output */
        PORTB  |= _BV(PORTB2);     /* PLL_LE disable */

        DDRD   |=  _BV(DDD6);      /* PA_EN - Set output */
        PORTD  &= ~_BV(PORTD6);    /* PA_EN disable at start */

        /* Enable SPI, as Master, prescaler = Fosc/16 */
        SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);

        /* General settigs based on Morse freq */
        pllSetFreq((MORSE_FREQUENCY * 1000000), 0);

        /* First initialisation of the PLL, with the default 0 */
        _delay_ms(100);
        pllTransmitWord(pllGeneralSettings[12]);
        pllTransmitWord(pllGeneralSettings[11]);
        pllTransmitWord(pllGeneralSettings[10]);
        pllTransmitWord(pllGeneralSettings[9]);
        pllTransmitWord(pllGeneralSettings[8]);
        pllTransmitWord(pllGeneralSettings[7]);
        pllTransmitWord(pllGeneralSettings[6]);
        pllTransmitWord(pllGeneralSettings[5]);
        pllTransmitWord(pllGeneralSettings[4]);
        pllTransmitWord(pllGeneralSettings[3]);
        pllTransmitWord(pllGeneralSettings[2]);
        pllTransmitWord(pllGeneralSettings[1]);
        pllTransmitWord(pllGeneralSettings[0]);
        _delay_ms(100);
    }


    void pllShutdown() {
    }


    void pllUpdate(uint8_t bank) {
        /* Regular way to update the PLL : Documentation ADF4355-2, page 29
           http://www.analog.com/media/en/technical-documentation/data-sheets/ADF4355-2.pdf */

        pllGeneralSettings[4] |= (1UL<<COUNTER_RESET);        // Counter reset enabled [DB4 = 1]
        pllTransmitWord(pllGeneralSettings[4]);               // Register 4 (counter reset enabled [DB4 = 1])

        //pllTransmitWord(pllGeneralSettings[bank][2]);         // Register 2

        pllTransmitWord(pllCustomSettings[bank][1]);          // Register 1

        pllCustomSettings[bank][0] &= ~(1UL<<AUTOCAL);        // Autocal enable
        pllTransmitWord(pllCustomSettings[bank][0]);          // Register 0 (autocal disabled [DB21 = 0])

        pllGeneralSettings[4] &= ~(1UL<<COUNTER_RESET);       // Counter reset disable [DB4 = 0]
        pllTransmitWord(pllGeneralSettings[4]);               // Register 4 (counter reset disabled [DB4 = 0])

        _delay_us(500);                                       // Sleep FIXME
        pllCustomSettings[bank][0] |= (1UL<<AUTOCAL);         // Autocal enable
        pllTransmitWord(pllCustomSettings[bank][0]);          // Register 0 (autocal enabled [DB21 = 1])

        _delay_us(216);  // Align on 1ms
    }


    void pllUpdateTiny(uint8_t bank) {
        /* Quick and dirty update if the delta is very low */
        pllTransmitWord(pllCustomSettings[bank][1]);          // Register 1
        pllCustomSettings[bank][0] |= (1UL<<AUTOCAL);         // Autocal enable
        pllTransmitWord(pllCustomSettings[bank][0]);          // Register 0 (autocal enabled [DB21 = 1])
        _delay_us(900);  // Align on 1ms
    }


    void pllSetFreq(uint64_t freq, uint8_t bank) {
        /* Calculate the frequency register -- Application 144MHz (Usable only beetween : 106.25 - 212.5 MHz) */
        /* NOTE : AVR do NOT support double, and precision of float are insuffisant, so I use uint64... */

        /* Output divider */
        uint8_t pllDivider = 1;

        /* Convert & use freq in MHz */
        uint64_t freqMHz = freq / 1000000000000;

        if ( freqMHz <  (6800/128) ) {        //  freq < 53.125
            pllGeneralSettings[6] |= 0x00C00000;
            pllDivider = 128; // Implicit prescaler /2
        } else if ( freqMHz <  (6800/64) ) {  //  freq < 106.25
            pllGeneralSettings[6] |= 0x00C00000;
            pllDivider = 64;
        } else if ( freqMHz <  (6800/32) ) {  //  freq < 212.5
            pllGeneralSettings[6] |= 0x00A00000;
            pllDivider = 32;
        } else if ( freqMHz <  (6800/16) ) {  //  freq < 425
            pllGeneralSettings[6] |= 0x00800000;
            pllDivider = 16;
        } else if ( freqMHz <  (6800/8) ) {  //  freq < 850
            pllGeneralSettings[6] |= 0x00600000;
            pllDivider = 8;
        } else if ( freqMHz <  (6800/4) ) {  //  freq < 1700
            pllGeneralSettings[6] |= 0x00400000;
            pllDivider = 4;
        } else if ( freqMHz <  (6800/2) ) {  //  freq < 3400
            pllGeneralSettings[6] |= 0x00200000;
            pllDivider = 2;
        } else { //if ( freqMHz <  (6800/1) ) {  //  freq < 6800
            pllGeneralSettings[6] |= 0x00000000; // Useless, but consistency...
            pllDivider = 1;
        }

        uint64_t pllVcoFreq  = freq * pllDivider;
        uint64_t pllN        = pllVcoFreq / 10000000;

        uint64_t pllNint1    = pllN / 1000000;
        uint64_t pllNfrac1   = ((pllN - (pllNint1*1000000)) * 16777216)/1000000;

        uint32_t intN        = (uint32_t)pllNint1;
        uint32_t intFrac1    = (uint32_t)pllNfrac1;

        pllCustomSettings[bank][0] = (intN<<4);
        pllCustomSettings[bank][1] = (intFrac1<<4) | 0x00000001;
    }


    void pllRfOutput(uint8_t enable) {
        if (enable)
            pllGeneralSettings[6] |= (1UL<<RF_OUTPUT_ENABLE);  // Bank 0 used by default
        else
            pllGeneralSettings[6] &= ~(1UL<<RF_OUTPUT_ENABLE);

        pllTransmitWord(pllGeneralSettings[6]);
    }


    void pllPA(uint8_t enable) {
        if (enable)
            PORTD |= _BV(PORTD6);
        else
            PORTD &= ~_BV(PORTD6);
    }


    uint32_t pllGetTiming() {
        return(1);
    }

/* === Si5351 CODE === */
#else

    #define SI_CLK_ENABLE    3
    #define SI_PLL_INPUT_SRC 15
    #define SI_CLK_CONTROL   16
    #define SI_SYNTH_PLL_A   26  // Multisynth NA
    #define SI_SYNTH_PLL_B   34  // Multisynth NB
    #define SI_SYNTH_MS_0    42
    #define SI_VCXO_PARAM    162
    #define SI_PLL_RESET     177

    #define XTAL_FREQ        27000000000000
    #define TCXO_FREQ        10000000000000


    /* Global definition for the I2C GPS address */
    static uint8_t pll_si5351c_Addr;

    static uint8_t pll_si5351c_BankSettings[6][8];


    void pllSendRegister(uint8_t reg, uint8_t data) {
        uint8_t tmp[2] = {0};
        tmp[0] = reg;
        tmp[1] = data;

        twi_writeTo(pll_si5351c_Addr, tmp, sizeof(tmp), 1, 0);
        _delay_ms(1);
    }


    void pllInit(uint8_t addr) {
        DDRD   |=  _BV(DDD6);      /* PA_EN - Set output */
        PORTD  &= ~_BV(PORTD6);    /* PA_EN disable at start */

        DDRB   &= ~_BV(DDB0);      /* PLL_INTR - Set input */
        PORTB  |= ~_BV(PORTB0);    /* Activate pull-ups in PORTB pin 12 */

        DDRB   |=  _BV(DDB2);      /* PLL_LE - Enable output */
        PORTB  &= ~_BV(PORTB2);    /* PLL_LE enable at start for config */

        _delay_ms(100);

        /* Define I2C address for the PLL */
        pll_si5351c_Addr = addr;

        pllSendRegister(SI_CLK_ENABLE, 0xFF);      // Disable all output
        pllSendRegister(SI_PLL_INPUT_SRC, 0x0C);   // Use external clock on PLL-A & PLL-B

        pllSendRegister(SI_CLK_CONTROL+0, 0x0F);   // Turn on CLK0 with PLL-A & 8mA (0x0F)
        pllSendRegister(SI_CLK_CONTROL+1, 0x84);   // Turn off
        pllSendRegister(SI_CLK_CONTROL+2, 0x84);   // Turn off
        pllSendRegister(SI_CLK_CONTROL+3, 0x84);   // Turn off
        pllSendRegister(SI_CLK_CONTROL+4, 0x84);   // Turn off
        pllSendRegister(SI_CLK_CONTROL+5, 0x84);   // Turn off
        pllSendRegister(SI_CLK_CONTROL+6, 0x84);   // Turn off
        pllSendRegister(SI_CLK_CONTROL+7, 0x84);   // Turn off

        /* MultiSynth0
           P1 : Integer part
           P2 : Fractional numerator
           P3 : Fractional denominator
        */
        pllSendRegister(SI_SYNTH_MS_0+0, 0x00); // MS0_P3[15:8]
        pllSendRegister(SI_SYNTH_MS_0+1, 0x01); // MS0_P3[7:0]
        pllSendRegister(SI_SYNTH_MS_0+2, 0x00); // R0_DIV[2:0] + MS0_DIVBY4[1:0] + MS0_P1[17:16] 
        pllSendRegister(SI_SYNTH_MS_0+3, 0x01); // MS0_P1[15:8]
        pllSendRegister(SI_SYNTH_MS_0+4, 0x00); // MS0_P1[7:0]
        pllSendRegister(SI_SYNTH_MS_0+5, 0x00); // MS0_P3[19:16] +  MS0_P2[19:16]
        pllSendRegister(SI_SYNTH_MS_0+6, 0x00); // MS0_P2[15:8]
        pllSendRegister(SI_SYNTH_MS_0+7, 0x00); // MS0_P2[7:0]

        pllSendRegister(SI_CLK_ENABLE, 0xFE);      // Disable all output exept CLK0 (CLK0_OEB)
        pllSendRegister(SI_PLL_RESET, 0xA0);
    }


    void pllShutdown() {
    }


    void pllUpdate(uint8_t bank) {
        pllSendRegister(SI_SYNTH_PLL_A + 0, pll_si5351c_BankSettings[bank][0]);
        pllSendRegister(SI_SYNTH_PLL_A + 1, pll_si5351c_BankSettings[bank][1]);
        pllSendRegister(SI_SYNTH_PLL_A + 2, pll_si5351c_BankSettings[bank][2]);
        pllSendRegister(SI_SYNTH_PLL_A + 3, pll_si5351c_BankSettings[bank][3]);
        pllSendRegister(SI_SYNTH_PLL_A + 4, pll_si5351c_BankSettings[bank][4]);
        pllSendRegister(SI_SYNTH_PLL_A + 5, pll_si5351c_BankSettings[bank][5]);
        pllSendRegister(SI_SYNTH_PLL_A + 6, pll_si5351c_BankSettings[bank][6]);
        pllSendRegister(SI_SYNTH_PLL_A + 7, pll_si5351c_BankSettings[bank][7]);
        //pllSendRegister(SI_PLL_RESET, 0xA0);  // Reset both PLL -- make glitch!!
        
        _delay_us(1702);  // 8 commands take : 10.5ms, aling on 12ms
    }


    void pllUpdateTiny(uint8_t bank) {
        pllUpdate(bank);
    }


    void pllMultiSynth(uint32_t divider, uint8_t rDiv) {
        uint32_t P1 = 128 * divider - 512;
        uint32_t P2 = 0; // P2 = 0, P3 = 1 forces an integer value for the divider
        uint32_t P3 = 1;

        pllSendRegister(SI_SYNTH_MS_0 + 0, (P3 & 0x0000FF00) >> 8);
        pllSendRegister(SI_SYNTH_MS_0 + 1, (P3 & 0x000000FF));
        pllSendRegister(SI_SYNTH_MS_0 + 2, ((P1 & 0x00030000) >> 16) | rDiv);
        pllSendRegister(SI_SYNTH_MS_0 + 3, (P1 & 0x0000FF00) >> 8);
        pllSendRegister(SI_SYNTH_MS_0 + 4, (P1 & 0x000000FF));
        pllSendRegister(SI_SYNTH_MS_0 + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
        pllSendRegister(SI_SYNTH_MS_0 + 6, (P2 & 0x0000FF00) >> 8);
        pllSendRegister(SI_SYNTH_MS_0 + 7, (P2 & 0x000000FF));
    }


    void pllSetFreq(uint64_t freq, uint8_t bank) {
        uint64_t divider = 900000000000000 / freq; // 900 / 144 = 6.25 > 6
        if (divider % 2) divider--;

        pllMultiSynth(divider, 0); // FIXME !!! 1 time only

        uint64_t pllFreq = divider * freq; // 6 * 144 = 864
        uint32_t mult = (uint32_t)(pllFreq / TCXO_FREQ); // 864 / 10 = 86.4 > 86 (entre 15 & 90)
        uint32_t num  = (uint32_t)(((pllFreq % TCXO_FREQ) * 1048575) / TCXO_FREQ); //
        uint32_t denom = 1048575;
        // FIXME best denom ajust for WSPR

        uint32_t P1,P2,P3;
        P1 = 128 * mult + ((128 * num) / denom) - 512;
        P2 = 128 * num - denom * ((128 * num) / denom);
        P3 = denom;

        /* Packing */
        pll_si5351c_BankSettings[bank][0] = (P3 & 0x0000FF00) >> 8;
        pll_si5351c_BankSettings[bank][1] = (P3 & 0x000000FF);
        pll_si5351c_BankSettings[bank][2] = (P1 & 0x00030000) >> 16;
        pll_si5351c_BankSettings[bank][3] = (P1 & 0x0000FF00) >> 8;
        pll_si5351c_BankSettings[bank][4] = (P1 & 0x000000FF);
        pll_si5351c_BankSettings[bank][5] = ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16);
        pll_si5351c_BankSettings[bank][6] = (P2 & 0x0000FF00) >> 8;
        pll_si5351c_BankSettings[bank][7] = (P2 & 0x000000FF);
    }

    void pllRfOutput(uint8_t enable) {
        if (enable) {
            PORTB &= ~_BV(PORTB2);
        } else {
            //PORTB |= _BV(PORTB2); // DEBUG always ON 
        }
    }


    void pllPA(uint8_t enable) {
        if (enable)
            PORTD |= _BV(PORTD6);
        else
            PORTD &= ~_BV(PORTD6);
    }


    uint32_t pllGetTiming() {
        return(12);
    }

#endif
