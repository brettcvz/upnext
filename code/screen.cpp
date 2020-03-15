/**
 *  @filename   :   screen.cpp
 *  @brief      :   Screen abstraction of cairo surface for epaper display
 *  @author     :   Brett van Zuiden
 */

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "screen.h"
#include "epd4in2b.h"
#include <algorithm>    // std::min

Screen::Screen() {
  Epd display;
};

int Screen::Init(void) {
    if (display.Init() != 0) {
        printf("e-Paper init failed\n");
        return -1;
    }

    cairo_stride = cairo_format_stride_for_width (CAIRO_FORMAT_A1, display.width);
    cairo_image_data = (uint32_t *) malloc (cairo_stride * display.height);
    cairo_surface = cairo_image_surface_create_for_data ((unsigned char *) cairo_image_data, CAIRO_FORMAT_A1, display.width, display.height, cairo_stride);

    screen_data = (unsigned char *) malloc (sizeof *screen_data * display.width * display.height / 8);
    partial_budget = (uint8_t *) malloc (sizeof *partial_budget * display.width * display.height / 8);
    return 0;
}

/**
 *  @brief: clears the screen and associated cairo surface
 */
void Screen::Clear(void) {
  display.ClearFrame();
  display.DisplayFrame();

  // Erase image data as well
  memset(cairo_image_data, 0, cairo_stride * display.height);
  memset(screen_data, 0xFF, sizeof *screen_data * display.width * display.height / 8);
  ClearPartialBudget();
  cairo_surface_mark_dirty(cairo_surface);
}

void Screen::HardWipe(void) {
  // Hard refresh to prevent burn-in
  for (int i = 0; i < 9; i++) {
    display.ClearFrame();
    display.DisplayFrame();
  }
  // Do other cleanup stuff
  Clear();
}

cairo_surface_t * Screen::GetCairoSurface(void) {
  return cairo_surface;
}

void Screen::FullRerender(void) {
  // Naive - re-renders the whole screen.
  // A better way to do this is to calculate which parts
  // have changed and do a partial update
  cairo_surface_flush(cairo_surface);
  ComputeScreenDataFromCairoData(cairo_image_data, screen_data);
  display.DisplayFrame(screen_data);
  ClearPartialBudget();
}

unsigned int reverseBits(unsigned int num)
{
    unsigned int count = sizeof(num) * 8 - 1;
    unsigned int reverse_num = num;
     
    num >>= 1; 
    while(num)
    {
       reverse_num <<= 1;       
       reverse_num |= num & 1;
       num >>= 1;
       count--;
    }
    reverse_num <<= count;
    return reverse_num;
}

void Screen::ComputeScreenDataFromCairoData(uint32_t *cairo_source_buffer, unsigned char *destination_buffer) {
  // Takes the stride-offset, int32_t data from cairo
  // and turns it into the width x height, 8-bit data that goes directly
  // onto the screen
  unsigned int i = 0;
  for(unsigned int row = 0; row < display.height; row++) {
    for(unsigned int col = 0; col < display.width; col += 32) {
      // Stride, annoyingly, is given by cairo in # of 8-bit blocks,
      // even though data is stored in 32 bit blocks
      uint32_t data = cairo_source_buffer[(row * cairo_stride * 8 + col)/ 32];
      // The screen data comes from cairo in 32-bit, little-endian blocks
      // (first pixel is last bit), which is super weird, so we fix it
      data = reverseBits(data);

      for (int bitOffset = 0; bitOffset < 32; bitOffset += 8){
        if (col + bitOffset >= display.width) {
          // Don't write past edge of screen, or we'll offset the next line
          continue;
        }
        // Write most significant byte first
        char byte = (data >> (24 - bitOffset)) & 0xFF;
        // Flip bits - for screen, 0 is dark and 1 is light
        destination_buffer[i] = byte ^ 0xFF;
        i++;
      }
    }
  }
}

void Screen::Render(void) {
  // Intelligently figures out which parts need to be updated, and does a partial update
  cairo_surface_flush(cairo_surface);

  // Calculate new screen data, keep it as a separate buffer so we can compare
  const unsigned int numBlocks = display.width * display.height / 8;
  unsigned char *new_screen_data = (unsigned char*) malloc (sizeof *new_screen_data * numBlocks);
  ComputeScreenDataFromCairoData(cairo_image_data, new_screen_data);

  // Compare new screen data to existing screen data, find "dirty" 8x1 blocks
  // and determine bounding box of "dirty" blocks
  // We are a bit lazy - we could find multiple minimal bounding boxes,
  // but we're just going to find the encompasing one
  // Bounding box = top left: (minDirtyX, minDirtyY); bottom right (maxDirtyX, maxDirtyY)
  // Initialize to the other extreme
  unsigned int minDirtyX = display.width;
  unsigned int minDirtyY = display.height;
  unsigned int maxDirtyX = 0;
  unsigned int maxDirtyY = 0;

  for (unsigned int i = 0; i < numBlocks; i++) {
    if (new_screen_data[i] != screen_data[i]) {
      unsigned int x = (i * 8) % display.width;
      unsigned int y = (i * 8) / display.width;
      // Dirty
      minDirtyX = std::min(x, minDirtyX);
      maxDirtyX = std::max(x + 7, maxDirtyX);

      minDirtyY = std::min(y, minDirtyY);
      maxDirtyY = std::max(y, maxDirtyY);
    }
  }
  
  int dirtyWidth = maxDirtyX - minDirtyX;
  int dirtyHeight = maxDirtyY - minDirtyY;
  if (dirtyWidth < 0 || dirtyHeight < 0) {
    // No-op
    std::cout << "Not refreshing, because nothing changed" << std::endl;
  } else if (2 * dirtyWidth * dirtyHeight > display.width * display.height) {
    // If dirty area is > 50% of display area, do a full refresh
    display.DisplayFrame(new_screen_data);
    ClearPartialBudget();
  } else {
    // Prevent screen burnout by keeping track of when we use partial LUT on a block
    bool overBudget = false;
    for (unsigned int x = minDirtyX; x < maxDirtyX; x += 8) {
      for (unsigned int y = minDirtyY; y < maxDirtyY; y++) {
        unsigned int i = (x + y * display.width) / 8;
        partial_budget[i]++;
        overBudget = overBudget || partial_budget[i] >= MAX_PARTIAL_BUDGET;
      }
    }

    if (overBudget) {
      // If we are over our "budget" of partial updates for any block, do a full refresh
      display.DisplayFrame(new_screen_data);
      ClearPartialBudget();
    } else {
      // Partial update
      display.DisplayPartialFrame(new_screen_data, minDirtyX, minDirtyY, dirtyWidth + 1, dirtyHeight + 1);
    }
  }

  // Update screen data with new data
  free(screen_data);
  screen_data = new_screen_data;
}

void Screen::ClearPartialBudget(void) {
    memset(partial_budget, 0, sizeof *partial_budget * display.width * display.height / 8);
}
void Screen::Cleanup(void) {
  display.Sleep();
  cairo_surface_destroy (cairo_surface);
  free(cairo_image_data);
  free(screen_data);
  free(partial_budget);
}
