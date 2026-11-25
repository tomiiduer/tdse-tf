/**
 * @file    display.c
 * @brief   Driver HD44780 por I2C (backpack PCF8574). Ver display.h.
 */

#include "display.h"

/********************** comandos HD44780 *************************************/

#define LCD_CMD_CLEAR_DISPLAY       0x01u
#define LCD_CMD_ENTRY_MODE_SET      0x04u
#define LCD_CMD_DISPLAY_CONTROL     0x08u
#define LCD_CMD_FUNCTION_SET        0x20u
#define LCD_CMD_SET_DDRAM_ADDR      0x80u

#define LCD_ENTRY_INCREMENT         0x02u
#define LCD_DISPLAY_ON              0x04u
#define LCD_FUNCTION_8BITS          0x10u
#define LCD_FUNCTION_2LINES         0x08u

/********************** pines del PCF8574 ************************************/

/* Mapeo del backpack tipico (LCM1602 / HW-061):
 * P0 = RS, P1 = RW, P2 = EN, P3 = luz de fondo, P4..P7 = D4..D7 */
#define PCF_RS          0x01u
#define PCF_RW          0x02u   /* siempre 0: solo se escribe */
#define PCF_EN          0x04u
#define PCF_BACKLIGHT   0x08u

#define PROBE_TRIALS    3u

/********************** estado interno ***************************************/

typedef enum { LCD_INSTRUCTION = 0, LCD_DATA = 1 } lcd_reg_t;

static struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t  address;       /* 7 bits */
    uint8_t  columns;
    uint8_t  rows;
    uint8_t  backlight;     /* PCF_BACKLIGHT o 0 */
} lcd;

/********************** funciones privadas ***********************************/

static display_status_t pcf_write(const uint8_t *buf, uint16_t len)
{
    if (lcd.hi2c == NULL) {
        return DISPLAY_ERROR_PARAM;
    }
    return (HAL_I2C_Master_Transmit(lcd.hi2c, (uint16_t)(lcd.address << 1),
                                    (uint8_t *)buf, len,
                                    DISPLAY_I2C_TIMEOUT_MS) == HAL_OK)
           ? DISPLAY_OK : DISPLAY_ERROR_BUS;
}

/* Envia solo el nibble alto (usado en la secuencia de arranque en 8 bits). */
static display_status_t lcd_write_nibble(uint8_t value)
{
    uint8_t hi = (value & 0xF0u) | lcd.backlight;
    uint8_t buf[3] = { hi, (uint8_t)(hi | PCF_EN), hi };

    return pcf_write(buf, sizeof(buf));
}

/* Envia un byte completo en modo 4 bits, en una sola transaccion I2C:
 *   [setup RS] [nibble alto + EN] [nibble alto] [nibble bajo + EN] [nibble bajo]
 * El HD44780 captura el dato en el flanco descendente de EN. Cada byte I2C
 * dura >= 22 us, de sobra para el ancho minimo de EN (450 ns), y la duracion
 * de la propia transferencia cubre los 37 us de ejecucion del comando. */
static display_status_t lcd_write(lcd_reg_t reg, uint8_t value)
{
    uint8_t ctrl = lcd.backlight | ((reg == LCD_DATA) ? PCF_RS : 0u);
    uint8_t hi   = (value & 0xF0u) | ctrl;
    uint8_t lo   = (uint8_t)(value << 4) | ctrl;
    uint8_t buf[5] = { ctrl, (uint8_t)(hi | PCF_EN), hi, (uint8_t)(lo | PCF_EN), lo };

    return pcf_write(buf, sizeof(buf));
}

static bool pcf_is_ready(uint8_t address)
{
    return HAL_I2C_IsDeviceReady(lcd.hi2c, (uint16_t)(address << 1),
                                 PROBE_TRIALS, DISPLAY_I2C_TIMEOUT_MS) == HAL_OK;
}

/********************** API **************************************************/

display_status_t displayInit(const display_config_t *config)
{
    display_status_t st = DISPLAY_OK;

    if ((config == NULL) || (config->hi2c == NULL) ||
        (config->columns == 0u) || (config->columns > 40u) ||
        (config->rows == 0u) || (config->rows > 4u)) {
        return DISPLAY_ERROR_PARAM;
    }

    lcd.hi2c      = config->hi2c;
    lcd.columns   = config->columns;
    lcd.rows      = config->rows;
    lcd.backlight = PCF_BACKLIGHT;

    /* Direccion: fija o autodetectada */
    if (config->address != DISPLAY_I2C_ADDR_AUTO) {
        lcd.address = config->address;
        if (!pcf_is_ready(lcd.address)) {
            return DISPLAY_ERROR_NOT_FOUND;
        }
    } else if (pcf_is_ready(DISPLAY_I2C_ADDR_PCF8574)) {
        lcd.address = DISPLAY_I2C_ADDR_PCF8574;
    } else if (pcf_is_ready(DISPLAY_I2C_ADDR_PCF8574A)) {
        lcd.address = DISPLAY_I2C_ADDR_PCF8574A;
    } else {
        lcd.address = DISPLAY_I2C_ADDR_PCF8574;
        return DISPLAY_ERROR_NOT_FOUND;
    }

    /* Inicializacion por instrucciones (datasheet HD44780, fig. 24) */
    HAL_Delay(50);

    st |= lcd_write_nibble(LCD_CMD_FUNCTION_SET | LCD_FUNCTION_8BITS);
    HAL_Delay(5);
    st |= lcd_write_nibble(LCD_CMD_FUNCTION_SET | LCD_FUNCTION_8BITS);
    HAL_Delay(1);
    st |= lcd_write_nibble(LCD_CMD_FUNCTION_SET | LCD_FUNCTION_8BITS);
    HAL_Delay(1);
    st |= lcd_write_nibble(LCD_CMD_FUNCTION_SET);              /* pasa a 4 bits */
    HAL_Delay(1);

    st |= lcd_write(LCD_INSTRUCTION, LCD_CMD_FUNCTION_SET |
                    ((lcd.rows > 1u) ? LCD_FUNCTION_2LINES : 0u));   /* 4 bits, 5x8 */
    st |= lcd_write(LCD_INSTRUCTION, LCD_CMD_DISPLAY_CONTROL);         /* display OFF */
    st |= displayClear();
    st |= lcd_write(LCD_INSTRUCTION, LCD_CMD_ENTRY_MODE_SET | LCD_ENTRY_INCREMENT);
    st |= lcd_write(LCD_INSTRUCTION, LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON);

    return (st == DISPLAY_OK) ? DISPLAY_OK : DISPLAY_ERROR_BUS;
}

uint8_t displayGetAddress(void)
{
    return lcd.address;
}

display_status_t displayCharPositionWrite(uint8_t x, uint8_t y)
{
    /* Direccion DDRAM del primer caracter de cada fila. Las filas 3 y 4
     * continuan a las filas 1 y 2 (vale para 16x4 y 20x4). */
    const uint8_t row_offset[4] = { 0x00u, 0x40u, lcd.columns, (uint8_t)(0x40u + lcd.columns) };

    if ((x >= lcd.columns) || (y >= lcd.rows)) {
        return DISPLAY_ERROR_PARAM;
    }
    return lcd_write(LCD_INSTRUCTION, LCD_CMD_SET_DDRAM_ADDR | (uint8_t)(row_offset[y] + x));
}

display_status_t displayStringWrite(const char *str)
{
    display_status_t st = DISPLAY_OK;

    if (str == NULL) {
        return DISPLAY_ERROR_PARAM;
    }
    while ((*str != '\0') && (st == DISPLAY_OK)) {
        st = lcd_write(LCD_DATA, (uint8_t)*str++);
    }
    return st;
}

display_status_t displayDataWrite(char data)
{
    return lcd_write(LCD_DATA, (uint8_t)data);
}

display_status_t displayClear(void)
{
    display_status_t st = lcd_write(LCD_INSTRUCTION, LCD_CMD_CLEAR_DISPLAY);

    HAL_Delay(2);   /* Clear Display tarda hasta 1.52 ms */
    return st;
}

display_status_t displayBacklightSet(bool on)
{
    uint8_t port;

    lcd.backlight = on ? PCF_BACKLIGHT : 0u;
    port = lcd.backlight;
    return pcf_write(&port, 1u);
}
