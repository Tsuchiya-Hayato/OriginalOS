#include "bootpack.h"
// 39行目からbootpack.hに構造体等を書いています
static volatile struct hdaudio_mmio *mmio = (volatile struct hdaudio_mmio *) BAR0;
static int corb_num_entries;
static int rirb_num_entries;
static uint32_t corb_buffer;
static uint32_t rirb_buffer;
static uint32_t corb_wp = 1;
static int iss;
static int oss;
static int speaker_node = -1;
static int pin_node = -1;


static uint32_t corb_enqueue(int cad, uint32_t nid, uint32_t verb, uint32_t payload){
  uint32_t *queue = (uint32_t *) corb_buffer;
  uint32_t current = corb_wp % corb_num_entries;
  uint32_t data = (cad << 28) | (nid << 20) | (verb << 8) | payload;
  queue[current] = data;
  mmio->corbwp = current;
  corb_wp++;
  return current;
}

static uint32_t rirb_data(uint32_t index) {
    uint32_t *resps = (uint32_t *) rirb_buffer;
    return resps[index * 2];
}

static uint32_t run_command(uint32_t nid, uint32_t verb, uint32_t payload) {
    int cad = 0;
    uint32_t idx = corb_enqueue(cad, nid, verb, payload);
    while ((mmio->rirbsts & 1) == 0);
    mmio->rirbsts =  mmio->rirbsts & 0x5;
    return rirb_data(idx);
}

void init_corb(struct MEMMAN *man){
  //  CORB CTLのリセット(最後のRUNにする),p37
  // p37, 1bit目を0にする
  mmio->corbctl = mmio->corbctl | ~(1 << 1);

  //sizeの取得
  corb_num_entries = 256;

  // memoryのアドレス書き込み,p36
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

// memoryのアドレス書き込み,p36
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

static int look_for_node(uint8_t parent_nid, uint8_t target_widget_type) {
    uint32_t value = run_command(parent_nid, VERB_GET_PARAM, PARAM_NODE_CNT);
    uint32_t nid_base = (value >> 16) & 0xff;
    uint32_t num_nodes = value & 0xff;
    if (!num_nodes) {
        return -1;
    }
    for (uint32_t i = 0; i < num_nodes; i++) {
        uint32_t value = run_command(nid_base + i, VERB_GET_PARAM,
                                     PARAM_AUDIO_WIDGET_CAPS);
        uint8_t widget_type = (value >> 20) & 0xf;
        if (widget_type == target_widget_type) {
            return nid_base + i;
        }

        int nid = look_for_node(nid_base + i, target_widget_type);
        if (nid >= 0) {
            return nid;
        }
    }
    return -1;
}

/// Initializes a stream.
static void init_out_stream(struct hdaudio_stream *stream, int id) {

    stream->id = id;
    stream->offset = 0x80 + gcap_iss * 0x20 + (id - 1) * 0x20;
    stream->buffer_size = 512 * 1024 /* 512KiB */ * 2;
    stream->buffer_dma = dma_alloc(stream->buffer_size, DMA_ALLOC_FROM_DEVICE);

    // Set the stream ID.
    io_write32(regs, REG_SDnCTL(stream),
        (io_read32(regs, REG_SDnCTL(stream))
             & ~(0xf << 20)) | (stream->id << 20));

    // Populate the buffer descriptor list.
    stream->bdl_dma = dma_alloc(stream->buffer_size, DMA_ALLOC_FROM_DEVICE);
    struct hdaudio_buffer_desc *bd_list =
        (struct hdaudio_buffer_desc *) dma_buf(stream->bdl_dma);

    // The spec requires at least two entries (see "3.3.39 Offset 8C: {IOB}ISDnLVI"
    // in the spec).
    size_t num_bds = 2;
    bd_list[0].buffer_addr = dma_daddr(stream->buffer_dma);
    bd_list[0].buffer_len = stream->buffer_size;
    bd_list[0].ioc = 0;
    bd_list[1].buffer_addr = 0;
    bd_list[1].buffer_len = 0;
    bd_list[1].ioc = 0;

    io_write32(regs, REG_SDnCBL(stream), stream->buffer_size);
    io_write32(regs, REG_SDnLVI(stream), num_bds - 1);
    io_write32(regs, REG_SDnBDLPL(stream), dma_daddr(stream->bdl_dma));
    io_write32(regs, REG_SDnBDLPU(stream), dma_daddr(stream->bdl_dma) >> 32);
}





void hdaudio_test(struct BOOTINFO *binfo,struct MEMMAN *man){
  iss = (mmio->gcap >> 8) & 0xf;
  oss = (mmio->gcap >> 12) & 0xf;
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

