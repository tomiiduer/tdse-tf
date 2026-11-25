/**
 * @file    display.h
 * @brief   Driver para LCD de caracteres HD44780 (16x2, 20x4, ...) conectado
 *          por I2C a traves de un backpack PCF8574 / PCF8574A.
 *
 * Dependencias: STM32 HAL con el modulo I2C habilitado
 * (HAL_I2C_MODULE_ENABLED) y un I2C_HandleTypeDef ya inicializado
 * (por ejemplo con MX_I2C1_Init() de CubeMX). No depende de ningun otro
 * modulo del proyecto.
 *
 * Uso minimo:
 * @code
 *   #include "display.h"
 *   extern I2C_HandleTypeDef hi2c1;
 *
 *   display_config_t cfg = DISPLAY_CONFIG_DEFAULT(&hi2c1);
 *   if (displayInit(&cfg) != DISPLAY_OK) { ... }
 *
 *   displayCharPositionWrite(0, 0);
 *   displayStringWrite("Hola mundo");
 * @endcode
 *
 * Tiempos: cada caracter o comando es una transaccion I2C de 6 bytes
 * (~140 us a 400 kHz, ~550 us a 100 kHz) y la funcion bloquea durante ese
 * tiempo. displayInit() y displayClear() usan HAL_Delay().
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "main.h"   /* trae el HAL de la familia STM32 del proyecto */

/********************** configuracion ****************************************/

/** Direcciones de 7 bits mas comunes del backpack (A2..A0 = 111). */
#define DISPLAY_I2C_ADDR_PCF8574    0x27u   /**< PCF8574T  */
#define DISPLAY_I2C_ADDR_PCF8574A   0x3Fu   /**< PCF8574AT */

/** Pasar como address para que displayInit() pruebe 0x27 y luego 0x3F. */
#define DISPLAY_I2C_ADDR_AUTO       0x00u

/** Timeout de cada transferencia I2C, en ms. Redefinible antes del include. */
#ifndef DISPLAY_I2C_TIMEOUT_MS
#define DISPLAY_I2C_TIMEOUT_MS      2u
#endif

/********************** tipos ************************************************/

typedef enum {
    DISPLAY_OK = 0,
    DISPLAY_ERROR_PARAM,        /**< configuracion invalida */
    DISPLAY_ERROR_NOT_FOUND,    /**< nadie responde en la direccion I2C */
    DISPLAY_ERROR_BUS,          /**< fallo una transferencia I2C */
} display_status_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;    /**< bus I2C ya inicializado */
    uint8_t address;            /**< direccion 7 bits o DISPLAY_I2C_ADDR_AUTO */
    uint8_t columns;            /**< 16 o 20 */
    uint8_t rows;               /**< 1, 2 o 4 */
} display_config_t;

/** Configuracion por defecto: LCD 16x2 con autodeteccion de direccion. */
#define DISPLAY_CONFIG_DEFAULT(p_hi2c) \
    { .hi2c = (p_hi2c), .address = DISPLAY_I2C_ADDR_AUTO, .columns = 16u, .rows = 2u }

/********************** API **************************************************/

/**
 * @brief  Inicializa el LCD en modo 4 bits, 5x8, display ON, cursor OFF,
 *         pantalla borrada y luz de fondo encendida. Tarda ~60 ms.
 * @param  config  configuracion (se copia; no hace falta mantenerla viva).
 * @retval DISPLAY_OK si el modulo respondio.
 */
display_status_t displayInit(const display_config_t *config);

/** Direccion I2C de 7 bits en uso (util con DISPLAY_I2C_ADDR_AUTO). */
uint8_t displayGetAddress(void);

/** Mueve el cursor a la columna x (0..columns-1) y fila y (0..rows-1). */
display_status_t displayCharPositionWrite(uint8_t x, uint8_t y);

/** Escribe una cadena terminada en '\0' desde la posicion actual. */
display_status_t displayStringWrite(const char *str);

/** Escribe un solo caracter en la posicion actual. */
display_status_t displayDataWrite(char data);

/** Borra la pantalla y vuelve el cursor a (0,0). Bloquea ~2 ms. */
display_status_t displayClear(void);

/** Enciende (true) o apaga (false) la luz de fondo. */
display_status_t displayBacklightSet(bool on);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_H_ */
