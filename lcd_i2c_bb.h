#ifndef LCD_I2C_BB_H
#define LCD_I2C_BB_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

#define LCD_COLS 16U
#define LCD_ROWS 2U

#define I2C_LCD_PORT     GPIOB
#define I2C_LCD_SCL_PIN  GPIO_PIN_6
#define I2C_LCD_SDA_PIN  GPIO_PIN_7

void LCD_Init(void);
void LCD_Clear(void);
void LCD_Set_Cursor(uint8_t row, uint8_t col);
void LCD_Print_Line(uint8_t row, const char *text);
void LCD_Send_String(const char *str);

#endif