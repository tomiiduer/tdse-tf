# display_i2c: LCD HD44780 por I2C (PCF8574)

Driver para displays de caracteres 16x2 o 20x4 con backpack I2C PCF8574 o PCF8574A, pensado para proyectos STM32 HAL hechos con CubeMX.
Son solo dos archivos y dependen únicamente del HAL: `HAL_I2C_Master_Transmit`, `HAL_I2C_IsDeviceReady` y `HAL_Delay`.

## Cómo integrarlo en un proyecto nuevo

1. Copiá `display.h` a tu carpeta de includes (por ejemplo `app/inc`) y `display.c` a la de fuentes (por ejemplo `app/src`).
2. En CubeMX habilitá un I2C en Standard (100 kHz) o Fast mode (400 kHz).
   Eso genera `MX_I2Cx_Init()`, el `HAL_I2C_MspInit()`, habilita `HAL_I2C_MODULE_ENABLED` y agrega `stm32f1xx_hal_i2c.c`.
   - En la NUCLEO-F103RB, los pines de Arduino D14/D15 son I2C1 remapeado: PB9 = SDA y PB8 = SCL.
3. Inicializá el display después de `MX_I2Cx_Init()`:

```c
#include "display.h"
extern I2C_HandleTypeDef hi2c1;

display_config_t cfg = DISPLAY_CONFIG_DEFAULT(&hi2c1);   // 16x2, dirección automática
// o bien, explícito:
// display_config_t cfg = { .hi2c = &hi2c1, .address = 0x27, .columns = 20, .rows = 4 };

if (displayInit(&cfg) != DISPLAY_OK) {
    // no responde: revisar SDA/SCL, alimentación y dirección
}

displayCharPositionWrite(0, 0);
displayStringWrite("Hola mundo");
```

## API

| Función | Qué hace |
| --- | --- |
| `displayInit(&cfg)` | Inicializa en 4 bits, display encendido, cursor apagado y luz de fondo encendida. Tarda unos 60 ms. |
| `displayCharPositionWrite(x, y)` | Mueve el cursor a la columna `x` y la fila `y`, contando desde 0. |
| `displayStringWrite(str)` | Escribe una cadena terminada en `'\0'`. |
| `displayDataWrite(c)` | Escribe un carácter. |
| `displayClear()` | Borra la pantalla. Tarda unos 2 ms. |
| `displayBacklightSet(on)` | Prende o apaga la luz de fondo. |
| `displayGetAddress()` | Devuelve la dirección de 7 bits en uso. |

Todas las funciones devuelven un `display_status_t`: `DISPLAY_OK`, `DISPLAY_ERROR_PARAM`, `DISPLAY_ERROR_NOT_FOUND` o `DISPLAY_ERROR_BUS`.

## Notas

- **Dirección.** Con `DISPLAY_I2C_ADDR_AUTO`, el driver prueba primero 0x27 (PCF8574T) y después 0x3F (PCF8574AT).
  Si soldaste los puentes A0..A2 del backpack, pasá la dirección a mano.
- **Tiempos.** Cada carácter o comando es una única transferencia I2C de 6 bytes, que es bloqueante:
  unos 140 µs a 400 kHz o unos 550 µs a 100 kHz.
  En un ejecutor cíclico de 1 ms conviene escribir un carácter por tick, como hace `task_display`.
- **Velocidad del bus.** Algunos PCF8574 están especificados solo hasta 100 kHz.
  Si a 400 kHz aparecen caracteres corruptos, bajá la velocidad.
- **Timeout.** El timeout de I2C por defecto es de 2 ms.
  Para cambiarlo, definí `DISPLAY_I2C_TIMEOUT_MS` en las opciones del compilador.
- **Pines del backpack.** P0 = RS, P1 = RW, P2 = EN, P3 = luz de fondo, P4..P7 = D4..D7.
  Es el mapeo del módulo LCM1602 / HW-061, que es el más común.
