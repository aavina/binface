#include "pebble.h"

/*
  Watchface that tells the time in a Binary format
*/
  
/*
  The MIT License (MIT)
  
  Copyright (c) Anthony Avina 2014
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
  
static Window *g_window;

static Layer *g_layer;

//static int PEB_RES_X = 144;
static int PEB_RES_Y = 168;

// Light/Dark theme choices for watchface
enum Theme {
  light,
  dark
};

// Configurable settings
static enum Theme THEME = dark;  // Theme of watchface

// tm struct to compare with
static struct tm g_tick_time;

// Dependent on configured settings
static int ON_COLOR;  // Color of Bits when on
static int OFF_COLOR; // Color of bits when off
static int OUTER_BOX_SIZE = 14; // Size of Bit box (outside)
static int INNER_BOX_SIZE; // Size of Bit box (inside)
static int BOX_OFFSET;
static int X_OFFSET = 21; // Where to start drawing the Bits - x
static int Y_OFFSET = 40; // Where to start drawing the Bits - y

typedef struct {
  int x; /* x-coord of origin */
  int y; /* y-coord of origin */
  int on; /* 0 = off, anything else = on */
} Bit;

static Bit g_h1[2]; // Bits for hour1 digit
static Bit g_h0[4]; // Bits for hour0 digit
static Bit g_m1[3]; // Bits for minute1 digit
static Bit g_m0[4]; // Bits for minute0 digit

// Bit-masks to find out which bits are set
static const int g_bitmasks[] = {0x01, 0x02, 0x04, 0x08};

// Will draw an 'on' bit, or a square that's filled in
static void draw_bit_on(int x, int y, GContext *ctx) {
  graphics_context_set_fill_color(ctx, ON_COLOR);
  graphics_fill_rect(ctx, GRect(x, y, OUTER_BOX_SIZE, OUTER_BOX_SIZE), 0, GCornerNone);
}

// Will draw an 'off' bit, or a square with a hole
static void draw_bit_off(int x, int y, GContext *ctx) {
  // Draw the entire box
  draw_bit_on(x, y, ctx);
  // Draw the hole in the box
  graphics_context_set_fill_color(ctx, OFF_COLOR);
  graphics_fill_rect(ctx, GRect(x+BOX_OFFSET, y+BOX_OFFSET, INNER_BOX_SIZE, INNER_BOX_SIZE), 0, GCornerNone);
}

// Draws a bit
static void draw_bit(Bit b, GContext *ctx) {
  if(b.on == 0)
    draw_bit_off(b.x, b.y, ctx);
  else
    draw_bit_on(b.x, b.y, ctx);
}

// Finds out which bits are on/off and sets them to respective value
static void flip_bits_array(Bit *arr, int size, char digit) {
  int i;
  
  for(i=0; i < size; ++i) {
    if(digit & g_bitmasks[i])
      arr[i].on = 1;
    else
      arr[i].on = 0;
  }
}

// Updates the on/off status of bits
static void update_bits(struct tm tick_time) {
  // Get a tm structure and time digits
  int hour = tick_time.tm_hour;
  
  if(clock_is_24h_style() == S_FALSE && hour > 12)
    hour -= 12;

  int m0_digit = tick_time.tm_min % 10;
  int m1_digit = (tick_time.tm_min - m0_digit) / 10;
  int h0_digit = hour % 10;
  int h1_digit = (hour - h0_digit) / 10;

  flip_bits_array(g_h1, 2, h1_digit);
  flip_bits_array(g_h0, 4, h0_digit);
  flip_bits_array(g_m1, 3, m1_digit);
  flip_bits_array(g_m0, 4, m0_digit);
}

static void layer_update_callback(Layer *me, GContext *ctx) {
  int i, h_limit;
  
  // Check if user is in 24 or 12 hour format
  if(clock_is_24h_style() == S_TRUE)
    h_limit = 2;
  else
    h_limit = 1;
  
  // Hour_1 bits (varies based on 24 or 12 hour format)
  for(i=0; i < h_limit; ++i)
    draw_bit(g_h1[i], ctx);
  // Hour_0 bits
  for(i=0; i < 4; ++i)
    draw_bit(g_h0[i], ctx);
  // Minute_1 bits
  for(i=0; i < 3; ++i)
    draw_bit(g_m1[i], ctx);
  // Minute_0 bits
  for(i=0; i < 4; ++i)
    draw_bit(g_m0[i], ctx);
}

// Sets up positioning and states of all bits of given array
static void setup_bin_arr(Bit* arr, int size, int x, int y_offset) {
  int i;
  int y_add = OUTER_BOX_SIZE*2;
  
  for(i=0; i < size; ++i) {
    arr[i].x = x;
    arr[i].y = PEB_RES_Y-y_offset;
    arr[i].on = 0;
    y_offset += y_add;
  }
}

// Calculates positioning for bits and calls helper on each set
static void setup_binary_arrays() {
  int x_add = OUTER_BOX_SIZE*2;
  setup_bin_arr(g_h1, 2, X_OFFSET, Y_OFFSET);
  setup_bin_arr(g_h0, 4, X_OFFSET+x_add, Y_OFFSET);
  setup_bin_arr(g_m1, 3, X_OFFSET+(x_add*2), Y_OFFSET);
  setup_bin_arr(g_m0, 4, X_OFFSET+(x_add*3), Y_OFFSET);
}

// Handles the minute ticks
static void tick_handler(struct tm *tick_time, TimeUnits units_changes) {
  // Check if minute changed. If so, update. Else, do nothing
  if(g_tick_time.tm_min != tick_time->tm_min || 
     g_tick_time.tm_hour != tick_time->tm_hour)
  {
    update_bits(*tick_time);
    layer_mark_dirty(g_layer);
  }
  g_tick_time = *tick_time;
}

void init() {
  time_t temp = time(NULL);
  // Initialize some constants
  INNER_BOX_SIZE = OUTER_BOX_SIZE - 4;
  BOX_OFFSET = (OUTER_BOX_SIZE - INNER_BOX_SIZE) / 2;
  
  if(THEME == dark) {
    ON_COLOR = GColorWhite;
    OFF_COLOR = GColorBlack;
  }else {
    ON_COLOR = GColorBlack;
    OFF_COLOR = GColorWhite;
  }
  
  // Current time
  g_tick_time = *(localtime(&temp));
  
  // Setup Binary arrays
  setup_binary_arrays();

  // Register with TickTimerService
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

int main(void) {
  g_window = window_create();
  window_stack_push(g_window, true /* Animated */);
  if(THEME == light)
    window_set_background_color(g_window, GColorWhite);
  else
    window_set_background_color(g_window, GColorBlack);

  // Init the layer for display the image
  Layer *window_layer = window_get_root_layer(g_window);
  GRect bounds = layer_get_frame(window_layer);
  g_layer = layer_create(bounds);
  layer_set_update_proc(g_layer, layer_update_callback);
  layer_add_child(window_layer, g_layer);

  // Initialize everything
  init();
  
  // Initialize the bits with the current time
  update_bits(g_tick_time);
  
  app_event_loop();

  // Unsubscribe from tick-timer
  tick_timer_service_unsubscribe();

  window_destroy(g_window);
  layer_destroy(g_layer);
}
