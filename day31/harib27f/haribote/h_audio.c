#include "bootpack.h"
// 39行目からbootpack.hに構造体等を書いています

// static char corb_buffer[4096] __attribute__((aligned(4096)));
// static char rirb_buffer[4096] __attribute__((aligned(4096)));
static volatile struct hdaudio_mmio *mmio = (volatile struct hdaudio_mmio *) BAR0;

void init_corb(void){
  //  CORB CTLのリセット(最後のRUNにする)
  // p37, 1bit目を0にする
  mmio->corbctl = mmio->corbctl | ~(1 << 1);
  // CORB wpのリセット
  mmio->corbwp = 0;
  // CORB rpの15bit目に1を書き込みその後0を書き込む
  mmio->corbrp = mmio->corbrp & (1 << 15);
  mmio->corbrp = 0;


  //  CORB CTLのRUN
  mmio->corbctl = mmio->corbctl | (1 << 1);
}

void init_rirb(void){
  // RIRB wpのリセット
  mmio->rirbwp = mmio->rirbwp & (1 << 15);

}


void hdaudio_test(struct BOOTINFO *binfo,struct MEMMAN *man){
  int iss = (mmio->gcap >> 8) & 0xf;
  int oss = (mmio->gcap >> 12) & 0xf;
  int x = (0x80 + iss * 0x20);
  int y = (0x80 + iss * 0x20) + (oss*0x20);
  volatile struct hdaudio_input_stream_descriptor *input_desc = BAR0 + x;
  volatile struct hdaudio_output_stream_descriptor *output_desc = BAR0 + y;

  // mmio->rirbctl |= 1 << 1;
  // mmio->rirbctl = mmio->rirbctl & ~0x1;
  // mmio->rirbctl = (mmio->rirbctl & ~0x1c) | (0x05 << 2);

  //  0x1c = 00011100
  // ~0x1c = 11100011
  //         10011000
  //         --------
  //         10000000
  //         --------
  //         10010100
  //         7  ^^^ 0


  // 0x12003456 <- *un*aligned

  // 0x12003000 <- 4KB-aligned

  // 4KB-aligned addresses (4KiB == 4096 == 0x1000):
  // 0x0000
  // 0x1000
  // 0x2000
  // 0x3000
  //    ^^^
  // ...
  // assert(addr & ~(0x1000 - 1) == 0);
  //                 ^^^^^^^^^^^
  //                     0xfff
  
  // 2KB-aligned addresses (2KiB == 2048 == 0x800)
  // 0x0000
  // 0x0800
  // 0x1000 <-- 4KiB-aligned
  // 0x1800
  // 0x2000 <-- 4KiB-aligned
  // ...

  // assert(addr & ~(0x800 - 1) == 0);
  //                 ^^^^^^^^^^
  //                   0x7ff
  
  char s[128];
  sprintf(s, "gcap: %x", mmio->gcap);
  putfonts8_asc(binfo->vram, binfo->scrnx, 100, 320, COL8_FFFFFF, s);

  // int test_buffer = (int *) memman_alloc_4k(man, 4096);
  // char t[4096];
  // sprintf(t, "CORBSIZE: %x", test_buffer);
  // putfonts8_asc(binfo->vram, binfo->scrnx, 100, 350, COL8_FFFFFF, t);

  // printf("gcap = %x\n", mmio->gcap);
}

