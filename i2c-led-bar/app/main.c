#include "intrinsics.h"
#include <msp430.h>
#include <stdbool.h>

//------------------------------------------------------------------------------
// Definitions
//------------------------------------------------------------------------------
#define ledOn 0xFF
#define ledOff 0
#define SLAVE_ADDR  0x68                    // Slave I2C Address
#define timing_base 32768;                 // 1 second

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------
bool new_input_bool = true;                 // Initialize new input to true
char key_cur;                               // Store key press
char key_prev = '\0';                       // Initialize keypress to null
int pattern_a_cur;
int pattern_b_cur;
bool pattern_a_start = false;
bool pattern_b_start = false;
int timing_adj;                             // + or - 0.5 seconds
unsigned char ledPattern_state;             // Store LED pattern
volatile unsigned char receivedData = 0;    // Recieved data

//------------------------------------------------------------------------------
// Begin I2C initialization
//------------------------------------------------------------------------------
void slave_i2c_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;           // Stop Watchdog Timer

    // Configure P1.2 as SDA and P1.3 as SCL
    P1SEL1 &= ~(BIT2 | BIT3);
    P1SEL0 |= BIT2 | BIT3;

    // Configure USCI_B0 as I2C Slave
    UCB0CTLW0 |= UCSWRST;               // Put eUSCI_B0 into software reset
    UCB0CTLW0 |= UCMODE_3;              // Select I2C slave mode
    UCB0I2COA0 = SLAVE_ADDR + UCOAEN;   // Set and enable first own address
    UCB0CTLW0 |= UCTXACK;               // Send ACKs

    PM5CTL0 &= ~LOCKLPM5;               // Disable low-power inhibit mode

    UCB0CTLW0 &= ~UCSWRST;              // Pull eUSCI_B0 out of software reset
    UCB0IE |= UCSTTIE + UCRXIE;         // Enable Start and RX interrupts

    __enable_interrupt();               // Enable Maskable IRQs
}
//--End I2C initializatoin------------------------------------------------------

//------------------------------------------------------------------------------
// Begin LED initialization
//------------------------------------------------------------------------------
void init_led_bar()
{
    WDTCTL = WDTPW | WDTHOLD;  // Stop Watchdog Timer
    
    // Setup Ports
    P1OUT &= ~0xFF;             // Clear  Pin 3 to start
    P1DIR |= 0xFF;              // Config Pin 3 as output
    P2OUT &= ~0xFF;             // Clear  Pin 4 to start
    P2DIR |= 0xFF;              // Config Pin 4 as output

    // Setup Timer
    TB1CTL |= TBCLR;            // Clear timers and dividers
    TB1CTL |= TBSSEL__ACLK;     // Source = ACLK
    TB1CTL |= MC__UP;           // Mode = UP
    TB1CCR0 = timing_base;      // CCR0 = default 1 second

    // Setup Timer Compare IRQ
    TB1CCTL0 &= ~CCIFG;         //Clear CCR0 Flag
    TB1CCTL0 |= CCIE;           // Enable TB0 CCR0 Overflow IRQ
    __enable_interrupt();       // Enable Maskable IRQ
    PM5CTL0 &= ~LOCKLPM5;
}
//--End LED Initialization------------------------------------------------------

//------------------------------------------------------------------------------
// Begin status indicator initialization
//------------------------------------------------------------------------------
void init_status_indicator()
{
    // Setup Timer
    TB0CTL |= TBCLR;            // Clear timers and dividers
    TB0CTL |= TBSSEL__ACLK;     // Source = ACLK
    TB0CTL |= MC__UP;           // Mode = UP
    TB0CCR0 = 32768;            // CCR0 = 32768 (1s overflow)
    __enable_interrupt();       // Enable Maskable IRQ
}
//--End Stat Ind Initialization-------------------------------------------------

//------------------------------------------------------------------------------
// Begin Display LED Patterns
//------------------------------------------------------------------------------
void display_led_pattern()
{
    P1OUT = ledPattern_state;
    P2OUT = (P2OUT & 0x3F) | ((ledPattern_state & 0x0C) << 4);
}
//--End Display LED Patterns----------------------------------------------------

//------------------------------------------------------------------------------
// Begin LED Patterns
//------------------------------------------------------------------------------
void led_patterns(char key_cur) 
{
    switch(key_cur)
    {
        case 'A':           // Heat
            if (new_input_bool) {
                if (key_cur == key_prev | pattern_a_start == false)
                {
                    ledPattern_state = ledOff;
                    pattern_a_start = true;
                }
                else
                    ledPattern_state = pattern_a_cur;
                new_input_bool = false;
            }
            else if ((ledPattern_state != 0xFF))  // Fill to the ??
            {
                ledPattern_state = (ledPattern_state >> 1) | 0x80;  // Shift right and force leftmost bit ON
            }
            else  // Reset LED bar
            {
                ledPattern_state = ledOff;
            }
            break;
        case 'B':           // Cool
            if (new_input_bool) {
                if (key_cur == key_prev | pattern_b_start == false)
                {
                    ledPattern_state = ledOff;
                    pattern_b_start = true;
                }
                else
                    ledPattern_state = pattern_a_cur;
                new_input_bool = false;
            }
            else if ((ledPattern_state != 0xFF))  // Fill to the ??
            {
                ledPattern_state = (ledPattern_state << 1) | 0x01;  // Shift right and force leftmost bit ON
            }
            else  // Reset LED bar
            {
                ledPattern_state = ledOff;
            }
            break;
        default:             // Off
            ledPattern_state = ledOff;
            break;
    }
    display_led_pattern();
}
//--End LED Patterns------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Set LED Bar
//------------------------------------------------------------------------------
void set_led_bar(char key_input)
{
    key_prev = key_cur;
    key_cur = key_input;
    new_input_bool = true;
    led_patterns(key_cur);
}
//--End Set LED Bar-------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Main
//------------------------------------------------------------------------------
int main(void)
{
    init_led_bar();
    init_status_indicator();
    slave_i2c_init();                   // Initialize the slave for I2C
    __bis_SR_register(LPM0_bits + GIE); // Enter LPM0, enable interrupts
    return 0;
}
//--End Main--------------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Interrupt Service Routines
//------------------------------------------------------------------------------
// Timer for pattern
#pragma vector = TIMER1_B0_VECTOR
__interrupt void ISR_TB1_CCR0(void)
{
    led_patterns(key_cur);
    TB1CCTL0 &= ~CCIFG;
}
//------------------------------------------------------------------------------
// Interrupt for Status LED (currently not working)
// ToDo: Fix status LED
#pragma vector = TIMER0_B0_VECTOR
__interrupt void ISR_TB0_CCR0(void)
{
    P2OUT &= ~BIT0;              // Turn off status indicator
    TB0CCTL0 &= ~CCIFG;
    TB0CCTL0 &= ~CCIE;          // Disable TB0 interrupt until next I2C receive
}
//------------------------------------------------------------------------------
// I2C Receive key press and set pattern
#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
    new_input_bool = true;
    switch (__even_in_range(UCB0IV, USCI_I2C_UCTXIFG0))
    {
        case USCI_I2C_UCRXIFG0:         // Receive Interrupt
            set_led_bar(UCB0RXBUF);    // Read received data
            P2OUT |= BIT0;              // Turn on status indicator
            TB0CCTL0 &= ~CCIFG;         //Clear CCR0 Flag
            TB0CCTL0 |= CCIE;           // Enable TB0 interrupt
            break;
        default: 
            break;
    }
}
//-- End Interrupt Service Routines --------------------------------------------
