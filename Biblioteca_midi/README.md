# midi: salida MIDI DIN no bloqueante por UART

Biblioteca para mandar mensajes MIDI a 31250 bps desde un STM32 con HAL, pensada para proyectos hechos con CubeMX y ejecutores cíclicos.
Son solo dos archivos y dependen únicamente del HAL: `HAL_UART_Transmit_IT` y el callback de fin de transmisión.

Las funciones de envío nunca esperan a la UART: copian el mensaje a una cola circular y vuelven en pocos microsegundos.
La interrupción de la USART va sacando la cola por el cable.

## Cómo integrarlo en un proyecto nuevo

1. Copiá `midi.h` a tu carpeta de includes (por ejemplo `app/inc`) y `midi.c` a la de fuentes (por ejemplo `app/src`).
2. En CubeMX configurá la USART:
   - **Mode:** Asynchronous, sin control de flujo.
   - **Parameter Settings:** Baud Rate = 31250, 8 bits, sin paridad, 1 bit de stop. Direction puede ser *Transmit Only*.
   - **NVIC Settings:** habilitá *USARTx global interrupt*. Sin esto la cola nunca avanza.

   Eso genera `MX_USARTx_UART_Init()`, el `HAL_UART_MspInit()`, habilita `HAL_UART_MODULE_ENABLED` y agrega el handler `USARTx_IRQHandler()` en `stm32f1xx_it.c`.
   - En la NUCLEO-F103RB, USART3 TX está en **PB10** (pin D6 de Arduino) sin remapeo.
3. Inicializá la biblioteca después de `MX_USARTx_UART_Init()`:

```c
#include "midi.h"
extern UART_HandleTypeDef huart3;

midi_config_t cfg = MIDI_CONFIG_DEFAULT(&huart3);   // canal 1
// o bien, explícito:
// midi_config_t cfg = { .huart = &huart3, .channel = 10 };

if (midi_init(&cfg) != MIDI_OK) {
    // configuración inválida
}

midi_send_note_on(60, 100);   // Do central, velocity 100
// ...
midi_send_note_off(60);
```

4. Conectá el callback del HAL. CubeMX no lo genera: el HAL trae una versión `__weak` vacía y vos la redefinís en cualquier `.c`, por ejemplo en `main.c` dentro de `/* USER CODE BEGIN 4 */`:

```c
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    midi_uart_tx_cplt_callback(huart);   // ignora las otras UART
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)   // opcional
{
    midi_uart_error_callback(huart);
}
```

Si tu proyecto ya define `HAL_UART_TxCpltCallback()` para otra UART, agregá la llamada adentro de esa función en vez de definirla dos veces.

## API

| Función | Qué hace |
| --- | --- |
| `midi_init(&cfg)` | Guarda el handle y el canal, y vacía la cola. No toca la UART. |
| `midi_set_channel(ch)` | Cambia el canal (1 a 16) para los mensajes siguientes. |
| `midi_send_note_on(note, velocity)` | Encola un Note On (`0x9n`). `note` y `velocity` van de 0 a 127. |
| `midi_send_note_off(note)` | Encola un Note Off (`0x8n`) con velocity 0. |
| `midi_all_notes_off()` | Manda Note Off por cada nota que quedó encendida y después el Control Change 123 (All Notes Off). |
| `midi_send_control_change(cc, value)` | Encola un Control Change (`0xBn`). |
| `midi_send_program_change(program)` | Encola un Program Change (`0xCn`). |
| `midi_send_raw(data, len)` | Encola bytes crudos, por ejemplo un SysEx. Entra el bloque entero o nada. |
| `midi_is_idle()` | `true` si la cola está vacía y la UART terminó. |
| `midi_get_free_space()` | Bytes libres en la cola. |
| `midi_get_dropped_count()` | Mensajes descartados por cola llena desde `midi_init()`. |

Todas las funciones de envío devuelven un `midi_status_t`: `MIDI_OK`, `MIDI_ERROR_PARAM`, `MIDI_ERROR_NOT_INIT`, `MIDI_ERROR_FULL` o `MIDI_ERROR_UART`.
Con `MIDI_ERROR_FULL` el mensaje se descarta completo, así nunca sale un mensaje cortado por la mitad.

## Notas

- **Tiempos en el cable.** Cada byte tarda 320 µs (10 bits a 31250 bps), así que un Note On de 3 bytes ocupa la línea casi 1 ms.
  En un cambio de paso del sequencer (Note Off + Note On) son unos 1,9 ms; la cola lo absorbe y el tick de 1 ms no se ve afectado.
- **Cola.** Por defecto tiene 128 bytes, unos 42 mensajes de 3 bytes. Para cambiarla, definí `MIDI_TX_BUFFER_SIZE` (potencia de 2) en las opciones del compilador.
  La biblioteca ocupa unos 960 bytes de Flash y 160 de RAM con el tamaño por defecto.
- **Concurrencia.** Las funciones de envío están pensadas para el contexto principal (las tareas del ejecutor cíclico).
  Las interrupciones se deshabilitan solo unos microsegundos al actualizar la cola. No las llames desde otra ISR.
- **All Notes Off.** La biblioteca recuerda qué notas mandó con Note On y les manda Note Off una por una, porque hay sintetizadores que ignoran el CC 123.
- **Velocity 0.** `midi_send_note_on(note, 0)` es un Note Off según la norma MIDI, y la biblioteca lo registra así.
- **Hardware de salida (MIDI 1.0 a 3,3 V).** TX (PB10) → resistencia de 10 Ω → pin 5 del DIN. +3,3 V → resistencia de 33 Ω → pin 4 del DIN. Pin 2 a GND (malla).
  Para 5 V la norma usa 220 Ω en ambos, pero el pin de la placa trabaja a 3,3 V.
