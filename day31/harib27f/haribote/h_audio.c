#include "bootpack.h"


int main(){
  volatile struct hdaduio_mmio *mmio = (volatile struct hdaduio_mmio *) BAR0;
  iss = (mmio->gcap >> 8) & 0xf;
  oss = (mmio->gcap >> 12) & 0xf;
  int x = (0x80 + iss * 0x20);
  int y = (0x80 + iss * 0x20) + (oss*0x20);
  volatile struct hdaudio_input_stream_descriptor *input_desc = BAR0 + x;
  volatile struct hdaudio_output_stream_descriptor *output_desc = BAR0 + y;
  printf("gcap = %x\n", mmio->gcap);
}

