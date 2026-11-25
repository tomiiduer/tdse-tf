/**
 * @file    midi.h
 * @brief   Salida MIDI (MIDI DIN, 31250 bps) no bloqueante sobre una UART
 *          del STM32, con cola circular de transmision por interrupcion.
 *
 * Dependencias: STM32 HAL con el modulo UART habilitado
 * (HAL_UART_MODULE_ENABLED), un UART_HandleTypeDef ya inicializado a
 * 31250 bps 8N1 (por ejemplo con MX_USART3_UART_Init() de CubeMX) y la
 * interrupcion global de esa USART habilitada en el NVIC. No depende de
 * ningun otro modulo del proyecto.
 *
 * Uso minimo:
 * @code
 *   #include "midi.h"
 *   extern UART_HandleTypeDef huart3;
 *
 *   midi_config_t cfg = MIDI_CONFIG_DEFAULT(&huart3);   // canal 1
 *   if (midi_init(&cfg) != MIDI_OK) { ... }
 *
 *   midi_send_note_on(60, 100);   // Do central
 *   ...
 *   midi_send_note_off(60);
 *
 *   // en algun .c del proyecto (por ejemplo main.c):
 *   void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
 *   {
 *       midi_uart_tx_cplt_callback(huart);
 *   }
 * @endcode
 *
 * Tiempos: las funciones de envio solo copian los bytes a la cola (unos
 * pocos us) y vuelven enseguida; nunca esperan a la UART. Cada byte MIDI
 * tarda 320 us en salir por el cable (10 bits a 31250 bps), asi que un
 * Note On de 3 bytes ocupa la linea ~0.96 ms.
 */

#ifndef MIDI_H_
#define MIDI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "main.h"   /* trae el HAL de la familia STM32 del proyecto */

/********************** configuracion ****************************************/

/** Velocidad estandar de MIDI DIN. La UART debe estar configurada asi. */
#define MIDI_BAUDRATE               31250u

/**
 * Tamano de la cola de transmision en bytes. Debe ser potencia de 2.
 * Redefinible en las opciones del compilador (-DMIDI_TX_BUFFER_SIZE=256).
 */
#ifndef MIDI_TX_BUFFER_SIZE
#define MIDI_TX_BUFFER_SIZE         128u
#endif

/** Velocidad (velocity) que usan los Note Off que envia la biblioteca. */
#ifndef MIDI_NOTE_OFF_VELOCITY
#define MIDI_NOTE_OFF_VELOCITY      0u
#endif

/********************** tipos ************************************************/

typedef enum {
    MIDI_OK = 0,
    MIDI_ERROR_PARAM,       /**< configuracion o argumento fuera de rango */
    MIDI_ERROR_NOT_INIT,    /**< no se llamo a midi_init() */
    MIDI_ERROR_FULL,        /**< la cola no tiene lugar; el mensaje se descarto entero */
    MIDI_ERROR_UART,        /**< HAL_UART_Transmit_IT() devolvio error */
} midi_status_t;

typedef struct {
    UART_HandleTypeDef *huart;  /**< UART ya inicializada a 31250 bps 8N1 */
    uint8_t channel;            /**< canal MIDI 1..16 (como lo muestra un sintetizador) */
} midi_config_t;

/** Configuracion por defecto: canal 1. */
#define MIDI_CONFIG_DEFAULT(p_huart) \
    { .huart = (p_huart), .channel = 1u }

/********************** API **************************************************/

/**
 * @brief  Inicializa la biblioteca y vacia la cola. No toca la UART.
 * @param  config  configuracion (se copia; no hace falta mantenerla viva).
 */
midi_status_t midi_init(const midi_config_t *config);

/** Cambia el canal MIDI (1..16) de los mensajes siguientes. */
midi_status_t midi_set_channel(uint8_t channel);

/** Encola un Note On. note y velocity van de 0 a 127. */
midi_status_t midi_send_note_on(uint8_t note, uint8_t velocity);

/** Encola un Note Off. note va de 0 a 127. */
midi_status_t midi_send_note_off(uint8_t note);

/**
 * @brief  Apaga todo lo que este sonando: envia Note Off por cada nota que
 *         la biblioteca registro como encendida y luego el Control Change
 *         123 (All Notes Off) del canal actual.
 */
midi_status_t midi_all_notes_off(void);

/** Encola un Control Change. controller y value van de 0 a 127. */
midi_status_t midi_send_control_change(uint8_t controller, uint8_t value);

/** Encola un Program Change. program va de 0 a 127. */
midi_status_t midi_send_program_change(uint8_t program);

/**
 * @brief  Encola bytes crudos (por ejemplo un SysEx). El bloque se encola
 *         completo o no se encola nada.
 */
midi_status_t midi_send_raw(const uint8_t *data, uint16_t length);

/** true si la cola esta vacia y la UART termino de transmitir. */
bool midi_is_idle(void);

/** Bytes libres en la cola. */
uint16_t midi_get_free_space(void);

/** Cantidad de mensajes descartados por cola llena desde midi_init(). */
uint32_t midi_get_dropped_count(void);

/**
 * @brief  Llamar desde HAL_UART_TxCpltCallback(). Ignora otras UART, asi
 *         que se puede llamar siempre, sin filtrar por instancia.
 */
void midi_uart_tx_cplt_callback(UART_HandleTypeDef *huart);

/**
 * @brief  Opcional: llamar desde HAL_UART_ErrorCallback(). Si la UART
 *         aborta una transmision, descarta ese bloque y retoma con el
 *         siguiente para que la cola no quede trabada.
 */
void midi_uart_error_callback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_H_ */
