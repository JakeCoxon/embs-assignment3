#include "vlab.h"

#define SAMPLE_W8 1
#define SAMPLE_W16 2


DECLARE_INTERRUPT_HANDLER(int_handler);

void process_packet(void);
void send_test(void);

int flash = 1;
int channel_id = 5;
int to_initialise = 0;


void int_handler() {
  int vec = intc_get_vector();

  switch (vec) {
    case INTC_FSL: 
      break;
    case INTC_SWITCHES:
      switches_clear_interrupt();
      to_initialise = 1;
      channel_id = ((int) get_switches()) + 1;
      uart_print_hex_16(UART, channel_id);
      //send_test();
      set_leds(flash);
      flash = !flash;
      break;
      
    case INTC_MAC:
      process_packet();
      break;
  }
  intc_acknowledge_interrupt(vec);
}


void process_packet() {
  int temp, i;
  volatile int *buffer, *dataptr;
 
  buffer             = mac_packet_ready();
  temp               = *(buffer+3);
  
  if (temp == (0x55AA0000 | channel_id)) {
  
    // 11111111     11111111      11111111  11111111
    // sample rate  sample width  reserved  reserved
    temp             = *(buffer + 4);
    putfslx(temp, 0, FSL_BLOCKING);

    // 11111111111111111111111111111111
    // index of packet
    temp             = *(buffer + 5);

    // 11111111111111111111111111111111
    // length of packet data
    temp             = *(buffer + 6);
    int len4         =  (temp >> 2);

    // 1      11111111111111111111111111111
    // init   length/4 of packet data
    temp             =  len4 | (to_initialise << 31);
    putfslx(temp, 0, FSL_BLOCKING);

    dataptr          =   buffer + 7;
    
    set_leds(flash);
    flash = !flash;

    for (i = 0; i < len4; i ++) {

      temp = *(dataptr+i);

      putfslx(temp, 0, FSL_BLOCKING);

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


int get_channel_ids() {
  int accept = 0;
  int channel_ids = 0;
  while (!accept) {
    char c; char c1 = '0'; char c2 = '5';
    channel_ids = 0;

    for (int i = 5; i <= 20; i++, c2++) {
      if (c2 > '9') { c2 = '0'; c1 ++; }

      uart_send_string(UART, "Mix channel ");
      uart_send_char(UART, c1); uart_send_char(UART, c2);
      uart_send_string(UART, ": ");

      int to_play = ((c = uart_wait_char(UART)) == '1' || c == 'y');
      uart_send_string(UART, to_play ? "Yes\r\n" : "No\r\n");
      channel_ids |= to_play << (20-i);
    }
    uart_send_string(UART, "Accept? ");
    accept = ((c = uart_wait_char(UART)) == '1' || c == 'y');
    uart_send_string(UART, "\r\n");
  }

  uart_send_string(UART, "Job done\r\n");
  return channel_ids;
}


int main(void) {
  
  initialise();

  // Setup mac adress
  *(MAC_TX1 + 0) = 0x00112233;
  *(MAC_TX1 + 1) = 0x00070000;
  mac_set_mac_address();

  //int channel_ids = get_channel_ids();


  intc_enable_all_interrupts();
  //buttons_enable_interrupts();
  switches_enable_interrupts();
  //uart_enable_interrupts(UART);
  mac_enable_interrupts();
  send_test();

  channel_id = ((int) get_switches()) + 1;
  uart_print_hex_16(UART, channel_id);

  volatile int counter = 0;
  while (1) {}
 
  return 0;
}
