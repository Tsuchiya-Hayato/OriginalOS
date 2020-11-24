#include "bootpack.h"
// 39行目からbootpack.hに構造体等を書いています
static volatile struct hdaudio_mmio *mmio = (volatile struct hdaudio_mmio *) BAR0;
static int corb_num_entries;
static int rirb_num_entries;
static unsigned int corb_buffer;
static unsigned int rirb_buffer;


// static uint32_t corb_enquenu(){

// }



void init_corb(struct MEMMAN *man){
  //  CORB CTLのリセット(最後のRUNにする),p37
  // p37, 1bit目を0にする
  mmio->corbctl = mmio->corbctl | ~(1 << 1);

  //sizeの取得
  corb_num_entries = 256;

  // memmoryのアドレス書き込み,p36
  corb_buffer = memman_alloc_4k(man, 4096);

  // Alignment:
  //
  // xxxx xx00 0000 0000 = 1KiB = 1024
  // xxxx 0000 0000 0000 = 4KiB = 4096
  //
  // x = 1 or 0

  //
  // CPU <----> MEMORY 
  //  ^              ^
  //  | io, out命令   | DMA
  //  V              |
  // HD Audio  <-----+
  //

  //  63_______________32|31______________________0
  // |    upper          |         low            |
  // ----------------------------------------------
  mmio->corblbase = corb_buffer;
  mmio->corbubase = (corb_buffer >> 31);
  // CORB wpのリセット
  mmio->corbwp = 0;
  // CORB rpの15bit目に1を書き込みその後0を書き込む
  mmio->corbrp = mmio->corbrp | (1 << 15);
  mmio->corbrp = 0;
  //  CORB CTLのRUN
  mmio->corbctl = mmio->corbctl | (1 << 1);
}

void init_rirb(struct MEMMAN *man){
  // RIRB CTLのリセット(最後のRUNにする),p40
  // p40, 1bit目を0にする
  mmio->rirbctl = mmio->rirbctl | ~(1 << 1);

  // RIRB sizeの取得
  rirb_num_entries = 256;

// memmoryのアドレス書き込み,p36
  rirb_buffer = memman_alloc_4k(man, 4096);
  mmio->rirblbase = rirb_buffer;
  mmio->rirbubase = (rirb_buffer >> 31);

  // RIRB wpのリセット, p39
  // 15bit目に1を書き込み、0を書き込み
  mmio->rirbwp = mmio->rirbwp | (1 << 15);
  mmio->rirbwp = 0;

  // RIRB CNTの0bit目に1を入れる
  mmio->rintcnt = mmio->rintcnt | (1 << 0);

  // RIRB CTLの0と1bit目を1にする
  mmio->rirbctl = mmio->rirbctl | (1 << 1);
  mmio->rirbctl = mmio->rirbctl | (1 << 0);
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
  sprintf(s, "CORBSIZE: %x", mmio->corbsize);
  putfonts8_asc(binfo->vram, binfo->scrnx, 100, 340, COL8_FFFFFF, s);
  sprintf(s, "RIRBSIZE: %x",  mmio->rirbsize);
  putfonts8_asc(binfo->vram, binfo->scrnx, 100, 360, COL8_FFFFFF, s);

  int test_buffer = (int *) memman_alloc_4k(man, 4096);
  sprintf(s, "memory size: %dKB", sizeof(test_buffer));
  putfonts8_asc(binfo->vram, binfo->scrnx, 100, 400, COL8_FFFFFF, s);
}

