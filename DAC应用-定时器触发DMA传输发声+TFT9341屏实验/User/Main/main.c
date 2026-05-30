/********************************************************
贪吃蛇（按键控制）
基于现有 TFT9341 + DAC/DMA/TIM3 工程改造
********************************************************/

#include <stdlib.h>
#include "stm32f10x.h"
#include "hw_config.h"
#include "lcd.h"

/* -------------------- 按键定义（低电平按下） -------------------- */
#define KEY_UP_PRESSED()     (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0)
#define KEY_DOWN_PRESSED()   (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_12) == 0)
#define KEY_LEFT_PRESSED()   (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 0)
#define KEY_RIGHT_PRESSED()  (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)

/* -------------------- 地图参数 -------------------- */
#define CELL_SIZE      10
#define BOARD_X        10
#define BOARD_Y        30
#define BOARD_W        22
#define BOARD_H        22
#define MAX_SNAKE_LEN  (BOARD_W * BOARD_H)

/* -------------------- 音频相关（沿用原工程） -------------------- */
uint32_t DAC_DHR12R2_Address = 0x40007414;
const uint16_t Sine12bit[32] = {
  2047,2447,2831,3185,3498,3750,3939,4056,4095,4056,3939,3750,3495,3185,2831,2447,
  2047,1647,1263,909,599,344,155,38,0,38,155,344,599,909,1263,1647
};
uint32_t DAC2Sine12bit[32];

static const uint16_t bgm_notes[] = {
  659*2,587*2,659*2,784*2,880*2,784*2,523*4,880*2,
  784*2,659*2,784*2,784*2,784*2,784*2,880*2,880*2
};
#define BGM_NOTE_COUNT (sizeof(bgm_notes)/sizeof(bgm_notes[0]))

/* -------------------- 游戏状态 -------------------- */
typedef enum {
  GS_BOOT = 0,
  GS_MENU,
  GS_RUN,
  GS_PAUSE,
  GS_OVER
} GameState;

typedef enum { DIR_UP=0, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;
typedef enum { DIF_EASY=0, DIF_NORMAL, DIF_HARD } Difficulty;

typedef struct { uint8_t x; uint8_t y; } Point;

static GameState g_state = GS_BOOT;
static Difficulty g_diff = DIF_NORMAL;
static uint8_t g_music_on = 1;
static uint16_t g_score = 0;
static uint16_t g_best = 0;
static uint8_t g_level = 1;

static Point snake[MAX_SNAKE_LEN];
static uint16_t snake_len = 0;
static Dir dir_now = DIR_RIGHT;
static Dir dir_next = DIR_RIGHT;
static Point food;
static uint8_t move_interval_ticks = 7;
static uint8_t tick_cnt = 0;

/* -------------------- 工具函数 -------------------- */
static void draw_cell(uint8_t gx, uint8_t gy, uint16_t color)
{
  uint16_t x1 = BOARD_X + gx * CELL_SIZE;
  uint16_t y1 = BOARD_Y + gy * CELL_SIZE;
  LCD_Fill(x1 + 1, y1 + 1, x1 + CELL_SIZE - 2, y1 + CELL_SIZE - 2, color);
}

static void draw_board_frame(void)
{
  uint16_t x2 = BOARD_X + BOARD_W * CELL_SIZE;
  uint16_t y2 = BOARD_Y + BOARD_H * CELL_SIZE;
  POINT_COLOR = WHITE;
  LCD_DrawRectangle(BOARD_X - 1, BOARD_Y - 1, x2 + 1, y2 + 1);
}

static uint8_t snake_contains(uint8_t x, uint8_t y)
{
  uint16_t i;
  for (i = 0; i < snake_len; i++) {
    if (snake[i].x == x && snake[i].y == y) return 1;
  }
  return 0;
}

static void spawn_food(void)
{
  uint16_t seed = (uint16_t)(g_score * 37 + g_level * 17 + snake_len * 11);
  uint8_t x, y;
  do {
    seed = (uint16_t)(seed * 25173 + 13849);
    x = (uint8_t)(seed % BOARD_W);
    seed = (uint16_t)(seed * 25173 + 13849);
    y = (uint8_t)(seed % BOARD_H);
  } while (snake_contains(x, y));
  food.x = x;
  food.y = y;
  draw_cell(food.x, food.y, RED);
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

static void update_hud(void)
{
  LCD_Fill(0, 0, 239, 24, BLACK);

  BACK_COLOR = BLACK;
  POINT_COLOR = YELLOW;

  Show_Str(2, 2, YELLOW, BLACK, (u8*)"SCORE", 16, 0);
  Show_Str(98, 2, YELLOW, BLACK, (u8*)"LV", 16, 0);
  Show_Str(150, 2, YELLOW, BLACK, (u8*)"BEST", 16, 0);

  POINT_COLOR = WHITE;
  LCD_ShowNum(50, 2, g_score, 4, 16);
  LCD_ShowNum(122, 2, g_level, 2, 16);
  LCD_ShowNum(190, 2, g_best, 4, 16);
}

static void snake_reset(void)
{
  uint16_t i;
  LCD_Clear(BLACK);
  draw_board_frame();

  snake_len = 3;
  snake[0].x = 5;  snake[0].y = 10;
  snake[1].x = 4;  snake[1].y = 10;
  snake[2].x = 3;  snake[2].y = 10;
  dir_now = DIR_RIGHT;
  dir_next = DIR_RIGHT;

  for (i = 0; i < snake_len; i++) draw_cell(snake[i].x, snake[i].y, GREEN);

  g_score = 0;
  g_level = 1;
  update_speed_by_level();
  update_hud();
  spawn_food();
}

static void play_bgm_step(void)
{
  static uint8_t idx = 0;
  if (!g_music_on || g_state != GS_RUN) {
    TIM_Cmd(TIM3, DISABLE);
    return;
  }
  TIM_Configuration(bgm_notes[idx]);
  idx++;
  if (idx >= BGM_NOTE_COUNT) idx = 0;
}

static void game_over_screen(void)
{
  g_state = GS_OVER;
  TIM_Cmd(TIM3, DISABLE);
  LCD_Fill(30, 100, 210, 170, BLACK);
  POINT_COLOR = RED;
  Show_Str(62, 108, RED, BLACK, (u8*)"GAME OVER", 24, 0);
  POINT_COLOR = WHITE;
  Show_Str(46, 136, WHITE, BLACK, (u8*)"K4:RETRY K3:MENU", 16, 0);
}

static void snake_step(void)
{
  Point head = snake[0];
  Point newh;
  Point old_tail = snake[snake_len - 1];
  uint16_t i;

  dir_now = dir_next;
  newh = head;

  if (dir_now == DIR_UP) newh.y--;
  else if (dir_now == DIR_DOWN) newh.y++;
  else if (dir_now == DIR_LEFT) newh.x--;
  else newh.x++;

  if (newh.x >= BOARD_W || newh.y >= BOARD_H || snake_contains(newh.x, newh.y)) {
    game_over_screen();
    return;
  }

  for (i = snake_len; i > 0; i--) snake[i] = snake[i - 1];
  snake[0] = newh;

  if (newh.x == food.x && newh.y == food.y) {
    if (snake_len < MAX_SNAKE_LEN - 1) snake_len++;
    g_score += 10;
    if (g_score > g_best) g_best = g_score;
    if ((g_score % 100) == 0 && g_level < 20) g_level++;
    update_speed_by_level();
    update_hud();
    spawn_food();
  } else {
    draw_cell(old_tail.x, old_tail.y, BLACK);
  }

  draw_cell(newh.x, newh.y, GREEN);
}

static uint8_t key_scan_click(void)
{
  static uint8_t lock = 0;
  uint8_t k = 0;
  if (KEY_UP_PRESSED()) k = 1;
  else if (KEY_DOWN_PRESSED()) k = 2;
  else if (KEY_LEFT_PRESSED()) k = 3;
  else if (KEY_RIGHT_PRESSED()) k = 4;

  if (k && !lock) {
    Delay_ms(12);
    lock = 1;
    return k;
  }
  if (!k) lock = 0;
  return 0;
}

static uint8_t key4_long_pressed(void)
{
  static uint8_t hold_ticks = 0;
  if (KEY_RIGHT_PRESSED()) {
    if (hold_ticks < 255) hold_ticks++;
    if (hold_ticks >= 25) {
      hold_ticks = 0;
      return 1;
    }
  } else {
    hold_ticks = 0;
  }
  return 0;
}

static void show_boot(void)
{
  LCD_Clear(BLACK);
  POINT_COLOR = CYAN;
  Show_Str(35, 60, CYAN, BLACK, (u8*)"SNAKE GAME", 32, 0);
  POINT_COLOR = WHITE;
  Show_Str(20, 120, WHITE, BLACK, (u8*)"K1/K2 MOVE", 16, 0);
  Show_Str(20, 145, WHITE, BLACK, (u8*)"K3 BACK   K4 OK", 16, 0);
  Show_Str(20, 190, YELLOW, BLACK, (u8*)"PRESS ANY KEY", 16, 0);
}

static void show_menu(uint8_t sel)
{
  LCD_Clear(BLACK);
  POINT_COLOR = WHITE;
  Show_Str(70, 20, WHITE, BLACK, (u8*)"MAIN MENU", 24, 0);

  Show_Str(30, 70, (sel==0)?YELLOW:WHITE, BLACK, (u8*)"> START", 16, 0);
  Show_Str(30, 95, (sel==1)?YELLOW:WHITE, BLACK, (u8*)"> DIFFICULTY", 16, 0);
  Show_Str(30, 120,(sel==2)?YELLOW:WHITE, BLACK, (u8*)"> MUSIC", 16, 0);

  if (g_diff == DIF_EASY) Show_Str(170,95, GREEN, BLACK, (u8*)"EASY",16,0);
  else if (g_diff == DIF_NORMAL) Show_Str(170,95, GREEN, BLACK, (u8*)"NORMAL",16,0);
  else Show_Str(170,95, GREEN, BLACK, (u8*)"HARD",16,0);

  Show_Str(170,120, GREEN, BLACK, g_music_on?(u8*)"ON":(u8*)"OFF", 16, 0);
  Show_Str(20, 210, CYAN, BLACK, (u8*)"K1/K2 SEL K3/K4 CHANGE", 16, 0);
}

int main(void)
{
  uint8_t i;
  uint8_t key;
  uint8_t menu_sel = 0;

  SystemInit();
  Delay_Init();
  GPIO_Configuration();
  LCD_Init();

  for (i = 0; i < 32; i++) DAC2Sine12bit[i] = (Sine12bit[i] << 16) + Sine12bit[i];
  DAC_Configuration();
  DMA_Configuration();

  g_state = GS_BOOT;
  show_boot();

  while (1) {
    key = key_scan_click();

    if (g_state == GS_BOOT) {
      if (key) {
        g_state = GS_MENU;
        show_menu(menu_sel);
      }
    }
    else if (g_state == GS_MENU) {
      if (key == 1) {
        menu_sel = (menu_sel + 2) % 3;
        show_menu(menu_sel);
      } else if (key == 2) {
        menu_sel = (menu_sel + 1) % 3;
        show_menu(menu_sel);
      } else if (key == 3 || key == 4) {
        if (menu_sel == 0 && key == 4) {
          snake_reset();
          g_state = GS_RUN;
        } else if (menu_sel == 1) {
          if (key == 4) g_diff = (Difficulty)((g_diff + 1) % 3);
          else g_diff = (Difficulty)((g_diff + 2) % 3);
          show_menu(menu_sel);
        } else if (menu_sel == 2) {
          g_music_on = !g_music_on;
          if (!g_music_on) TIM_Cmd(TIM3, DISABLE);
          show_menu(menu_sel);
        }
      }
    }
    else if (g_state == GS_RUN) {
      uint8_t key4_hold = key4_long_pressed();

      if (key == 1 && dir_now != DIR_DOWN) dir_next = DIR_UP;
      else if (key == 2 && dir_now != DIR_UP) dir_next = DIR_DOWN;
      else if (key == 3 && dir_now != DIR_RIGHT) dir_next = DIR_LEFT;
      else if (key == 4 && dir_now != DIR_LEFT) dir_next = DIR_RIGHT;

      if (key4_hold) {
        g_state = GS_PAUSE;
        TIM_Cmd(TIM3, DISABLE);
      }

      tick_cnt++;
      if (tick_cnt >= move_interval_ticks) {
        tick_cnt = 0;
        snake_step();
        play_bgm_step();
      }
      Delay_ms(20);
    }
    else if (g_state == GS_PAUSE) {
      Show_Str(70, 110, YELLOW, BLACK, (u8*)"PAUSED", 24, 0);
      Show_Str(24, 140, WHITE, BLACK, (u8*)"K4 HOLD:CONT K3:MENU", 16, 0);
      if (key4_long_pressed()) {
        g_state = GS_RUN;
        LCD_Fill(20, 100, 220, 170, BLACK);
      } else if (key == 3) {
        g_state = GS_MENU;
        show_menu(menu_sel);
      }
      Delay_ms(30);
    }
    else if (g_state == GS_OVER) {
      if (key == 4) {
        snake_reset();
        g_state = GS_RUN;
      } else if (key == 3) {
        g_state = GS_MENU;
        show_menu(menu_sel);
      }
      Delay_ms(30);
    }
  }
}
