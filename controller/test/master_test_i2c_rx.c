#include <msp430fr2355.h>
#include <stdint.h>

#define LM92_ADDR 0x4B

float temp_C = 0;

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    // SDA = P4.6, SCL = P4.7
    P4SEL0 |= BIT6 | BIT7;
    P4SEL1 &= ~(BIT6 | BIT7);

    // Configuración inicial del I2C
    UCB1CTLW0 = UCSWRST;
    UCB1CTLW0 |= UCMST | UCMODE_3 | UCSSEL_2;
    UCB1BRW = 10;
    UCB1I2CSA = LM92_ADDR;
    UCB1CTLW0 &= ~UCSWRST;

    int16_t raw_temp;
    uint8_t msb = 0, lsb = 0;

    while (1)
    {
        // 🔄 Reiniciar I2C antes de cada ciclo (soluciona el desfase)
        UCB1CTLW0 |= UCSWRST;
        UCB1CTLW0 &= ~UCSWRST;

        // --- Write: enviar 0x00 para seleccionar registro temperatura ---
        while (UCB1CTLW0 & UCTXSTP);
        UCB1CTLW0 |= UCTR | UCTXSTT;
        while (!(UCB1IFG & UCTXIFG0));
        UCB1TXBUF = 0x00;
        while (!(UCB1IFG & UCTXIFG0));
        UCB1CTLW0 |= UCTXSTP;
        while (UCB1CTLW0 & UCTXSTP);

        __delay_cycles(500);  // Pequeño delay

        // --- Read: solicitar 2 bytes ---
        while (UCB1CTLW0 & UCTXSTP);
        UCB1CTLW0 &= ~UCTR;
        UCB1CTLW0 |= UCTXSTT;
        while (UCB1CTLW0 & UCTXSTT);

        while (!(UCB1IFG & UCRXIFG0));
        msb = UCB1RXBUF;

        while (!(UCB1IFG & UCRXIFG0));
        UCB1CTLW0 |= UCTXSTP;
        while (!(UCB1IFG & UCRXIFG0));
        lsb = UCB1RXBUF;

        while (UCB1CTLW0 & UCTXSTP);

        // --- Conversión a °C
        raw_temp = ((int16_t)msb << 8) | lsb;
        raw_temp >>= 3;
        temp_C = raw_temp * 0.0625f;

        __no_operation();  // 🔴 breakpoint para revisar temp_C

        __delay_cycles(1000000);
    }
}
