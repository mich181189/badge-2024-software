
// Local Headers
#include "dmx.h"

// ESP32 Headers
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sys/param.h"
//#include "py/runtime.h"

static const int UART_BUFFER_SIZE = 2048;

static uart_config_t dmx_uart_config = {
    .baud_rate = 250000,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_2,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    
};

enum dmx_state {
    WAIT_BREAK,
    WAIT_START_CODE,
    RECEIVING_CHANNELS,
};

struct dmx_struct_t {
    bool is_running;
    TaskHandle_t dmx_task;
    enum dmx_state state;
    
    int uart;
    QueueHandle_t eventQueue;
    StaticSemaphore_t quitSemData;
    SemaphoreHandle_t quitSem;
    
    uint8_t channel_data[512];
    size_t current_channel;
};

static void dmx_task(void *params) {
    dmx_t* dmx = (dmx_t*)params;
    uart_event_t event;
    
    // Clear queue and UART so we start from a fresh state
    uart_flush_input(dmx->uart);
    xQueueReset(dmx->eventQueue);
    
    while(xQueueReceive(dmx->eventQueue, (void*)&event, portMAX_DELAY)) {
        switch(event.type) {
            case UART_DATA: {
                switch(dmx->state) {
                    case WAIT_BREAK: {
                        // Just dump the buffer
                        uart_flush_input(dmx->uart);
                    } break;
                    case WAIT_START_CODE: {
                        uint8_t startCode;
                        uart_read_bytes(dmx->uart, &startCode, 1, 0);
                        if(startCode == 0) {
                            size_t toRead = MIN(event.size - 1, 512);
                            uart_read_bytes(dmx->uart, dmx->channel_data, toRead, 0);
                            dmx->current_channel += event.size-1;
                            dmx->state = RECEIVING_CHANNELS;
                        } else {
                            // Probably RDM data - dump the buffer
                            uart_flush_input(dmx->uart);
                        }
                    } break;
                    case RECEIVING_CHANNELS: {
                        if(dmx->current_channel >= 512) {
                            // Too much data - dump it
                            uart_flush_input(dmx->uart);
                            dmx->state = WAIT_BREAK;
                        }
                        size_t toRead = MIN(event.size, 512 - dmx->current_channel);
                        uart_read_bytes(dmx->uart, dmx->channel_data + dmx->current_channel, toRead, 0);
                        dmx->current_channel += event.size;
                    } break;
                };
            } break;
            case UART_BREAK: {
                // If we don't clear the input here, we get a stray zero
                uart_flush_input(dmx->uart);
                xQueueReset(dmx->eventQueue);
                dmx->state = WAIT_START_CODE;
            } break;
            case UART_BUFFER_FULL: {
                //mp_printf(&mp_plat_print,"UART(%d) is full - clearing to attempt recovery\n", dmx->uart);
                uart_flush_input(dmx->uart);
                xQueueReset(dmx->eventQueue);
            } break;
            default: {
                //mp_printf(&mp_plat_print,"uart[%d] event: %d\n", dmx->uart, event.type);
            }
        }
    }
    
    xSemaphoreGive(dmx->quitSem);
    vTaskDelete(NULL);
}

dmx_t* dmx_create(int uart, int tx_pin, int rx_pin, int rts_pin) {
    dmx_t* dmx = calloc(1, sizeof(dmx_t));
    dmx->uart = uart;
    esp_err_t res = uart_param_config(uart, &dmx_uart_config);
    if(res != ESP_OK) {
        //mp_printf(&mp_plat_print,"Error setting uart parameters on UART %d\n", uart);
        free(dmx);
        return NULL;
    }
    
    res = uart_set_pin(uart, tx_pin, rx_pin, rts_pin, UART_PIN_NO_CHANGE);
    if(res != ESP_OK) {
        //mp_printf(&mp_plat_print,"Error setting uart pins on UART %d\n", uart);
        free(dmx);
        return NULL;
    }
    
    res = uart_driver_install(uart, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 10, &dmx->eventQueue, 0);
    if(res != ESP_OK) {
        //mp_printf(&mp_plat_print,"Error installing driver for UART %d\n", uart);
        free(dmx);
        return NULL;
    }
    
    res = uart_set_mode(uart, UART_MODE_RS485_HALF_DUPLEX);
    if(res != ESP_OK) {
        //mp_printf(&mp_plat_print,"Error setting half duplex mode on UART %d\n", uart);
        free(dmx);
        return NULL;
    }
    
    dmx->quitSem = xSemaphoreCreateBinaryStatic(&dmx->quitSemData);
    return dmx;
}

void dmx_destroy(dmx_t* dmx) {
    if(dmx->is_running) {
        dmx_stop(dmx);
    }
    
    // Clean up UART
    uart_driver_delete(dmx->uart);
    // esp-idf will create this queue, but seems to leave it to us to destoy it:
    vQueueDelete(dmx->eventQueue);
    
    free(dmx);
}

void dmx_start(dmx_t* dmx) {
    xTaskCreate(dmx_task, "dmx512", 2048, dmx, tskIDLE_PRIORITY + 2, &dmx->dmx_task);
    dmx->is_running = true;
}

void dmx_stop(dmx_t* dmx) {
    if(!dmx->is_running) {
        return;
    }
    xQueueReset(dmx->eventQueue);
    xTaskAbortDelay(dmx->dmx_task);
    // Block until task exits
    //mp_printf(&mp_plat_print,"Waiting for DMX to quit\n");
    xSemaphoreTake(dmx->quitSem, portMAX_DELAY);
    //mp_printf(&mp_plat_print,"DMX Stopped\n");
    dmx->is_running = false;
}

uint8_t dmx_get_value(dmx_t* dmx, int channel) {
    if(!dmx) {
        return 0;
    }
    
    if(channel < 1 || channel > 512) {
        return 0;
    }
    
    return dmx->channel_data[channel-1];
}