/*
 * Project: pico9918
 *
 * Copyright (c) 2024 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/pico-prom
 *
 */

#include "vga.h"
#include "vga-modes.h"

#include "tms9918.pio.h"

#include "vrEmuTms9918Util.h"

#include "pico/stdlib.h"

#include <stdlib.h>


 /*
  * External pins
  *
  * Pin  | GPIO | Name   | TMS9918A Pin
  * -----+------+--------+-------------
  *  19  |  14  |  CD0   |  24
  *  20  |  15  |  CD1   |  23
  *  21  |  16  |  CD2   |  22
  *  22  |  17  |  CD3   |  21
  *  24  |  18  |  CD4   |  20
  *  25  |  19  |  CD5   |  19
  *  26  |  20  |  CD6   |  18
  *  27  |  21  |  CD7   |  17
  *  29  |  22  |  /INT  |  16
  *  30  |  RUN |  RST   |  34
  *  31  |  26  |  /CSR  |  15
  *  32  |  27  |  /CSW  |  14
  *  34  |  28  |  MODE  |  13
  */

#define GPIO_CD0 14
#define GPIO_CSR 26
#define GPIO_CSW 27
#define GPIO_MODE 28

#define GPIO_CD_MASK (0xff << GPIO_CD0)
#define GPIO_CSR_MASK (0x01 << GPIO_CSR)
#define GPIO_CSW_MASK (0x01 << GPIO_CSW)
#define GPIO_MODE_MASK (0x01 << GPIO_MODE)


  /* todo should I make this uint32_t and shift the bits too?*/
static uint32_t reversed[] =
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

uint8_t regValue = 0;


VrEmuTms9918* tms = NULL;


void gpioCallback(uint gpio, uint32_t events)
{
  if (gpio == GPIO_CSR)
  {
    gpio_set_dir_out_masked(GPIO_CD_MASK);
    uint32_t gpios = gpio_get_all();
    if (gpios & GPIO_MODE_MASK)
    {
      gpio_put_masked(GPIO_CD_MASK, reversed[vrEmuTms9918ReadStatus(tms)] << GPIO_CD0);
    }
    else
    {
      gpio_put_masked(GPIO_CD_MASK, reversed[vrEmuTms9918ReadDataNoInc(tms)] << GPIO_CD0);
      vrEmuTms9918ReadData(tms);
    }
  }
  else if (gpio == GPIO_CSW)
  {
    gpio_set_dir_in_masked(GPIO_CD_MASK);
    uint32_t gpios = gpio_get_all();
    uint8_t value = reversed[(gpios >> GPIO_CD0) & 0xff];
    if (gpios & GPIO_MODE_MASK)
    {
      vrEmuTms9918WriteAddr(tms, value);
    }
    else
    {
      vrEmuTms9918WriteData(tms, value);
    }
  }
}



uint16_t __aligned(4) tmsPal[16];
uint8_t __aligned(4) tmsScanlineBuffer[TMS9918_PIXELS_X];

uint16_t colorFromRgb(uint16_t r, uint16_t g, uint16_t b)
{
  return ((uint16_t)(r / 16.0f) & 0x0f) | (((uint16_t)(g / 16.0f) & 0x0f) << 4) | (((uint16_t)(b / 16.0f) & 0x0f) << 8);
}

static void tmsScanline(uint16_t y, VgaParams* params, uint16_t* pixels)
{
  const uint32_t vBorder = (params->vVirtualPixels - TMS9918_PIXELS_Y) / 2;
  const uint32_t hBorder = (params->hVirtualPixels - TMS9918_PIXELS_X) / 2;

  uint16_t bg = tmsPal[vrEmuTms9918RegValue(tms, TMS_REG_FG_BG_COLOR) & 0x0f];

  if (y < vBorder || y >= (vBorder + TMS9918_PIXELS_Y))
  {
    for (int x = 0; x < params->hVirtualPixels; ++x)
    {
      pixels[x] = bg;
    }
    return;
  }

  y -= vBorder;

  for (int x = 0; x < hBorder; ++x)
  {
    pixels[x] = bg;
  }

  vrEmuTms9918ScanLine(tms, y, tmsScanlineBuffer);

  int tmsX = 0;
  for (int x = hBorder; x < hBorder + TMS9918_PIXELS_X; ++x, ++tmsX)
  {
    pixels[x] = tmsPal[tmsScanlineBuffer[tmsX]];
  }

  for (int x = hBorder + TMS9918_PIXELS_X; x < params->hVirtualPixels; ++x)
  {
    pixels[x] = bg;
  }
}


int main(void)
{
  set_sys_clock_khz(252000, false);

  gpio_init_mask(GPIO_CD_MASK | GPIO_CSR_MASK | GPIO_CSW_MASK | GPIO_MODE_MASK);
  gpio_set_dir_all_bits(0); // all inputs

  // Set up TMS9918 external PINs first. This is a priority to capture early writes 
  gpio_set_irq_enabled_with_callback(GPIO_CSW, GPIO_IRQ_EDGE_FALL, true, &gpioCallback);
  gpio_set_irq_enabled(GPIO_CSR, GPIO_IRQ_EDGE_FALL, true);

  tms = vrEmuTms9918New();

  for (int c = 0; c < 16; ++c)
  {
    uint32_t rgba8 = vrEmuTms9918Palette[c];
    tmsPal[c] = colorFromRgb((rgba8 & 0xff000000) >> 24, (rgba8 & 0xff0000) >> 16, (rgba8 & 0xff00) >> 8);
  }

  // Then set up VGA output
  VgaInitParams params = { 0 };
  params.params = vgaGetParams(VGA_640_480_60HZ, 2);
  params.scanlineFn = tmsScanline;


  vgaInit(params);

  while (1)
  {
    tight_loop_contents();
  }

  return 0;
}