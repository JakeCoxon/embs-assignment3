#include "vlab.h"

#define SAMPLE_W8 1
#define SAMPLE_W16 2


DECLARE_INTERRUPT_HANDLER(int_handler);

void process_packet(volatile int *buffer);
void send_test(void);

#define PS(x) uart_send_string(UART, x);
#define PC(x) uart_send_char(UART, x);


int to_initialise = 0;
int channels[] = {0,0,0,0};
int num_channels = 0;
volatile int *buffer;



void set_channel_switches() { 
  int switches = (int) get_switches();

 
  if ((switches & 0b1000) == 0b1000)  // Task 1 switches
    channels[0] = (switches & 0b111) + 5;
  else                                // Task 2 switches
    channels[0] = (switches & 0b11) + 1;
  
  num_channels = 1;
}


void int_handler() {
  int vec = intc_get_vector();

  switch (vec) {
    case INTC_FSL: 
      break;
    case INTC_SWITCHES:
      switches_clear_interrupt();

      to_initialise = 1;
      set_channel_switches();

      break;
      
    case INTC_MAC:
      buffer = mac_packet_ready(); 
      // Process all data packets
      while (buffer != (void *) 0) {
        process_packet(buffer);
        buffer = mac_packet_ready();
      }
      break;
  }
  intc_acknowledge_interrupt(vec);
}



void process_packet(volatile int *buffer) {
  int temp, i;
  volatile int *dataptr;
 
  buffer             = mac_packet_ready();
  temp               = *(buffer + 3);

  int mix_number = -1;
  for (i = 0; i < num_channels && mix_number == -1; i ++) {
    if (temp == (0x55AA0000 | channels[i])) mix_number = i;
  }
  
  if (mix_number > -1) {
  
    // sample rate  sample width  mix  mix id  num count  unused
    // 11111111     11111111      1    11      111        1111111111
    temp             = *(buffer + 4);

    temp            |= (mix_number & 0b11)    << 13;
    temp            |= (num_channels & 0b111) << 10;
    putfslx(temp, 0, FSL_BLOCKING);

    // index of packet
    // 11111111111111111111111111111111
    temp             = *(buffer + 5);

    // length of packet data
    // 11111111111111111111111111111111
    temp             = *(buffer + 6);
    int len4         =  (temp >> 2);

    // init   length/4 of packet data
    // 1      11111111111111111111111111111
    temp             =  len4 | (to_initialise << 31);
    putfslx(temp, 0, FSL_BLOCKING);

    dataptr          =   buffer + 7;
  

    for (i = 0; i < len4; i ++, dataptr ++) {

      putfslx(*dataptr, 0, FSL_BLOCKING);

    }

  }

  to_initialise = 0;
  
  //Packet read, clear it out
  mac_clear_rx_packet(buffer);
  
}

void send_test(void) {
  *(MAC_TX1 + 0) = 0xFFFFFFFF; //Destination MAC
  *(MAC_TX1 + 1) = 0xFFFF0011; //Destination MAC, Source MAC
  *(MAC_TX1 + 2) = 0x22330007; //Source MAC
  *(MAC_TX1 + 3) = 0x55AA0011; //Type, Data
  *(MAC_TX1 + 4) = 0x22334455; //Data
  *(MAC_TX1 + 5) = 0x66778899; //Data
  *(MAC_TX1 + 6) = 0xAABBCCDD; //Data
  mac_send_packet(28);
}


void input_channel_ids() {

  char in_c, digit0, digit1; 

  PS("Press a key to start mixing up to 4 channels. Then use 1 or 0 to select yes or no\r\n")
  uart_wait_char(UART);

  num_channels = 0; digit1 = '0'; digit0 = '5';
  channels[0] = 0; channels[1] = 0; channels[2] = 0; channels[3] = 0;

  // Loop to channel 20 or until num channels is 4
  for (int i = 5; i <= 20 && num_channels < 4; i++, digit0++) {

    // Increase character digits for output
    if (digit0 > '9') { digit0 = '0'; digit1 ++; }
    PS("Mix channel ") PC(digit1) PC(digit0) PS("/20: ")

    // Wait until input character is 1 or 0
    do { in_c = uart_wait_char(UART); } while (in_c != '1' && in_c != '0');

    // Update channel array
    if (in_c == '1') channels[num_channels ++] = i;

    PS((in_c == '1') ? "Yes\r\n" : "No\r\n")

  }

  PC(num_channels + '0') PS(" channels selected\r\n")
}


int main(void) {
  
  initialise();

  // Setup mac adress
  *(MAC_TX1 + 0) = 0x00112233;
  *(MAC_TX1 + 1) = 0x00070000;
  mac_set_mac_address();


  intc_enable_all_interrupts();
  switches_enable_interrupts();
  mac_enable_interrupts();
  send_test();

  set_channel_switches();
  
  while (1) {
    input_channel_ids();
  }
 
  return 0;
}
