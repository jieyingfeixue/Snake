/********************************************************
???????????????
???????? TFT9341 + DAC/DMA/TIM3 ???????
********************************************************/

#include <stdlib.h>
#include "stm32f10x.h"
#include "hw_config.h"
#include "lcd.h"

/* last 2KB page of 256KB internal flash (STM32F107VC) */
#define FLASH_SAVE_ADDR  0x0803F800u
#define SAVE_MAGIC       0x534Bu   /* 'SN' */

/* -------------------- ?????????????????? -------------------- */
#define KEY_UP_PRESSED()     (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0)
#define KEY_DOWN_PRESSED()   (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_12) == 0)
#define KEY_LEFT_PRESSED()   (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 0)
#define KEY_RIGHT_PRESSED()  (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)

/* -------------------- ??????? -------------------- */
#define CELL_SIZE      10
#define BOARD_X        10
#define BOARD_Y        30
#define BOARD_W        22
#define BOARD_H        22
#define BOARD_CELLS    (BOARD_W * BOARD_H)
#define MAX_SNAKE_LEN  (BOARD_W * BOARD_H)
#define HUD_H          25
#define FOOT_Y         252

/* -------------------- Cyberpunk neon theme -------------------- */
#define NEON_VOID      0x0008
#define UI_BG          0x0606
#define UI_PANEL       0x1966
#define UI_PANEL_HI    0x2D0F
#define UI_GRID        0x1208
#define NEON_CYAN      0x07FF
#define NEON_MAG       0xF81F
#define NEON_PINK      0xFB9F
#define NEON_PURPLE    0x901F
#define NEON_LIME      0x07FE
#define NEON_YELLOW    0xFFE0
#define NEON_DIM       0x1808
#define NEON_MUTED     0x4A69
#define NEON_SEL_BG    0x4028
#define NEON_SEL_FG    0x07FF
#define NEON_VAL       0x07FE
#define SNAKE_OUTLINE  0xFFFF
#define SNAKE_HEAD     0xBFFF
#define SNAKE_BODY     0x07FF
#define SNAKE_BODY2    0x05DF
#define SNAKE_GLOW     0x03FF
#define P2_OUTLINE     0xFFDF
#define P2_HEAD        0xFF5F
#define P2_BODY        0xFB9F
#define P2_BODY2       0xF81F
#define P2_GLOW        0xC01F
#define FOOD_MAIN      0xF800
#define FOOD_DOT       0xFFE0
#define FOOD_GLOW      0xF81F

#define MENU_ITEM_Y0   62
#define MENU_ITEM_STEP 19
#define MENU_ITEM_CNT  5

/* -------------------- ????????????????? -------------------- */
uint32_t DAC_DHR12R2_Address = 0x40007414;
const uint16_t Sine12bit[32] = {
  2047,2447,2831,3185,3498,3750,3939,4056,4095,4056,3939,3750,3495,3185,2831,2447,
  2047,1647,1263,909,599,344,155,38,0,38,155,344,599,909,1263,1647
};
uint32_t DAC2Sine12bit[32];

#define BGM_NOTE_TICKS      8    /* 8 x 20ms = 160ms per note (~150ms) */
#define MARIO_LOOP_START   16    /* skip 16-note opening fanfare on repeat */

/* Super Mario overworld theme (full), 150ms per note, 144 steps */
static const uint16_t Mario_Full[] = {
  /* 1-16: opening fanfare */
  659*2, 659*2, 0,     659*2, 0,     523*2, 659*2, 0,
  784*2, 0,     0,     784,   0,     0,     0,     0,

  /* 17-32: main theme A-1 */
  523*2, 0,     0,     784,   0,     0,     659,   0,
  0,     880,   0,     988,   0,     932,   880,   0,

  /* 33-48: main theme A-2 */
  784,   659*2, 0,     784*2, 880*2, 0,     698*2, 784*2,
  0,     659*2, 0,     523*2, 587*2, 988,   0,

  /* 49-64: main theme B-1 (la-sol-mi) */
  0,     0,     659,   0,     0,     880,   0,     880,
  0,     932,   988,   0,     880,   784,   0,     659,

  /* 65-80: main theme B-2 */
  880*2, 0,     523*4, 587*4, 0,     988*2, 0,     0,
  0,     0,     0,     0,     0,     0,     0,     0,

  /* 81-96: extension-1 */
  0,     784*2, 740*2, 698*2, 622*2, 659*2, 0,     440,
  494,   523*2, 0,     440,   494,   523*2, 587*2, 0,

  /* 97-112: extension-2 */
  0,     784*2, 740*2, 698*2, 622*2, 659*2, 0,     523*4,
  0,     523*4, 523*4, 0,     0,     0,     0,     0,

  /* 113-128: staircase transition */
  523*2, 0,     0,     523*2, 0,     523*2, 587*2, 0,
  659*2, 523*2, 0,     880,   784,   0,     0,     0,

  /* 129-144: staircase-2 and ending */
  523*2, 0,     0,     523*2, 0,     523*2, 587*2, 659*2,
  0,     0,     0,     0,     0,     0,     0,     0
};
#define Mario_FullLen (sizeof(Mario_Full) / sizeof(Mario_Full[0]))

/* Adventure Island death SFX: 80ms per step, descending + tail */
static const uint16_t Adventure_Die[] = {
  1046*2, 0, 988, 0, 880, 0, 784, 0,
  698, 0, 659, 0, 523, 523, 523, 0
};
#define AdventureDieLen (sizeof(Adventure_Die) / sizeof(Adventure_Die[0]))
#define ADVENTURE_DIE_MS  80

static uint8_t g_audio_vol = 80;

static void audio_rebuild_wavetable(void)
{
  uint8_t i;
  uint32_t s;

  for (i = 0; i < 32; i++) {
    s = ((uint32_t)Sine12bit[i] * g_audio_vol) / 100u;
    DAC2Sine12bit[i] = (s << 16) | s;
  }
}

static uint8_t audio_volume_service(void)
{
  uint8_t vol;

  vol = pot_read_volume_pct();
  if (vol != g_audio_vol) {
    g_audio_vol = vol;
    audio_rebuild_wavetable();
    return 1;
  }
  return 0;
}

static uint8_t g_bgm_idx = 0;
static uint8_t g_bgm_tick = 0;

/* -------------------- ????? -------------------- */
typedef enum {
  GS_BOOT = 0,
  GS_MENU,
  GS_RUN,
  GS_PAUSE,
  GS_OVER
} GameState;

typedef enum { DIR_UP=0, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;
typedef enum { DIF_EASY=0, DIF_NORMAL, DIF_HARD } Difficulty;
typedef enum { DIE_NONE=0, DIE_WALL, DIE_BODY, DIE_RIVAL } DieReason;
typedef enum { VS_NONE=0, VS_P1_WIN, VS_P2_WIN, VS_DRAW } VsResult;
typedef enum { VR_NONE=0, VR_WALL, VR_SELF, VR_RIVAL, VR_HEAD } VsReason;

typedef struct { uint8_t x; uint8_t y; } Point;

static GameState g_state = GS_BOOT;
static Difficulty g_diff = DIF_NORMAL;
static uint8_t g_music_on = 0;
static uint16_t g_score = 0;
static uint16_t g_best = 0;
static uint8_t g_level = 1;

static Point snake[MAX_SNAKE_LEN];
static uint16_t snake_len = 0;
static Dir dir_now = DIR_RIGHT;
static Dir dir_next = DIR_RIGHT;
static Point snake2[MAX_SNAKE_LEN];
static uint16_t snake2_len = 0;
static Dir dir2_now = DIR_LEFT;
static Dir dir2_next = DIR_LEFT;
static Point food;
static uint8_t move_interval_ticks = 7;
static uint8_t tick_cnt = 0;

static uint8_t g_pause_drawn = 0;
static uint8_t g_key4_hold_ticks = 0;
static uint8_t g_pause_block_ticks = 0;
static uint8_t g_input_block_ticks = 0;
static uint8_t g_key_click_lock = 0;
static uint32_t g_rng_state = 1;
static uint32_t g_rng_stir = 0;
static uint8_t g_food_phase = 0;
static uint8_t g_anim_tick = 0;
static uint8_t g_level_flash = 0;
static uint8_t g_score_flash = 0;
static DieReason g_die_reason = DIE_NONE;

#define KEY_CONFIRM    5

static uint8_t g_uart_key = 0;
static uint8_t g_uart_j_pending = 0;
static uint8_t g_uart_j_cd = 0;
static uint8_t g_demo_mode = 0;
static uint8_t g_demo_restart_wait = 0;
static uint8_t g_versus_mode = 0;
static uint8_t demo_vis[BOARD_H][BOARD_W];
static uint8_t demo_first[BOARD_H][BOARD_W];
static uint16_t demo_qx[BOARD_CELLS];
static uint16_t demo_qy[BOARD_CELLS];

static uint8_t key_scan_click(void);
static void key4_reset_hold(void);
static void key_wait_release(void);
static void bgm_stop(void);
static void sfx_play_eat(void);
static void sfx_play_gameover(void);
static void show_boot(void);
static void enter_run_mode(void);
static void enter_demo_mode(void);
static void enter_versus_mode(void);
static void show_menu(uint8_t sel);

static void rng_stir_once(void)
{
  g_rng_stir ^= (uint32_t)SysTick->VAL;
  g_rng_stir ^= (uint32_t)GPIOA->IDR << 5;
  g_rng_stir ^= (uint32_t)GPIOD->IDR << 11;
  g_rng_stir = (g_rng_stir << 13) | (g_rng_stir >> 19);
}

static void rng_reseed(void)
{
  uint32_t seed = 0xC0FFEE01u ^ g_rng_stir;
  uint8_t i;

  for (i = 0; i < 8; i++) {
    rng_stir_once();
    seed ^= (uint32_t)SysTick->VAL;
    seed = seed * 1664525u + 1013904223u;
  }

  seed ^= (uint32_t)g_best << 8;
  seed ^= (uint32_t)tick_cnt << 16;
  if (seed == 0) seed = 1;
  g_rng_state = seed;
}

static uint32_t rng_next(void)
{
  g_rng_state = g_rng_state * 1664525u + 1013904223u;
  return g_rng_state;
}

static uint8_t rng_range(uint8_t n)
{
  return (uint8_t)(rng_next() % n);
}

static const u8* diff_name(void)
{
  if (g_diff == DIF_EASY) return (u8*)"EASY";
  if (g_diff == DIF_HARD) return (u8*)"HARD";
  return (u8*)"NORMAL";
}

static void save_load_best(void)
{
  uint16_t magic = *(volatile uint16_t*)FLASH_SAVE_ADDR;
  uint16_t best = *(volatile uint16_t*)(FLASH_SAVE_ADDR + 2);

  if (magic == SAVE_MAGIC && best <= 9999u) {
    g_best = best;
  }
}

static void save_store_best(void)
{
  uint16_t magic = *(volatile uint16_t*)FLASH_SAVE_ADDR;
  uint16_t stored = *(volatile uint16_t*)(FLASH_SAVE_ADDR + 2);
  FLASH_Status st;

  if (magic == SAVE_MAGIC && stored == g_best) return;

  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
  st = FLASH_ErasePage(FLASH_SAVE_ADDR);
  if (st == FLASH_COMPLETE) {
    FLASH_ProgramHalfWord(FLASH_SAVE_ADDR, SAVE_MAGIC);
    FLASH_ProgramHalfWord(FLASH_SAVE_ADDR + 2, g_best);
  }
  FLASH_Lock();
}

static void draw_stat_line(uint16_t y, const u8 *label, uint16_t value, uint8_t width)
{
  Show_Str(36, y, NEON_CYAN, UI_PANEL, (u8*)label, 16, 0);
  POINT_COLOR = NEON_YELLOW;
  BACK_COLOR = UI_PANEL;
  LCD_ShowNum(110, y, value, width, 16);
}

static void uart_service(void)
{
  char c;

  while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
    c = (char)(USART_ReceiveData(USART1) & 0xFF);

    switch (c) {
      case 'w': case 'W':
        if (g_state == GS_RUN && g_input_block_ticks == 0) {
          if (g_versus_mode) {
            if (dir2_now != DIR_DOWN) dir2_next = DIR_UP;
          } else if (dir_now != DIR_DOWN) {
            dir_next = DIR_UP;
          }
        } else {
          g_uart_key = 1;
        }
        break;
      case 's': case 'S':
        if (g_state == GS_RUN && g_input_block_ticks == 0) {
          if (g_versus_mode) {
            if (dir2_now != DIR_UP) dir2_next = DIR_DOWN;
          } else if (dir_now != DIR_UP) {
            dir_next = DIR_DOWN;
          }
        } else {
          g_uart_key = 2;
        }
        break;
      case 'a': case 'A':
        if (g_state == GS_RUN && g_input_block_ticks == 0) {
          if (g_versus_mode) {
            if (dir2_now != DIR_RIGHT) dir2_next = DIR_LEFT;
          } else if (dir_now != DIR_RIGHT) {
            dir_next = DIR_LEFT;
          }
        }
        break;
      case 'd': case 'D':
        if (g_state == GS_RUN && g_input_block_ticks == 0) {
          if (g_versus_mode) {
            if (dir2_now != DIR_LEFT) dir2_next = DIR_RIGHT;
          } else if (dir_now != DIR_LEFT) {
            dir_next = DIR_RIGHT;
          }
        }
        break;
      case 'j': case 'J':
        g_uart_j_pending = 1;
        break;
      case 'k': case 'K':
        if (g_state == GS_OVER || g_state == GS_PAUSE) {
          g_uart_key = 3;
        }
        break;
      default:
        break;
    }
  }
}

static uint8_t uart_take_confirm(void)
{
  if (g_uart_j_cd > 0 || !g_uart_j_pending) return 0;
  g_uart_j_pending = 0;
  g_uart_j_cd = 10;
  return KEY_CONFIRM;
}

static uint8_t poll_key(void)
{
  uint8_t k = key_scan_click();

  if (k) return k;

  k = g_uart_key;
  g_uart_key = 0;
  if (k) return k;

  return uart_take_confirm();
}

static void uart_send_uint(uint16_t v)
{
  char buf[6];
  uint8_t n = 0;
  uint8_t i;

  if (v == 0) {
    usart1_send_byte('0');
    return;
  }
  while (v > 0 && n < 5) {
    buf[n++] = (char)('0' + (v % 10));
    v = (uint16_t)(v / 10);
  }
  for (i = n; i > 0; i--) {
    usart1_send_byte((uint8_t)buf[i - 1]);
  }
}

static const char* uart_die_reason_str(void)
{
  if (g_die_reason == DIE_WALL) return "WALL";
  if (g_die_reason == DIE_BODY) return "BODY";
  return "NONE";
}

static const char* uart_vs_reason_str(VsReason r)
{
  if (r == VR_WALL) return "WALL";
  if (r == VR_SELF) return "SELF";
  if (r == VR_RIVAL) return "RIVAL";
  if (r == VR_HEAD) return "HEAD";
  return "NONE";
}

static void uart_log_start_solo(uint8_t demo)
{
  usart1_send_str("[START] ");
  if (demo) {
    usart1_send_str("DEMO\r\n");
  } else {
    usart1_send_str("SOLO diff=");
    usart1_send_str((const char*)diff_name());
    usart1_send_str(" best=");
    uart_send_uint(g_best);
    usart1_send_str("\r\n");
  }
}

static void uart_log_start_vs(void)
{
  usart1_send_str("[START] 2P_VS P1=KEYS P2=UART\r\n");
}

static void uart_log_score(void)
{
  usart1_send_str("[SCORE] ");
  uart_send_uint(g_score);
  usart1_send_str(" len=");
  uart_send_uint(snake_len);
  usart1_send_str(" lv=");
  uart_send_uint(g_level);
  usart1_send_str("\r\n");
}

static void uart_log_level(void)
{
  usart1_send_str("[LEVEL] ");
  uart_send_uint(g_level);
  usart1_send_str("\r\n");
}

static void uart_log_game_over(void)
{
  usart1_send_str("[OVER] score=");
  uart_send_uint(g_score);
  usart1_send_str(" lv=");
  uart_send_uint(g_level);
  usart1_send_str(" len=");
  uart_send_uint(snake_len);
  usart1_send_str(" best=");
  uart_send_uint(g_best);
  usart1_send_str(" reason=");
  usart1_send_str(uart_die_reason_str());
  if (g_score > 0 && g_score >= g_best) {
    usart1_send_str(" NEW_RECORD");
  }
  usart1_send_str("\r\n");
}

static void uart_log_demo_over(void)
{
  usart1_send_str("[DEMO_OVER] score=");
  uart_send_uint(g_score);
  usart1_send_str(" len=");
  uart_send_uint(snake_len);
  usart1_send_str(" lv=");
  uart_send_uint(g_level);
  usart1_send_str("\r\n");
}

static void uart_log_vs_over(VsResult result, VsReason r1, VsReason r2)
{
  usart1_send_str("[VS_OVER] ");
  if (result == VS_P1_WIN) usart1_send_str("P1_WIN");
  else if (result == VS_P2_WIN) usart1_send_str("P2_WIN");
  else usart1_send_str("DRAW");
  usart1_send_str(" p1_len=");
  uart_send_uint(snake_len);
  usart1_send_str(" p2_len=");
  uart_send_uint(snake2_len);
  if (result == VS_P1_WIN) {
    usart1_send_str(" p2_reason=");
    usart1_send_str(uart_vs_reason_str(r2));
  } else if (result == VS_P2_WIN) {
    usart1_send_str(" p1_reason=");
    usart1_send_str(uart_vs_reason_str(r1));
  }
  usart1_send_str("\r\n");
}

/* pause_enter / pause_resume defined after show_pause_dialog */

/* -------------------- ??????? -------------------- */
static void restore_board_cell(uint8_t gx, uint8_t gy)
{
  uint16_t x1 = BOARD_X + (uint16_t)gx * CELL_SIZE;
  uint16_t y1 = BOARD_Y + (uint16_t)gy * CELL_SIZE;

  LCD_Fill(x1, y1, x1 + CELL_SIZE - 1, y1 + CELL_SIZE - 1, UI_BG);
}

static void draw_cell_clear(uint8_t gx, uint8_t gy)
{
  restore_board_cell(gx, gy);
}

static void draw_neon_scanlines(uint16_t y0, uint16_t y1)
{
  uint16_t y;

  POINT_COLOR = 0x0C08;
  for (y = y0; y <= y1; y += 4) {
    LCD_DrawLine(0, y, 239, y);
  }
}

static void draw_neon_corners(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  POINT_COLOR = NEON_CYAN;
  LCD_DrawLine(x1, y1, x1 + 10, y1);
  LCD_DrawLine(x1, y1, x1, y1 + 10);
  LCD_DrawLine(x2, y1, x2 - 10, y1);
  LCD_DrawLine(x2, y1, x2, y1 + 10);
  LCD_DrawLine(x1, y2, x1 + 10, y2);
  LCD_DrawLine(x1, y2, x1, y2 - 10);
  LCD_DrawLine(x2, y2, x2 - 10, y2);
  LCD_DrawLine(x2, y2, x2, y2 - 10);

  POINT_COLOR = NEON_MAG;
  LCD_DrawLine(x1 + 2, y1 + 2, x1 + 7, y1 + 2);
  LCD_DrawLine(x1 + 2, y1 + 2, x1 + 2, y1 + 7);
  LCD_DrawLine(x2 - 2, y2 - 2, x2 - 7, y2 - 2);
  LCD_DrawLine(x2 - 2, y2 - 2, x2 - 2, y2 - 7);
}

static void draw_snake_cell_p(uint8_t gx, uint8_t gy, uint8_t is_head, uint8_t seg_idx,
                              uint8_t player, Dir face)
{
  uint16_t x1 = BOARD_X + gx * CELL_SIZE;
  uint16_t y1 = BOARD_Y + gy * CELL_SIZE;
  uint16_t x2 = x1 + CELL_SIZE - 1;
  uint16_t y2 = y1 + CELL_SIZE - 1;
  uint16_t color;
  uint16_t outline;

  if (player == 0) {
    outline = SNAKE_OUTLINE;
    if (is_head) color = SNAKE_HEAD;
    else color = (seg_idx & 1) ? SNAKE_BODY : SNAKE_BODY2;
  } else {
    outline = P2_OUTLINE;
    if (is_head) color = P2_HEAD;
    else color = (seg_idx & 1) ? P2_BODY : P2_BODY2;
  }

  restore_board_cell(gx, gy);
  LCD_Fill(x1 + 1, y1 + 1, x2 - 1, y2 - 1, outline);
  LCD_Fill(x1 + 2, y1 + 2, x2 - 2, y2 - 2, color);
  if (is_head) {
    uint16_t eye = NEON_VOID;
    if (face == DIR_UP) {
      LCD_Fill(x1 + 3, y1 + 2, x1 + 4, y1 + 3, eye);
      LCD_Fill(x1 + 6, y1 + 2, x1 + 7, y1 + 3, eye);
    } else if (face == DIR_DOWN) {
      LCD_Fill(x1 + 3, y1 + 6, x1 + 4, y1 + 7, eye);
      LCD_Fill(x1 + 6, y1 + 6, x1 + 7, y1 + 7, eye);
    } else if (face == DIR_LEFT) {
      LCD_Fill(x1 + 2, y1 + 3, x1 + 3, y1 + 4, eye);
      LCD_Fill(x1 + 2, y1 + 6, x1 + 3, y1 + 7, eye);
    } else {
      LCD_Fill(x1 + 6, y1 + 3, x1 + 7, y1 + 4, eye);
      LCD_Fill(x1 + 6, y1 + 6, x1 + 7, y1 + 7, eye);
    }
  }
}

static void draw_snake_cell(uint8_t gx, uint8_t gy, uint8_t is_head, uint8_t seg_idx)
{
  draw_snake_cell_p(gx, gy, is_head, seg_idx, 0, dir_now);
}

static void draw_food_at(uint8_t gx, uint8_t gy)
{
  uint16_t x1 = BOARD_X + gx * CELL_SIZE;
  uint16_t y1 = BOARD_Y + gy * CELL_SIZE;
  uint16_t x2 = x1 + CELL_SIZE - 1;
  uint16_t y2 = y1 + CELL_SIZE - 1;
  uint16_t inner = g_food_phase ? FOOD_DOT : NEON_YELLOW;
  uint16_t ring = g_food_phase ? FOOD_GLOW : FOOD_MAIN;

  restore_board_cell(gx, gy);
  LCD_Fill(x1 + 1, y1 + 1, x2 - 1, y2 - 1, ring);
  LCD_Fill(x1 + 2, y1 + 2, x2 - 2, y2 - 2, FOOD_MAIN);
  LCD_Fill(x1 + 3, y1 + 3, x2 - 3, y2 - 3, inner);
}

static void draw_board_frame(void)
{
  uint16_t x2 = BOARD_X + BOARD_W * CELL_SIZE;
  uint16_t y2 = BOARD_Y + BOARD_H * CELL_SIZE;

  LCD_Fill(BOARD_X, BOARD_Y, x2 - 1, y2 - 1, UI_BG);

  POINT_COLOR = NEON_MAG;
  LCD_DrawRectangle(BOARD_X - 1, BOARD_Y - 1, x2 + 1, y2 + 1);
  POINT_COLOR = NEON_CYAN;
  LCD_DrawRectangle(BOARD_X - 2, BOARD_Y - 2, x2 + 2, y2 + 2);
  draw_neon_corners(BOARD_X - 2, BOARD_Y - 2, x2 + 2, y2 + 2);
}

static void clear_game_bottom(void)
{
  LCD_Fill(0, FOOT_Y, 239, 319, NEON_VOID);
}

static void draw_menu_footer(void)
{
  LCD_Fill(0, FOOT_Y, 239, 319, UI_PANEL);
  POINT_COLOR = NEON_MAG;
  LCD_DrawLine(0, FOOT_Y, 239, FOOT_Y);
  POINT_COLOR = NEON_CYAN;
  LCD_DrawLine(0, FOOT_Y + 1, 239, FOOT_Y + 1);
  Show_Str(8, FOOT_Y + 8, NEON_CYAN, UI_PANEL, (u8*)"K1/K2 or W/S: SELECT", 16, 0);
  Show_Str(8, FOOT_Y + 28, NEON_PINK, UI_PANEL, (u8*)"K3/K4 or J: CHANGE/OK", 16, 0);
}

static void draw_hud_static(void)
{
  LCD_Fill(0, 0, 239, HUD_H - 1, UI_PANEL);
  POINT_COLOR = NEON_MAG;
  LCD_DrawLine(0, HUD_H, 239, HUD_H);
  POINT_COLOR = NEON_CYAN;
  LCD_DrawLine(0, HUD_H - 1, 239, HUD_H - 1);

  Show_Str(2, 4, NEON_CYAN, UI_PANEL, (u8*)"SCORE", 16, 0);
  Show_Str(98, 4, NEON_MAG, UI_PANEL, (u8*)"LV", 16, 0);
  Show_Str(150, 4, NEON_PINK, UI_PANEL, (u8*)"BEST", 16, 0);
}

static void update_hud_nums(void)
{
  uint8_t prog;
  BACK_COLOR = UI_PANEL;
  POINT_COLOR = NEON_YELLOW;

  LCD_Fill(48, 4, 94, 20, UI_PANEL);
  LCD_Fill(118, 4, 142, 20, UI_PANEL);
  LCD_Fill(188, 4, 234, 20, UI_PANEL);

  LCD_ShowNum(50, 4, g_score, 4, 16);
  LCD_ShowNum(122, 4, g_level, 2, 16);
  LCD_ShowNum(190, 4, g_best, 4, 16);

  prog = (uint8_t)((g_score % 100) * 70 / 100);
  LCD_Fill(2, 20, 72, 22, UI_PANEL);
  if (prog > 0) {
    LCD_Fill(2, 20, 2 + prog, 22, NEON_MAG);
    if (prog > 4) LCD_Fill(2, 20, 2 + prog - 3, 22, NEON_CYAN);
  }
}

static void draw_hud_toast(void)
{
  if (g_score_flash > 0) {
    Show_Str(52, 4, NEON_YELLOW, UI_PANEL, (u8*)"+10", 16, 0);
  }
}

static void draw_dialog_frame(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  LCD_Fill(x1, y1, x2, y2, UI_PANEL);
  POINT_COLOR = NEON_MAG;
  LCD_DrawRectangle(x1, y1, x2, y2);
  POINT_COLOR = NEON_CYAN;
  LCD_DrawRectangle(x1 + 2, y1 + 2, x2 - 2, y2 - 2);
  draw_neon_corners(x1, y1, x2, y2);
}

static void draw_level_up_banner(void)
{
  if (g_level_flash == 0) return;

  draw_dialog_frame(40, 118, 200, 158);
  Show_Str(52, 128, NEON_MAG, UI_PANEL, (u8*)"LEVEL UP!", 24, 0);
}

static void update_hud(void)
{
  update_hud_nums();
}

static void dim_board(void)
{
  uint16_t x2 = BOARD_X + BOARD_W * CELL_SIZE;
  uint16_t y2 = BOARD_Y + BOARD_H * CELL_SIZE;
  LCD_Fill(BOARD_X, BOARD_Y, x2 - 1, y2 - 1, NEON_DIM);
}

static uint8_t snake_contains(uint8_t x, uint8_t y)
{
  uint16_t i;
  for (i = 0; i < snake_len; i++) {
    if (snake[i].x == x && snake[i].y == y) return 1;
  }
  return 0;
}

static uint8_t snake_hits_body(uint8_t x, uint8_t y, uint8_t ignore_tail)
{
  uint16_t i;
  uint16_t limit = snake_len;

  if (ignore_tail && snake_len > 0) limit--;
  for (i = 0; i < limit; i++) {
    if (snake[i].x == x && snake[i].y == y) return 1;
  }
  return 0;
}

static void draw_demo_tag(void)
{
  if (!g_demo_mode) return;
  LCD_Fill(0, 26, 86, 42, NEON_VOID);
  Show_Str(0, 26, NEON_LIME, NEON_VOID, (u8*)"[AI DEMO]", 16, 0);
}

static void demo_vis_clear(void)
{
  uint8_t y, x;

  for (y = 0; y < BOARD_H; y++) {
    for (x = 0; x < BOARD_W; x++) {
      demo_vis[y][x] = 0;
    }
  }
}

static int demo_manhattan(uint8_t x, uint8_t y)
{
  int dx = (int)food.x - (int)x;
  int dy = (int)food.y - (int)y;

  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  return dx + dy;
}

static uint8_t demo_cell_safe(uint8_t x, uint8_t y, uint8_t will_eat)
{
  if (x >= BOARD_W || y >= BOARD_H) return 0;
  return !snake_hits_body(x, y, will_eat ? 0 : 1);
}

static uint8_t demo_is_reverse(Dir d)
{
  if (d == DIR_UP && dir_now == DIR_DOWN) return 1;
  if (d == DIR_DOWN && dir_now == DIR_UP) return 1;
  if (d == DIR_LEFT && dir_now == DIR_RIGHT) return 1;
  if (d == DIR_RIGHT && dir_now == DIR_LEFT) return 1;
  return 0;
}

static uint8_t demo_move_info(Dir d, uint8_t *nx, uint8_t *ny, uint8_t *will_eat)
{
  Point p = snake[0];

  if (demo_is_reverse(d)) return 0;

  if (d == DIR_UP) p.y--;
  else if (d == DIR_DOWN) p.y++;
  else if (d == DIR_LEFT) p.x--;
  else p.x++;

  *will_eat = (uint8_t)(p.x == food.x && p.y == food.y);
  if (!demo_cell_safe(p.x, p.y, *will_eat)) return 0;

  *nx = p.x;
  *ny = p.y;
  return 1;
}

static uint16_t demo_flood_fill(uint8_t sx, uint8_t sy, uint8_t ignore_tail)
{
  uint16_t head = 0;
  uint16_t tail = 0;
  uint16_t count = 0;
  static const int8_t ddx[4] = {0, 0, -1, 1};
  static const int8_t ddy[4] = {-1, 1, 0, 0};

  demo_vis_clear();
  demo_qx[tail] = sx;
  demo_qy[tail] = sy;
  tail++;
  demo_vis[sy][sx] = 1;
  count = 1;

  while (head < tail) {
    uint8_t x = demo_qx[head];
    uint8_t y = demo_qy[head];
    uint8_t i;

    head++;
    for (i = 0; i < 4; i++) {
      int nx = (int)x + ddx[i];
      int ny = (int)y + ddy[i];

      if (nx < 0 || ny < 0 || nx >= BOARD_W || ny >= BOARD_H) continue;
      if (demo_vis[ny][nx]) continue;
      if (snake_hits_body((uint8_t)nx, (uint8_t)ny, ignore_tail)) continue;

      demo_vis[ny][nx] = 1;
      demo_qx[tail] = (uint8_t)nx;
      demo_qy[tail] = (uint8_t)ny;
      tail++;
      count++;
    }
  }

  return count;
}

static uint8_t demo_survives(uint8_t nx, uint8_t ny, uint8_t will_eat)
{
  uint16_t space = demo_flood_fill(nx, ny, will_eat ? 0 : 1);
  uint16_t need = snake_len;

  if (will_eat) need++;
  return space >= need;
}

static uint8_t demo_bfs_food_dir(Dir *out_dir)
{
  uint16_t head_q = 0;
  uint16_t tail_q = 0;
  uint8_t hx = snake[0].x;
  uint8_t hy = snake[0].y;
  uint8_t y, x;
  static const int8_t ddx[4] = {0, 0, -1, 1};
  static const int8_t ddy[4] = {-1, 1, 0, 0};
  static const Dir ddir[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};

  demo_vis_clear();
  for (y = 0; y < BOARD_H; y++) {
    for (x = 0; x < BOARD_W; x++) {
      demo_first[y][x] = 255;
    }
  }

  demo_qx[tail_q] = hx;
  demo_qy[tail_q] = hy;
  tail_q++;
  demo_vis[hy][hx] = 1;

  while (head_q < tail_q) {
    uint8_t cx = demo_qx[head_q];
    uint8_t cy = demo_qy[head_q];
    uint8_t fs = demo_first[cy][cx];
    uint8_t i;

    head_q++;
    for (i = 0; i < 4; i++) {
      int nx = (int)cx + ddx[i];
      int ny = (int)cy + ddy[i];
      uint8_t nd;

      if (nx < 0 || ny < 0 || nx >= BOARD_W || ny >= BOARD_H) continue;
      if (demo_vis[ny][nx]) continue;
      if (snake_hits_body((uint8_t)nx, (uint8_t)ny, 1)) continue;

      nd = (cx == hx && cy == hy) ? (uint8_t)ddir[i] : fs;
      if ((uint8_t)nx == food.x && (uint8_t)ny == food.y) {
        *out_dir = (Dir)nd;
        return 1;
      }

      demo_vis[ny][nx] = 1;
      demo_first[ny][nx] = nd;
      demo_qx[tail_q] = (uint8_t)nx;
      demo_qy[tail_q] = (uint8_t)ny;
      tail_q++;
    }
  }

  return 0;
}

static uint8_t demo_try_dir(Dir d)
{
  uint8_t nx, ny, eat;

  if (!demo_move_info(d, &nx, &ny, &eat)) return 0;
  dir_next = d;
  return 1;
}

static void demo_ai_pick_dir(void)
{
  Dir food_dir;
  Dir best_d = dir_now;
  uint8_t nx, ny, eat, i;
  uint8_t found = 0;
  uint16_t best_space = 0;
  int best_dist = 1000;

  if (demo_bfs_food_dir(&food_dir)) {
    if (demo_move_info(food_dir, &nx, &ny, &eat) && demo_survives(nx, ny, eat)) {
      dir_next = food_dir;
      return;
    }
  }

  for (i = 0; i < 4; i++) {
    Dir d = (Dir)i;
    uint16_t space;
    int dist;

    if (!demo_move_info(d, &nx, &ny, &eat)) continue;
    if (!demo_survives(nx, ny, eat)) continue;

    space = demo_flood_fill(nx, ny, eat ? 0 : 1);
    dist = demo_manhattan(nx, ny);
    if (!found || space > best_space || (space == best_space && dist < best_dist)) {
      found = 1;
      best_space = space;
      best_dist = dist;
      best_d = d;
    }
  }

  if (found) {
    dir_next = best_d;
    return;
  }

  for (i = 0; i < 4; i++) {
    if (demo_try_dir((Dir)i)) return;
  }
}

static uint8_t snake2_contains(uint8_t x, uint8_t y)
{
  uint16_t i;
  for (i = 0; i < snake2_len; i++) {
    if (snake2[i].x == x && snake2[i].y == y) return 1;
  }
  return 0;
}

static uint8_t versus_on_snake(const Point *s, uint16_t len, uint8_t x, uint8_t y, uint8_t eating)
{
  uint16_t i;
  uint16_t limit = len;

  if (!eating && len > 0) limit--;
  for (i = 0; i < limit; i++) {
    if (s[i].x == x && s[i].y == y) return 1;
  }
  return 0;
}

static Point versus_move(Point p, Dir d)
{
  if (d == DIR_UP) p.y--;
  else if (d == DIR_DOWN) p.y++;
  else if (d == DIR_LEFT) p.x--;
  else p.x++;
  return p;
}

static void draw_hud_versus_static(void)
{
  LCD_Fill(0, 0, 239, HUD_H - 1, UI_PANEL);
  POINT_COLOR = NEON_MAG;
  LCD_DrawLine(0, HUD_H, 239, HUD_H);
  POINT_COLOR = NEON_CYAN;
  LCD_DrawLine(0, HUD_H - 1, 239, HUD_H - 1);
  Show_Str(2, 4, NEON_CYAN, UI_PANEL, (u8*)"P1", 16, 0);
  Show_Str(118, 4, NEON_PINK, UI_PANEL, (u8*)"P2", 16, 0);
}

static void update_hud_versus(void)
{
  BACK_COLOR = UI_PANEL;
  POINT_COLOR = NEON_YELLOW;
  LCD_Fill(28, 4, 60, 20, UI_PANEL);
  LCD_Fill(144, 4, 176, 20, UI_PANEL);
  LCD_ShowNum(30, 4, snake_len, 3, 16);
  LCD_ShowNum(146, 4, snake2_len, 3, 16);
}

static void spawn_food_versus(void)
{
  uint8_t x, y;
  uint16_t tries = 0;

  do {
    x = rng_range(BOARD_W);
    y = rng_range(BOARD_H);
    tries++;
  } while ((snake_contains(x, y) || snake2_contains(x, y)) && tries < 600);

  food.x = x;
  food.y = y;
  draw_food_at(food.x, food.y);
}

static void versus_redraw_view(void)
{
  uint16_t i;

  draw_hud_versus_static();
  update_hud_versus();
  draw_board_frame();
  clear_game_bottom();

  for (i = 0; i < snake_len; i++) {
    draw_snake_cell_p(snake[i].x, snake[i].y, (uint8_t)(i == 0), (uint8_t)i, 0, dir_now);
  }
  for (i = 0; i < snake2_len; i++) {
    draw_snake_cell_p(snake2[i].x, snake2[i].y, (uint8_t)(i == 0), (uint8_t)i, 1, dir2_now);
  }
  draw_food_at(food.x, food.y);
}

static void versus_reset(void)
{
  uint16_t i;

  LCD_Clear(NEON_VOID);
  draw_hud_versus_static();
  draw_board_frame();
  clear_game_bottom();

  snake_len = 3;
  snake[0].x = 3; snake[0].y = 11;
  snake[1].x = 2; snake[1].y = 11;
  snake[2].x = 1; snake[2].y = 11;
  dir_now = DIR_RIGHT;
  dir_next = DIR_RIGHT;

  snake2_len = 3;
  snake2[0].x = 18; snake2[0].y = 11;
  snake2[1].x = 19; snake2[1].y = 11;
  snake2[2].x = 20; snake2[2].y = 11;
  dir2_now = DIR_LEFT;
  dir2_next = DIR_LEFT;

  for (i = 0; i < snake_len; i++) {
    draw_snake_cell_p(snake[i].x, snake[i].y, (uint8_t)(i == 0), (uint8_t)i, 0, dir_now);
  }
  for (i = 0; i < snake2_len; i++) {
    draw_snake_cell_p(snake2[i].x, snake2[i].y, (uint8_t)(i == 0), (uint8_t)i, 1, dir2_now);
  }

  g_score = 0;
  g_level = 1;
  g_level_flash = 0;
  g_score_flash = 0;
  g_die_reason = DIE_NONE;
  update_hud_versus();
  spawn_food_versus();
  g_pause_drawn = 0;
  tick_cnt = 0;
  move_interval_ticks = 7;
}

static const u8* vs_reason_str(VsReason r)
{
  if (r == VR_WALL) return (u8*)"HIT WALL";
  if (r == VR_SELF) return (u8*)"HIT SELF";
  if (r == VR_RIVAL) return (u8*)"HIT RIVAL";
  if (r == VR_HEAD) return (u8*)"HEAD CRASH";
  return (u8*)"";
}

static void versus_over_screen(VsResult result, VsReason r1, VsReason r2)
{
  g_state = GS_OVER;
  g_pause_drawn = 0;
  bgm_stop();
  dim_board();
  sfx_play_gameover();

  draw_dialog_frame(10, 75, 230, 255);

  if (result == VS_P1_WIN) {
    Show_Str(52, 88, NEON_CYAN, UI_PANEL, (u8*)"P1 WINS!", 24, 0);
    Show_Str(24, 124, NEON_MUTED, UI_PANEL, (u8*)"P2", 16, 0);
    Show_Str(56, 124, NEON_PINK, UI_PANEL, (u8*)vs_reason_str(r2), 16, 0);
  } else if (result == VS_P2_WIN) {
    Show_Str(52, 88, NEON_PINK, UI_PANEL, (u8*)"P2 WINS!", 24, 0);
    Show_Str(24, 124, NEON_MUTED, UI_PANEL, (u8*)"P1", 16, 0);
    Show_Str(56, 124, NEON_CYAN, UI_PANEL, (u8*)vs_reason_str(r1), 16, 0);
  } else {
    Show_Str(62, 88, NEON_YELLOW, UI_PANEL, (u8*)"DRAW!", 24, 0);
    Show_Str(36, 124, NEON_MAG, UI_PANEL, (u8*)"HEAD CRASH", 16, 0);
  }

  draw_stat_line(148, (u8*)"P1 LEN", snake_len, 3);
  draw_stat_line(168, (u8*)"P2 LEN", snake2_len, 3);
  Show_Str(20, 200, NEON_MUTED, UI_PANEL, (u8*)"P1:K1-K4  P2:WASD", 16, 0);
  Show_Str(20, 228, NEON_CYAN, UI_PANEL, (u8*)"J:RETRY  K3/K:MENU", 16, 0);
  uart_log_vs_over(result, r1, r2);
}

static void versus_step(void)
{
  Point new1, new2, tail1, tail2;
  uint8_t eat1, eat2;
  uint8_t p1_die = 0, p2_die = 0;
  VsReason r1 = VR_NONE, r2 = VR_NONE;
  uint16_t i;

  dir_now = dir_next;
  dir2_now = dir2_next;
  new1 = versus_move(snake[0], dir_now);
  new2 = versus_move(snake2[0], dir2_now);
  eat1 = (uint8_t)(new1.x == food.x && new1.y == food.y);
  eat2 = (uint8_t)(new2.x == food.x && new2.y == food.y);

  if (new1.x == new2.x && new1.y == new2.y) {
    versus_over_screen(VS_DRAW, VR_HEAD, VR_HEAD);
    return;
  }

  if (new1.x >= BOARD_W || new1.y >= BOARD_H) { p1_die = 1; r1 = VR_WALL; }
  if (new2.x >= BOARD_W || new2.y >= BOARD_H) { p2_die = 1; r2 = VR_WALL; }

  if (!p1_die && versus_on_snake(snake, snake_len, new1.x, new1.y, eat1)) {
    p1_die = 1; r1 = VR_SELF;
  }
  if (!p2_die && versus_on_snake(snake2, snake2_len, new2.x, new2.y, eat2)) {
    p2_die = 1; r2 = VR_SELF;
  }

  if (!p1_die && versus_on_snake(snake2, snake2_len, new1.x, new1.y, eat2)) {
    p1_die = 1; r1 = VR_RIVAL;
  }
  if (!p2_die && versus_on_snake(snake, snake_len, new2.x, new2.y, eat1)) {
    p2_die = 1; r2 = VR_RIVAL;
  }

  if (p1_die && p2_die) {
    versus_over_screen(VS_DRAW, r1, r2);
    return;
  }
  if (p1_die) {
    versus_over_screen(VS_P2_WIN, r1, r2);
    return;
  }
  if (p2_die) {
    versus_over_screen(VS_P1_WIN, r1, r2);
    return;
  }

  tail1 = snake[snake_len - 1];
  tail2 = snake2[snake2_len - 1];

  for (i = snake_len; i > 0; i--) snake[i] = snake[i - 1];
  snake[0] = new1;
  for (i = snake2_len; i > 0; i--) snake2[i] = snake2[i - 1];
  snake2[0] = new2;

  if (eat1) {
    if (snake_len < MAX_SNAKE_LEN - 1) snake_len++;
  } else {
    draw_cell_clear(tail1.x, tail1.y);
  }

  if (eat2) {
    if (snake2_len < MAX_SNAKE_LEN - 1) snake2_len++;
  } else {
    draw_cell_clear(tail2.x, tail2.y);
  }

  if (eat1 || eat2) {
    sfx_play_eat();
    spawn_food_versus();
    update_hud_versus();
  }

  draw_snake_cell_p(new1.x, new1.y, 1, 0, 0, dir_now);
  if (snake_len > 1) {
    draw_snake_cell_p(snake[1].x, snake[1].y, 0, 1, 0, dir_now);
  }
  draw_snake_cell_p(new2.x, new2.y, 1, 0, 1, dir2_now);
  if (snake2_len > 1) {
    draw_snake_cell_p(snake2[1].x, snake2[1].y, 0, 1, 1, dir2_now);
  }
}

static void spawn_food(void)
{
  uint8_t x, y;
  uint16_t tries = 0;
  uint16_t max_tries = (uint16_t)(BOARD_W * BOARD_H);

  do {
    x = rng_range(BOARD_W);
    y = rng_range(BOARD_H);
    tries++;
  } while (snake_contains(x, y) && tries < max_tries);

  food.x = x;
  food.y = y;
  draw_food_at(food.x, food.y);
}

static void update_speed_by_level(void)
{
  uint8_t base;
  if (g_diff == DIF_EASY) base = 9;
  else if (g_diff == DIF_NORMAL) base = 7;
  else base = 5;

  if (g_level > 1) {
    uint8_t dec = (uint8_t)(g_level - 1);
    if (base > dec + 2) base = (uint8_t)(base - dec);
    else base = 2;
  }
  move_interval_ticks = base;
}

static void snake_redraw_view(void)
{
  uint16_t i;

  draw_hud_static();
  update_hud_nums();
  draw_board_frame();
  clear_game_bottom();

  for (i = 0; i < snake_len; i++) {
    draw_snake_cell(snake[i].x, snake[i].y, (uint8_t)(i == 0), (uint8_t)i);
  }
  draw_food_at(food.x, food.y);
  draw_demo_tag();
}

static void snake_reset(void)
{
  uint16_t i;

  LCD_Clear(NEON_VOID);
  draw_hud_static();
  draw_board_frame();
  clear_game_bottom();

  snake_len = 3;
  snake[0].x = 5;  snake[0].y = 10;
  snake[1].x = 4;  snake[1].y = 10;
  snake[2].x = 3;  snake[2].y = 10;
  dir_now = DIR_RIGHT;
  dir_next = DIR_RIGHT;

  draw_snake_cell(snake[0].x, snake[0].y, 1, 0);
  for (i = 1; i < snake_len; i++) {
    draw_snake_cell(snake[i].x, snake[i].y, 0, (uint8_t)i);
  }

  g_score = 0;
  g_level = 1;
  g_level_flash = 0;
  g_score_flash = 0;
  g_die_reason = DIE_NONE;
  update_speed_by_level();
  update_hud();
  spawn_food();
  draw_demo_tag();
  g_pause_drawn = 0;
  tick_cnt = 0;
}

static void bgm_reset(void)
{
  g_bgm_idx = 0;
  g_bgm_tick = BGM_NOTE_TICKS;
}

static void bgm_advance_note(void)
{
  g_bgm_idx++;
  if (g_bgm_idx >= Mario_FullLen) {
    g_bgm_idx = MARIO_LOOP_START;
  }
}

static void bgm_play_note(uint16_t freq)
{
  if (freq == 0) {
    TIM_Cmd(TIM3, DISABLE);
    return;
  }

  TIM_Configuration(freq);
  DAC_Cmd(DAC_Channel_2, ENABLE);
  DAC_DMACmd(DAC_Channel_2, ENABLE);
  DMA_Cmd(DMA2_Channel4, ENABLE);
}

static void audio_hw_init(void)
{
  g_audio_vol = pot_read_volume_pct();
  audio_rebuild_wavetable();
  DMA_Configuration();
  TIM_Configuration(Mario_Full[0]);
  DAC_Configuration();
}

static void bgm_stop(void)
{
  TIM_Cmd(TIM3, DISABLE);
}

static void sfx_play_eat(void)
{
  uint8_t resume_bgm = (uint8_t)(g_music_on && g_state == GS_RUN);

  bgm_play_note(880);
  Delay_ms(40);
  if (resume_bgm) {
    bgm_play_note(Mario_Full[g_bgm_idx]);
  } else {
    bgm_stop();
  }
}

static void sfx_play_gameover(void)
{
  uint8_t i;

  for (i = 0; i < AdventureDieLen; i++) {
    bgm_play_note(Adventure_Die[i]);
    Delay_ms(ADVENTURE_DIE_MS);
  }
  bgm_stop();
}

static void play_bgm_step(void)
{
  if (!g_music_on || g_state != GS_RUN || g_demo_mode || g_versus_mode) {
    bgm_stop();
    return;
  }

  g_bgm_tick++;
  if (g_bgm_tick < BGM_NOTE_TICKS) return;
  g_bgm_tick = 0;

  bgm_play_note(Mario_Full[g_bgm_idx]);
  bgm_advance_note();
}

static void game_over_screen(void)
{
  g_state = GS_OVER;
  g_pause_drawn = 0;
  bgm_stop();
  dim_board();
  sfx_play_gameover();

  if (g_demo_mode) {
    g_demo_restart_wait = 45;
    draw_dialog_frame(10, 100, 230, 200);
    Show_Str(48, 118, NEON_MAG, UI_PANEL, (u8*)"DEMO OVER", 24, 0);
    Show_Str(36, 158, NEON_CYAN, UI_PANEL, (u8*)"RESTARTING...", 16, 0);
    draw_stat_line(178, (u8*)"SCORE", g_score, 4);
    uart_log_demo_over();
    return;
  }

  if (g_versus_mode) return;

  draw_dialog_frame(10, 75, 230, 255);

  Show_Str(58, 88, NEON_MAG, UI_PANEL, (u8*)"GAME OVER", 24, 0);
  if (g_score > 0 && g_score >= g_best) {
    Show_Str(56, 118, NEON_CYAN, UI_PANEL, (u8*)"NEW RECORD!", 16, 0);
  } else if (g_die_reason == DIE_WALL) {
    Show_Str(68, 118, NEON_MUTED, UI_PANEL, (u8*)"HIT WALL", 16, 0);
  } else if (g_die_reason == DIE_BODY) {
    Show_Str(62, 118, NEON_MUTED, UI_PANEL, (u8*)"HIT BODY", 16, 0);
  }

  draw_stat_line(142, (u8*)"SCORE", g_score, 4);
  draw_stat_line(162, (u8*)"LEVEL", g_level, 2);
  draw_stat_line(182, (u8*)"LEN", (uint16_t)snake_len, 3);
  draw_stat_line(202, (u8*)"BEST", g_best, 4);

  Show_Str(20, 228, NEON_CYAN, UI_PANEL, (u8*)"J:RETRY  K3/K:MENU", 16, 0);
  uart_log_game_over();
}

static void show_pause_dialog(void)
{
  if (g_pause_drawn) return;

  dim_board();
  draw_dialog_frame(10, 75, 230, 255);

  Show_Str(78, 88, NEON_MAG, UI_PANEL, (u8*)"PAUSED", 24, 0);

  if (g_versus_mode) {
    draw_stat_line(126, (u8*)"P1 LEN", snake_len, 3);
    draw_stat_line(146, (u8*)"P2 LEN", snake2_len, 3);
    Show_Str(20, 176, NEON_MUTED, UI_PANEL, (u8*)"P1:K1-K4 P2:WASD", 16, 0);
  } else {
    draw_stat_line(126, (u8*)"SCORE", g_score, 4);
    draw_stat_line(146, (u8*)"LEVEL", g_level, 2);
    draw_stat_line(166, (u8*)"LEN", (uint16_t)snake_len, 3);
    Show_Str(36, 186, NEON_CYAN, UI_PANEL, (u8*)"MODE", 16, 0);
    Show_Str(110, 186, NEON_LIME, UI_PANEL, (u8*)diff_name(), 16, 0);
  }

  Show_Str(20, 210, NEON_CYAN, UI_PANEL, (u8*)"J / K4HOLD: CONTINUE", 16, 0);
  Show_Str(20, 232, NEON_PINK, UI_PANEL, (u8*)"K3/K: BACK TO MENU", 16, 0);
  g_pause_drawn = 1;
}

static void pause_enter(void)
{
  g_state = GS_PAUSE;
  g_pause_drawn = 0;
  bgm_stop();
  show_pause_dialog();
}

static void pause_resume(void)
{
  g_state = GS_RUN;
  g_pause_drawn = 0;
  key4_reset_hold();
  g_pause_block_ticks = 30;
  g_input_block_ticks = 20;
  if (g_versus_mode) versus_redraw_view();
  else snake_redraw_view();
  if (g_music_on && !g_demo_mode && !g_versus_mode) bgm_play_note(Mario_Full[g_bgm_idx]);
}

static void snake_step(void)
{
  Point head = snake[0];
  Point newh;
  Point old_tail = snake[snake_len - 1];
  uint16_t i;
  uint8_t eating;

  dir_now = dir_next;
  newh = head;

  if (dir_now == DIR_UP) newh.y--;
  else if (dir_now == DIR_DOWN) newh.y++;
  else if (dir_now == DIR_LEFT) newh.x--;
  else newh.x++;

  eating = (uint8_t)(newh.x == food.x && newh.y == food.y);

  if (newh.x >= BOARD_W || newh.y >= BOARD_H) {
    g_die_reason = DIE_WALL;
    game_over_screen();
    return;
  }
  if (!eating && snake_hits_body(newh.x, newh.y, 1)) {
    g_die_reason = DIE_BODY;
    game_over_screen();
    return;
  }

  for (i = snake_len; i > 0; i--) snake[i] = snake[i - 1];
  snake[0] = newh;

  if (eating) {
    uint8_t old_level = g_level;

    if (snake_len < MAX_SNAKE_LEN - 1) snake_len++;
    g_score += 10;
    if (!g_demo_mode && g_score > g_best) {
      g_best = g_score;
      save_store_best();
    }
    if ((g_score % 100) == 0 && g_level < 20) g_level++;
    if (g_level > old_level) {
      g_level_flash = 35;
      uart_log_level();
    }
    g_score_flash = 20;
    update_speed_by_level();
    update_hud();
    draw_hud_toast();
    sfx_play_eat();
    uart_log_score();
    spawn_food();
  } else {
    draw_cell_clear(old_tail.x, old_tail.y);
  }

  draw_snake_cell(newh.x, newh.y, 1, 0);
  if (snake_len > 1) {
    draw_snake_cell(snake[1].x, snake[1].y, 0, 1);
  }
}

static uint8_t key_scan_click(void)
{
  uint8_t k = 0;
  if (KEY_UP_PRESSED()) k = 1;
  else if (KEY_DOWN_PRESSED()) k = 2;
  else if (KEY_LEFT_PRESSED()) k = 3;
  else if (KEY_RIGHT_PRESSED()) k = 4;

  if (k && !g_key_click_lock) {
    Delay_ms(12);
    g_key_click_lock = 1;
    return k;
  }
  if (!k) g_key_click_lock = 0;
  return 0;
}

static void key_wait_release(void)
{
  while (KEY_UP_PRESSED() || KEY_DOWN_PRESSED() ||
         KEY_LEFT_PRESSED() || KEY_RIGHT_PRESSED()) {
    rng_stir_once();
    Delay_ms(10);
  }
  g_key_click_lock = 0;
}

static void key4_reset_hold(void)
{
  g_key4_hold_ticks = 0;
}

static uint8_t key4_long_pressed(void)
{
  if (g_pause_block_ticks > 0) {
    g_pause_block_ticks--;
    g_key4_hold_ticks = 0;
    return 0;
  }

  if (KEY_RIGHT_PRESSED()) {
    if (g_key4_hold_ticks < 255) g_key4_hold_ticks++;
    if (g_key4_hold_ticks >= 25) {
      g_key4_hold_ticks = 0;
      return 1;
    }
  } else {
    g_key4_hold_ticks = 0;
  }
  return 0;
}

static void countdown_before_start(void)
{
  const u8 *labels[4] = {(u8*)"3", (u8*)"2", (u8*)"1", (u8*)"GO"};
  const uint16_t colors[4] = {NEON_CYAN, NEON_MAG, NEON_PINK, NEON_YELLOW};
  uint8_t i;

  for (i = 0; i < 4; i++) {
    LCD_Fill(95, 120, 145, 165, UI_BG);
    Show_Str(108, 128, colors[i], UI_BG, (u8*)labels[i], 32, 0);
    Delay_ms(i < 3 ? 450 : 280);
  }
  snake_redraw_view();
}

static void enter_play_mode(uint8_t demo)
{
  key4_reset_hold();
  key_wait_release();

  g_demo_mode = demo;
  g_versus_mode = 0;
  g_demo_restart_wait = 0;
  g_state = GS_RUN;
  g_pause_block_ticks = 60;
  g_input_block_ticks = demo ? 10 : 50;

  rng_reseed();
  bgm_reset();
  snake_reset();

  if (demo) {
    Show_Str(60, 120, NEON_MAG, UI_BG, (u8*)"AI DEMO", 32, 0);
    Delay_ms(600);
    snake_redraw_view();
  } else {
    countdown_before_start();
  }

  if (g_music_on && !demo) {
    bgm_play_note(Mario_Full[0]);
  }
  uart_log_start_solo(demo);
}

static void enter_run_mode(void)
{
  enter_play_mode(0);
}

static void enter_demo_mode(void)
{
  enter_play_mode(1);
}

static void enter_versus_mode(void)
{
  key4_reset_hold();
  key_wait_release();

  g_demo_mode = 0;
  g_versus_mode = 1;
  g_demo_restart_wait = 0;
  g_state = GS_RUN;
  g_pause_block_ticks = 60;
  g_input_block_ticks = 40;

  rng_reseed();
  bgm_reset();
  versus_reset();

  Show_Str(36, 120, NEON_CYAN, UI_BG, (u8*)"2P FIGHT!", 32, 0);
  Delay_ms(700);
  versus_redraw_view();
  uart_log_start_vs();
}

static void show_boot(void)
{
  LCD_Clear(NEON_VOID);
  draw_neon_scanlines(0, 319);
  draw_dialog_frame(10, 55, 230, 235);
  Show_Str(30, 68, NEON_CYAN, UI_PANEL, (u8*)"CYBER", 32, 0);
  Show_Str(118, 68, NEON_MAG, UI_PANEL, (u8*)"SNAKE", 32, 0);
  Show_Str(38, 118, NEON_PINK, UI_PANEL, (u8*)"NUAA CM3-107", 16, 0);
  Show_Str(24, 142, NEON_MUTED, UI_PANEL, (u8*)"K1-K4 / UART WASDJ", 16, 0);
  Show_Str(24, 164, NEON_MUTED, UI_PANEL, (u8*)"WASD MOVE  J OK/PAUSE", 16, 0);
  Show_Str(30, 200, NEON_YELLOW, UI_PANEL, (u8*)"PRESS ANY KEY", 16, 0);
}

static void menu_draw_item(uint8_t idx, uint8_t selected)
{
  uint16_t y = MENU_ITEM_Y0 + idx * MENU_ITEM_STEP;
  uint16_t bg = selected ? NEON_SEL_BG : UI_PANEL;
  uint16_t fg = selected ? NEON_SEL_FG : NEON_YELLOW;
  const u8 *labels[5] = {
    (u8*)"> START",
    (u8*)"> DIFFICULTY",
    (u8*)"> MUSIC",
    (u8*)"> DEMO",
    (u8*)"> 2P VS"
  };

  if (idx >= MENU_ITEM_CNT) return;

  LCD_Fill(20, y - 2, 158, y + 16, bg);
  if (selected) {
    LCD_Fill(20, y - 2, 23, y + 16, NEON_CYAN);
  }
  Show_Str(30, y, fg, bg, (u8*)labels[idx], 16, 0);
}

static void menu_draw_diff_value(void)
{
  LCD_Fill(165, 83, 230, 99, UI_PANEL);
  if (g_diff == DIF_EASY) Show_Str(170, 85, NEON_LIME, UI_PANEL, (u8*)"EASY", 16, 0);
  else if (g_diff == DIF_NORMAL) Show_Str(170, 85, NEON_CYAN, UI_PANEL, (u8*)"NORMAL", 16, 0);
  else Show_Str(170, 85, NEON_MAG, UI_PANEL, (u8*)"HARD", 16, 0);
}

static void menu_draw_music_value(void)
{
  LCD_Fill(165, 102, 230, 118, UI_PANEL);
  Show_Str(170, 104, g_music_on ? NEON_CYAN : NEON_MUTED, UI_PANEL,
           g_music_on ? (u8*)"ON" : (u8*)"OFF", 16, 0);
}

static void menu_draw_demo_value(void)
{
  LCD_Fill(165, 121, 230, 137, UI_PANEL);
  Show_Str(170, 123, NEON_PINK, UI_PANEL, (u8*)"AUTO", 16, 0);
}

static void menu_draw_vs_value(void)
{
  LCD_Fill(165, 140, 230, 156, UI_PANEL);
  Show_Str(170, 142, NEON_MAG, UI_PANEL, (u8*)"P1+P2", 16, 0);
}

static void menu_redraw_value(uint8_t idx)
{
  if (idx == 1) menu_draw_diff_value();
  else if (idx == 2) menu_draw_music_value();
  else if (idx == 3) menu_draw_demo_value();
  else if (idx == 4) menu_draw_vs_value();
}

static void menu_draw_best_value(void)
{
  BACK_COLOR = UI_PANEL;
  POINT_COLOR = NEON_YELLOW;
  LCD_Fill(30, 165, 120, 181, UI_PANEL);
  Show_Str(30, 165, NEON_MAG, UI_PANEL, (u8*)"BEST", 16, 0);
  LCD_ShowNum(80, 165, g_best, 4, 16);
}

static void menu_draw_pot_volume(void)
{
  LCD_Fill(125, 165, 215, 181, UI_PANEL);
  Show_Str(125, 165, NEON_CYAN, UI_PANEL, (u8*)"VOL", 16, 0);
  POINT_COLOR = NEON_YELLOW;
  BACK_COLOR = UI_PANEL;
  LCD_ShowNum(160, 165, g_audio_vol, 3, 16);
  Show_Str(190, 165, NEON_MUTED, UI_PANEL, (u8*)"%", 16, 0);
}

static void show_menu_init(uint8_t sel)
{
  LCD_Clear(NEON_VOID);
  draw_neon_scanlines(0, 54);

  Show_Str(24, 20, NEON_CYAN, NEON_VOID, (u8*)"CYBER MENU", 16, 0);
  Show_Str(168, 20, NEON_MAG, NEON_VOID, (u8*)"2077", 16, 0);

  LCD_Fill(15, 55, 225, 198, UI_PANEL);
  POINT_COLOR = NEON_MAG;
  LCD_DrawRectangle(15, 55, 225, 198);
  POINT_COLOR = NEON_CYAN;
  LCD_DrawRectangle(16, 56, 224, 197);
  draw_neon_corners(15, 55, 225, 198);

  menu_draw_item(0, sel == 0);
  menu_draw_item(1, sel == 1);
  menu_draw_item(2, sel == 2);
  menu_draw_item(3, sel == 3);
  menu_draw_item(4, sel == 4);
  menu_draw_diff_value();
  menu_draw_music_value();
  menu_draw_demo_value();
  menu_draw_vs_value();
  menu_draw_best_value();
  menu_draw_pot_volume();
  draw_menu_footer();
}

static void menu_update_selection(uint8_t old_sel, uint8_t new_sel)
{
  if (old_sel < MENU_ITEM_CNT) menu_draw_item(old_sel, 0);
  if (new_sel < MENU_ITEM_CNT) menu_draw_item(new_sel, 1);
  menu_redraw_value(old_sel);
  menu_redraw_value(new_sel);
}

static void menu_leave(void)
{
}

static void show_menu(uint8_t sel)
{
  show_menu_init(sel);
}

int main(void)
{
  uint8_t key;
  uint8_t menu_sel = 0;

  SystemInit();
  Delay_Init();
  GPIO_Configuration();
  ADC_Mode_Config();
  Delay_ms(20);
  USART1_Configuration();
  DAC_Configuration();
  LCD_Init();
  audio_hw_init();
  bgm_stop();
  save_load_best();
  usart1_send_str("Snake ready. WASD=move, J=OK/pause, 115200\r\n");
  usart1_send_str("2P VS: menu item 5, P1=K1-K4(green) P2=WASD(blue)\r\n");
  usart1_send_str("UART OUT: [START] [SCORE] [LEVEL] [OVER] [VS_OVER]\r\n");
  usart1_send_str("POT on PA3/ADC3 = audio volume 0-100%\r\n");

  g_state = GS_BOOT;
  show_boot();

  while (1) {
    rng_stir_once();
    if (audio_volume_service()) {
      if (g_state == GS_MENU) {
        menu_draw_pot_volume();
      }
    }
    if (g_uart_j_cd > 0) g_uart_j_cd--;
    uart_service();
    key = poll_key();

    if (g_state == GS_BOOT) {
      if (key) {
        g_state = GS_MENU;
        show_menu(menu_sel);
      }
    }
    else if (g_state == GS_MENU) {
      uint8_t act = key;
      if (act == KEY_CONFIRM) act = 4;

      if (act == 1) {
        uint8_t old_sel = menu_sel;
        menu_sel = (uint8_t)((menu_sel + MENU_ITEM_CNT - 1) % MENU_ITEM_CNT);
        menu_update_selection(old_sel, menu_sel);
      } else if (act == 2) {
        uint8_t old_sel = menu_sel;
        menu_sel = (uint8_t)((menu_sel + 1) % MENU_ITEM_CNT);
        menu_update_selection(old_sel, menu_sel);
      } else if (act == 3 || act == 4) {
        if (menu_sel == 0 && act == 4) {
          enter_run_mode();
        } else if (menu_sel == 1) {
          if (act == 4) g_diff = (Difficulty)((g_diff + 1) % 3);
          else g_diff = (Difficulty)((g_diff + 2) % 3);
          menu_draw_diff_value();
        } else if (menu_sel == 2) {
          g_music_on = !g_music_on;
          if (!g_music_on) bgm_stop();
          menu_draw_music_value();
        } else if (menu_sel == 3 && act == 4) {
          enter_demo_mode();
        } else if (menu_sel == 4 && act == 4) {
          enter_versus_mode();
        }
      }
    }
    else if (g_state == GS_RUN) {
      uint8_t key4_hold = key4_long_pressed();

      if (g_input_block_ticks > 0) {
        g_input_block_ticks--;
      } else if (g_versus_mode) {
        if (key != KEY_CONFIRM) {
          if (key == 1 && dir_now != DIR_DOWN) dir_next = DIR_UP;
          else if (key == 2 && dir_now != DIR_UP) dir_next = DIR_DOWN;
          else if (key == 3 && dir_now != DIR_RIGHT) dir_next = DIR_LEFT;
          else if (key == 4 && dir_now != DIR_LEFT) dir_next = DIR_RIGHT;
        }
      } else if (g_demo_mode) {
        demo_ai_pick_dir();
      } else if (key != KEY_CONFIRM) {
        if (key == 1 && dir_now != DIR_DOWN) dir_next = DIR_UP;
        else if (key == 2 && dir_now != DIR_UP) dir_next = DIR_DOWN;
        else if (key == 3 && dir_now != DIR_RIGHT) dir_next = DIR_LEFT;
        else if (key == 4 && dir_now != DIR_LEFT) dir_next = DIR_RIGHT;
      }

      if (key == KEY_CONFIRM || key4_hold) {
        pause_enter();
      }

      tick_cnt++;
      if (tick_cnt >= move_interval_ticks) {
        tick_cnt = 0;
        if (g_versus_mode) versus_step();
        else snake_step();
      }

      g_anim_tick++;
      if (g_anim_tick >= 8) {
        g_anim_tick = 0;
        g_food_phase ^= 1;
        if (g_state == GS_RUN) draw_food_at(food.x, food.y);
      }

      if (!g_versus_mode && g_score_flash > 0) {
        g_score_flash--;
        if (g_score_flash == 0) update_hud_nums();
      }

      if (!g_versus_mode && g_level_flash > 0) {
        draw_level_up_banner();
        g_level_flash--;
        if (g_level_flash == 0) snake_redraw_view();
      }

      play_bgm_step();
      Delay_ms(20);
    }
    else if (g_state == GS_PAUSE) {
      if (key == KEY_CONFIRM || key4_long_pressed()) {
        pause_resume();
        key_wait_release();
      } else if (key == 3) {
        g_demo_mode = 0;
        g_versus_mode = 0;
        g_state = GS_MENU;
        g_pause_drawn = 0;
        menu_leave();
        show_menu(menu_sel);
      }
      Delay_ms(30);
    }
    else if (g_state == GS_OVER) {
      if (g_demo_mode && g_demo_restart_wait > 0) {
        g_demo_restart_wait--;
        if (g_demo_restart_wait == 0) {
          rng_reseed();
          snake_reset();
          g_state = GS_RUN;
        }
      } else if (key == 4 || key == KEY_CONFIRM) {
        if (g_versus_mode) enter_versus_mode();
        else enter_run_mode();
      } else if (key == 3) {
        g_demo_mode = 0;
        g_versus_mode = 0;
        g_state = GS_MENU;
        menu_leave();
        show_menu(menu_sel);
      }
      Delay_ms(30);
    }
  }
}
