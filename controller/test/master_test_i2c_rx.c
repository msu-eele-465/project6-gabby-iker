#include <msp430.h>

int data_in = 0;

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog timer
    
    UCB1CTLW0 |= UCSWRST;                   // Put eUSCI_B0 into software reset

    UCB1CTLW0 |= UCSSEL_3;                  // Choose BRCLK=SMCLK=1MHz
    UCB1BRW = 10;                           // Divide BRCLK by 10 for SCL=100kHz

    UCB1CTLW0 |= UCMODE_3;                  // Put into I2C mode
    UCB1CTLW0 |= UCMST;                     // Put into master mode
    //UCB1CTLW0 |= UCTR;                      // Put into Tx mode
    UCB1I2CSA = 0x0058;                     // Slave address = 0x68

    UCB1CTLW1 |= UCASTP_2;                  // Auto STOP when UCB0TBCNT reached
    UCB1TBCNT = 0X01;                       // # of Bytes in Packet

    P4SEL1 &= ~BIT6;                        // We want P1.2 = SDA
    P4SEL0 |= BIT6;
    P4SEL1 &= ~BIT7;                        // We want P1.3 = SCL
    P4SEL0 |= BIT7;

    P6OUT &= ~BIT6;                         // Clear P1.0 output latch for a defined power-on state
    P6DIR |= BIT6;                          // Set P1.0 to output direction

    PM5CTL0 &= ~LOCKLPM5;                   // Disable the GPIO power-on default high-impedance mode
                                            // to activate previously configured port settings
    UCB1CTLW0 &= ~UCSWRST;                  // Take eUSCI_B0 out of SW reset
    UCB1IE |= UCTXIE0;                      // Enable I2C Tx0 IRQ
    UCB0IE |= UCRXIE0;
    __enable_interrupt();                   // Enable Maskable IRQs

    int i;
    
    while(1)
    {
        //Transmit Register Address with Write Message
        UCB1CTLW0 |= UCTR;                  // Put into Tx mode
        UCB1CTLW0 |= UCTXSTT;               // Generate START condition
        
        while ((UCB0IFG  & UCSTPIFG) == 0); // Wait for STOP
            UCB1IFG &= ~UCSTPIFG;           // Clear STOP flag
        
        // Recieve Data from Rx
        UCB1CTLW0 &= ~UCTR;             // Put into Rx mode
        UCB1CTLW0 |= UCTXSTT;               // Generate START condition
        
        while ((UCB1IFG  & UCSTPIFG) == 0); // Wait for STOP
            UCB0IFG &= ~UCSTPIFG;           // Clear STOP flag
    }
    return 0;
}

//----------------------------------------------------------------------
// Begin Interrupt Service Routine
//----------------------------------------------------------------------

#pragma vector=EUSCI_B1_VECTOR
__interrupt void EUSCI_B1_I2C_ISR(void){
    switch (UCB1IV)
    {
        case 0x16:
            data_in = UCB0RXBUF;
            break;
        case 0x18:
            UCB0TXBUF = 0x03;
            break;
        default:
            break;
    }
}