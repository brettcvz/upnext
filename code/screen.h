/**
 *  @filename   :   screen.h
 *  @brief      :   Header file for Screen abstraction of cairo surface for epaper display
 *  @author     :   Brett van Zuiden
 *  
 */

#ifndef SCREEN_H
#define SCREEN_H

#define MAX_PARTIAL_BUDGET 10

#include <pango/pangocairo.h>
#include <stdint.h>
#include "epd4in2b.h"

class Screen {
public:
    Screen();

    int  Init(void);
    void Clear(void);
    void HardWipe(void);
    cairo_surface_t * GetCairoSurface(void);
    void Render(void);
    void FullRerender(void);
    void Cleanup(void);

private:
    Epd display;
    uint32_t *cairo_image_data;
    int cairo_stride;
    unsigned char *screen_data;
    uint8_t *partial_budget;
    cairo_surface_t *cairo_surface;

    void ComputeScreenDataFromCairoData(uint32_t *cairo_source_buffer, unsigned char *destination_buffer);
    void ClearPartialBudget();
};

#endif
