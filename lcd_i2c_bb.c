#include "lcd_i2c_bb.h"
#include <string.h>

#define LCD_I2C_BACKLIGHT 0x08U
#define LCD_I2C_EN        0x04U
#define LCD_I2C_RS        0x01U

static uint8_t lcd_i2c_addr = 0x27U;
static bool lcd_is_ready = false;
static char lcd_last_line[LCD_ROWS][LCD_COLS + 1] = {{0}};

static void I2C1_User_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = I2C_LCD_SCL_PIN | I2C_LCD_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; 
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_LCD_PORT, &GPIO_InitStruct);

    /* Idle bus = HIGH on both lines */
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN | I2C_LCD_SDA_PIN, GPIO_PIN_SET);
}

static void I2C1_Delay(void) {
    volatile uint32_t d;
    for (d = 0U; d < 90U; d++) { __NOP(); } 
}

static void LCD_ShortDelay(void) {
    volatile uint32_t d;
    for (d = 0U; d < 20U; d++) { __NOP(); } 
}

static void I2C1_Start(void) {
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_SET);
    I2C1_Delay();
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_RESET);
    I2C1_Delay();
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_RESET);
}

static void I2C1_Stop(void) {
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_RESET);
    I2C1_Delay();
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_SET);
    I2C1_Delay();
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_SET);
    I2C1_Delay();
}

static bool I2C1_WriteByteRaw(uint8_t value) {
    uint8_t bit;
    for (bit = 0U; bit < 8U; bit++) {
        if ((value & 0x80U) != 0U) HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_SET);
        else HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_RESET);
        
        I2C1_Delay();
        HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_SET);
        I2C1_Delay();
        HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_RESET);
        value <<= 1;
    }
    /* ACK cycle */
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SDA_PIN, GPIO_PIN_SET);
    I2C1_Delay();
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_SET);
    I2C1_Delay();
    bit = (HAL_GPIO_ReadPin(I2C_LCD_PORT, I2C_LCD_SDA_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
    HAL_GPIO_WritePin(I2C_LCD_PORT, I2C_LCD_SCL_PIN, GPIO_PIN_RESET);
    I2C1_Delay();
    return (bit != 0U);
}

static bool I2C1_WriteBytes(uint8_t addr7, const uint8_t *data, uint8_t len) {
    uint8_t i;
    bool ok;
    I2C1_Start();
    ok = I2C1_WriteByteRaw((uint8_t)(addr7 << 1));
    if (!ok) { I2C1_Stop(); return false; }
    
    for (i = 0U; i < len; i++) {
        ok = I2C1_WriteByteRaw(data[i]);
        if (!ok) { I2C1_Stop(); return false; }
    }
    I2C1_Stop();
    return true;
}

static bool I2C1_WriteByte(uint8_t addr7, uint8_t data) {
    return I2C1_WriteBytes(addr7, &data, 1U);
}

static bool LCD_Detect_Address(void) {
    static const uint8_t candidates[] = {
        0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, /* PCF8574 */
        0x38U, 0x39U, 0x3AU, 0x3BU, 0x3CU, 0x3DU, 0x3EU, 0x3FU  /* PCF8574A */
    };
    for (uint8_t i = 0U; i < (uint8_t)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        if (I2C1_WriteByte(candidates[i], LCD_I2C_BACKLIGHT)) {
            lcd_i2c_addr = candidates[i];
            return true;
        }
    }
    return false;
}

static void LCD_Send_4Bits(uint8_t nibble, uint8_t rs) {
    uint8_t tx[2];
    uint8_t data = (uint8_t)((nibble & 0x0FU) << 4);
    data |= LCD_I2C_BACKLIGHT;
    if (rs != 0U) data |= LCD_I2C_RS;
    tx[0] = (uint8_t)(data | LCD_I2C_EN);
    tx[1] = (uint8_t)(data & (uint8_t)(~LCD_I2C_EN));
    (void)I2C1_WriteBytes(lcd_i2c_addr, tx, 2U);
    LCD_ShortDelay();
}

static void LCD_Send_Byte(uint8_t value, uint8_t rs) {
    uint8_t tx[4];
    uint8_t high = (uint8_t)((value >> 4) & 0x0FU);
    uint8_t low  = (uint8_t)(value & 0x0FU);
    uint8_t base;
    
    base = (uint8_t)(high << 4) | LCD_I2C_BACKLIGHT;
    if (rs != 0U) base |= LCD_I2C_RS;
    tx[0] = (uint8_t)(base | LCD_I2C_EN);
    tx[1] = (uint8_t)(base & (uint8_t)(~LCD_I2C_EN));
    
    base = (uint8_t)(low << 4) | LCD_I2C_BACKLIGHT;
    if (rs != 0U) base |= LCD_I2C_RS;
    tx[2] = (uint8_t)(base | LCD_I2C_EN);
    tx[3] = (uint8_t)(base & (uint8_t)(~LCD_I2C_EN));
    
    (void)I2C1_WriteBytes(lcd_i2c_addr, tx, 4U);
    LCD_ShortDelay();
}

static void LCD_Send_Cmd(uint8_t cmd) {
    if (!lcd_is_ready) return;
    LCD_Send_Byte(cmd, 0U);
    if ((cmd == 0x01U) || (cmd == 0x02U)) HAL_Delay(2);
    else LCD_ShortDelay();
}

static void LCD_Send_Data(uint8_t data) {
    if (!lcd_is_ready) return;
    LCD_Send_Byte(data, 1U);
}

static void LCD_Format_Line(const char *src, char *dst) {
    uint8_t i = 0U;
    while ((i < LCD_COLS) && (src[i] != '\0')) { dst[i] = src[i]; i++; }
    while (i < LCD_COLS) { dst[i] = ' '; i++; }
    dst[LCD_COLS] = '\0';
}

static void LCD_Send_Buffer(const uint8_t *data, uint8_t len, uint8_t rs) {
    uint8_t i, idx = 0U, base;
    uint8_t tx[LCD_COLS * 4U];
    if (len > LCD_COLS) len = LCD_COLS;
    
    for (i = 0U; i < len; i++) {
        base = (uint8_t)((data[i] >> 4) & 0x0FU);
        base = (uint8_t)((base << 4) | LCD_I2C_BACKLIGHT);
        if (rs != 0U) base |= LCD_I2C_RS;
        tx[idx++] = (uint8_t)(base | LCD_I2C_EN);
        tx[idx++] = (uint8_t)(base & (uint8_t)(~LCD_I2C_EN));
        
        base = (uint8_t)(data[i] & 0x0FU);
        base = (uint8_t)((base << 4) | LCD_I2C_BACKLIGHT);
        if (rs != 0U) base |= LCD_I2C_RS;
        tx[idx++] = (uint8_t)(base | LCD_I2C_EN);
        tx[idx++] = (uint8_t)(base & (uint8_t)(~LCD_I2C_EN));
    }
    if (idx > 0U) {
        (void)I2C1_WriteBytes(lcd_i2c_addr, tx, idx);
        LCD_ShortDelay();
    }
}

void LCD_Init(void) {
    I2C1_User_Init();
    lcd_is_ready = LCD_Detect_Address();
    if (!lcd_is_ready) return;
    
    HAL_Delay(40);
    LCD_Send_4Bits(0x03U, 0U); HAL_Delay(5);
    LCD_Send_4Bits(0x03U, 0U); HAL_Delay(1);
    LCD_Send_4Bits(0x03U, 0U); HAL_Delay(1);
    LCD_Send_4Bits(0x02U, 0U); 
    
    LCD_Send_Cmd(0x28U); /* 4-bit, multi-line, 5x8 font */
    LCD_Send_Cmd(0x0CU); /* Display ON, cursor OFF */
    LCD_Send_Cmd(0x06U); /* Entry mode: increment */
    LCD_Clear();
}

void LCD_Clear(void) {
    if (!lcd_is_ready) return;
    LCD_Send_Cmd(0x01U);
    HAL_Delay(2);
    memset(lcd_last_line, 0, sizeof(lcd_last_line));
}

void LCD_Set_Cursor(uint8_t row, uint8_t col) {
    static const uint8_t row_offsets[LCD_ROWS] = {0x00U, 0x40U}; 
    if (!lcd_is_ready || row >= LCD_ROWS || col >= LCD_COLS) return;
    LCD_Send_Cmd((uint8_t)(0x80U + row_offsets[row] + col));
}

void LCD_Send_String(const char *str) {
    if (!lcd_is_ready) return;
    while (*str != '\0') { LCD_Send_Data((uint8_t)(*str)); str++; }
}

void LCD_Print_Line(uint8_t row, const char *text) {
    char line[LCD_COLS + 1];
    if (!lcd_is_ready || row >= LCD_ROWS) return;
    
    LCD_Format_Line(text, line);
    
    if (strncmp(lcd_last_line[row], line, LCD_COLS) == 0) return;
    
    LCD_Set_Cursor(row, 0);
    LCD_Send_Buffer((const uint8_t *)line, LCD_COLS, 1U);
    memcpy(lcd_last_line[row], line, sizeof(line));
}