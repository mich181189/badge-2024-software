// Local
#include "dmx.h"

// ESP-IDF
#include "driver/uart.h"

// Micropython
#include "py/runtime.h"
#include "py/mphal.h"

// To allow re-initialisation, we keep track of the DMX objects globally
static dmx_t* global_dmx_objs[UART_NUM_MAX] = { NULL };

typedef struct _mp_dmx_obj_t {
    mp_obj_base_t base;
    mp_int_t uart;
} mp_dmx_obj_t;

// DMX class methods
static mp_obj_t mp_dmx_make_new(const mp_obj_type_t *type, size_t n_args,
                                size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_UART,
        ARG_TX_PIN,
        ARG_RX_PIN,
        ARG_RTS_PIN,
    };
    
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uart, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = UART_NUM_0} },
        { MP_QSTR_tx, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL} },
        { MP_QSTR_rx, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL} },
        { MP_QSTR_rts, MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL} },
    };
    
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    mp_int_t uart_id = args[ARG_UART].u_int;
    if(uart_id < UART_NUM_0 || uart_id >= UART_NUM_MAX) {
        mp_printf(&mp_plat_print, "Error: Does not exist UART %d (of %d uarts)\n", uart_id, UART_NUM_MAX);
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("UART(%d) doesn't exist"), uart_id);
    } else {
        mp_printf(&mp_plat_print, "Creating DMX channel for UART %d (of %d uarts)\n", uart_id, UART_NUM_MAX);
    }
    
    int tx_pin = machine_pin_get_id(args[ARG_TX_PIN].u_obj);
    int rx_pin = machine_pin_get_id(args[ARG_RX_PIN].u_obj);
    int rts_pin = UART_PIN_NO_CHANGE;
    
    if(args[ARG_RTS_PIN].u_obj != MP_OBJ_NULL) {
        rts_pin = machine_pin_get_id(args[ARG_RTS_PIN].u_obj);
    }
    
    // If we have an instance already, reset it
    if(global_dmx_objs[uart_id]) {
        dmx_destroy(global_dmx_objs[uart_id]);
    }
    
    global_dmx_objs[uart_id] = dmx_create(uart_id, tx_pin, rx_pin, rts_pin);
    
    mp_dmx_obj_t* self = m_new_obj(mp_dmx_obj_t);
    self->base.type = type;
    self->uart = uart_id;
    
    if(!global_dmx_objs[self->uart]) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to create DMX instance for UART %d"), self->uart);
    }

    mp_printf(&mp_plat_print, "Done creating DMX channel for UART %d\n", uart_id);
    
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_dmx_get_channel(mp_obj_t self_in, mp_obj_t channel_in) {
    mp_dmx_obj_t* self = MP_OBJ_TO_PTR(self_in);
    mp_int_t channel = mp_obj_get_int(channel_in);
    
    if(channel < 1 || channel > 512) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("DMX Channel %d does not exist"), channel);
    }
    
    if(!global_dmx_objs[self->uart]) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("DMX Channel for uart %d does not exist"), self->uart);
    }
    return MP_OBJ_NEW_SMALL_INT(dmx_get_value(global_dmx_objs[self->uart], channel));
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_dmx_get_channel_obj, mp_dmx_get_channel);

static mp_obj_t mp_dmx_start(mp_obj_t self_in) {
    mp_dmx_obj_t* self = MP_OBJ_TO_PTR(self_in);
    
    if(global_dmx_objs[self->uart]) {
        dmx_start(global_dmx_objs[self->uart]);
    } else {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("DMX Channel for uart %d does not exist"), self->uart);
    }
    
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_dmx_start_obj, mp_dmx_start);

static mp_obj_t mp_dmx_stop(mp_obj_t self_in) {
    mp_dmx_obj_t* self = MP_OBJ_TO_PTR(self_in);
    
    if(global_dmx_objs[self->uart]) {
        dmx_stop(global_dmx_objs[self->uart]);
        // Also destroy since otherwise it will hang around forever
        dmx_destroy(global_dmx_objs[self->uart]);
        global_dmx_objs[self->uart] = NULL;
    } else {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("DMX Channel for uart %d does not exist"), self->uart);
    }
    
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_dmx_stop_obj, mp_dmx_stop);

// Dmx class type
static const mp_rom_map_elem_t mp_dmx_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_UART_0), MP_ROM_INT(UART_NUM_0)},
    { MP_ROM_QSTR(MP_QSTR_UART_1), MP_ROM_INT(UART_NUM_1)},
    { MP_ROM_QSTR(MP_QSTR_UART_2), MP_ROM_INT(UART_NUM_2)},
    
    { MP_ROM_QSTR(MP_QSTR_get_channel), MP_ROM_PTR(&mp_dmx_get_channel_obj) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&mp_dmx_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&mp_dmx_stop_obj) },
};
static MP_DEFINE_CONST_DICT(mp_dmx_locals_dict, mp_dmx_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_dmx,
    MP_QSTR_dmx_type,
    MP_TYPE_FLAG_NONE,
    make_new, mp_dmx_make_new,
    locals_dict, &mp_dmx_locals_dict
);

// Module Table and definition
static const mp_rom_map_elem_t mp_module_dmx_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_dmx) },
    { MP_ROM_QSTR(MP_QSTR_Dmx), MP_ROM_PTR(&mp_type_dmx) },
};
static MP_DEFINE_CONST_DICT(mp_module_dmx_globals, mp_module_dmx_globals_table);

const mp_obj_module_t mp_module_dmx = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_dmx_globals,
};

MP_REGISTER_MODULE(MP_QSTR_dmx, mp_module_dmx);