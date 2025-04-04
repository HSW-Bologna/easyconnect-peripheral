#include <sys/time.h>
#include <assert.h>
#include "minion.h"
#include "config/app_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "utils/utils.h"
#include "freertos/projdefs.h"
#include "peripherals/hardwareprofile.h"
#include "lightmodbus/base.h"
#include "lightmodbus/lightmodbus.h"
#include "lightmodbus/slave.h"
#include "lightmodbus/slave_func.h"
#include <stdio.h>
#include <stdlib.h>
#include "configuration.h"
#include "peripherals/digout.h"
#include "peripherals/digin.h"
#include "peripherals/rs485.h"
#include "easyconnect.h"
#include "model/model.h"
#include "rele.h"
#include "safety.h"
#include "config/app_config.h"
#include "gel/serializer/serializer.h"
#include "gel/timer/timecheck.h"
#include "event_log.h"
#include "model/model.h"


#define HOLDING_REGISTER_SAFETY_MESSAGE   EASYCONNECT_HOLDING_REGISTER_MESSAGE_1
#define HOLDING_REGISTER_FEEDBACK_MESSAGE (HOLDING_REGISTER_SAFETY_MESSAGE + EASYCONNECT_MESSAGE_NUM_REGISTERS)

#define HOLDING_REGISTER_WORK_HOURS EASYCONNECT_HOLDING_REGISTER_CUSTOM_START

#define COIL_RELE_STATE    0
#define COIL_SAFETY_BYPASS 1

static const char   *TAG = "Minion";
static ModbusSlave   minion;
static unsigned long timestamp = 0;

static ModbusError           register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                                               ModbusRegisterCallbackResult *result);
static ModbusError           exception_callback(const ModbusSlave *minion, uint8_t function, ModbusExceptionCode code);
static LIGHTMODBUS_RET_ERROR initialization_function(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                     uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR set_class_output(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                              uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR set_datetime(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                          uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR heartbeat_received(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                uint8_t requestLength);


static const ModbusSlaveFunctionHandler custom_functions[] = {
#if defined(LIGHTMODBUS_F01S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {1, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F02S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {2, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F03S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {3, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F04S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {4, modbusParseRequest01020304},
#endif
#if defined(LIGHTMODBUS_F05S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {5, modbusParseRequest0506},
#endif
#if defined(LIGHTMODBUS_F06S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {6, modbusParseRequest0506},
#endif
#if defined(LIGHTMODBUS_F15S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {15, modbusParseRequest1516},
#endif
#if defined(LIGHTMODBUS_F16S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {16, modbusParseRequest1516},
#endif
#if defined(LIGHTMODBUS_F22S) || defined(LIGHTMODBUS_SLAVE_FULL)
    {22, modbusParseRequest22},
#endif

    {EASYCONNECT_FUNCTION_CODE_CONFIG_ADDRESS, easyconnect_set_address_function},
    {EASYCONNECT_FUNCTION_CODE_RANDOM_SERIAL_NUMBER, easyconnect_send_address_function},
    {EASYCONNECT_FUNCTION_CODE_SET_TIME, set_datetime},
    {EASYCONNECT_FUNCTION_CODE_HEARTBEAT, heartbeat_received},
    {EASYCONNECT_FUNCTION_CODE_NETWORK_INITIALIZATION, initialization_function},
    {EASYCONNECT_FUNCTION_CODE_SET_CLASS_OUTPUT, set_class_output},

    // Guard - prevents 0 array size
    {0, NULL},
};


void minion_init(easyconnect_interface_t *context) {
    ModbusErrorInfo err;
    err = modbusSlaveInit(&minion,
                          register_callback,          // Callback for register operations
                          exception_callback,         // Callback for handling minion exceptions (optional)
                          modbusDefaultAllocator,     // Memory allocator for allocating responses
                          custom_functions,           // Set of supported functions
                          15                          // Number of supported functions
    );

    // Check for errors
    assert(modbusIsOk(err) && "modbusSlaveInit() failed");

    modbusSlaveSetUserPointer(&minion, context);

    timestamp = get_millis();
}


void minion_manage(void) {
    uint8_t buffer[256] = {0};
    int     len         = rs485_read(buffer, 256);

    easyconnect_interface_t *context = modbusSlaveGetUserPointer(&minion);

    if (len > 0) {
        /*
        printf("Read %i bytes\n", len);
        for (uint16_t i = 0; i < len;i++){ 
            printf("0x%X ", buffer[i]);
        }
        printf("\n");
        */
        //ESP_LOG_BUFFER_HEX(TAG, buffer, len);

        ModbusErrorInfo err;
        err = modbusParseRequestRTU(&minion, context->get_address(context->arg), buffer, len);

        if (modbusIsOk(err)) {
            size_t rlen = modbusSlaveGetResponseLength(&minion);
            if (rlen > 0) {
                //printf("responding with %i bytes\n", rlen);
                rs485_write((uint8_t *)modbusSlaveGetResponse(&minion), rlen);
            } else {
                ESP_LOGD(TAG, "Empty response");
            }
        } else if (err.error != MODBUS_ERROR_ADDRESS) {
            ESP_LOGW(TAG, "Invalid request with source %i and error %i", err.source, err.error);
            ESP_LOG_BUFFER_HEX(TAG, buffer, len);
        }
    }

    if (is_expired(timestamp, get_millis(), EASYCONNECT_HEARTBEAT_TIMEOUT)) {
        if (model_get_missing_heartbeat(context->arg) == 0) {
            model_set_missing_heartbeat(context->arg, 1);
            rele_refresh(context->arg);
        }
    }
}


ModbusError register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                              ModbusRegisterCallbackResult *result) {

    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(status);
    result->value                = 0;

    switch (args->query) {
        // R/W access check
        case MODBUS_REGQ_R_CHECK:
            result->exceptionCode = MODBUS_EXCEP_NONE;
            break;

        case MODBUS_REGQ_W_CHECK:
            result->exceptionCode = MODBUS_EXCEP_NONE;

            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                        case EASYCONNECT_HOLDING_REGISTER_CLASS:
                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_1:
                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_2:
                        case HOLDING_REGISTER_WORK_HOURS:
                            break;

                        default:
                            result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
                            break;
                    }
                    break;
                }
                case MODBUS_INPUT_REGISTER:
                    result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
                    break;
                case MODBUS_DISCRETE_INPUT:
                    result->exceptionCode = MODBUS_EXCEP_ILLEGAL_FUNCTION;
                    break;
                default:
                    break;
            }
            break;

        // Read register
        case MODBUS_REGQ_R:
            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                            result->value = ctx->get_address(ctx->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_FIRMWARE_VERSION:
                            result->value = EASYCONNECT_FIRMWARE_VERSION(APP_CONFIG_FIRMWARE_VERSION_MAJOR,
                                                                         APP_CONFIG_FIRMWARE_VERSION_MINOR,
                                                                         APP_CONFIG_FIRMWARE_VERSION_PATCH);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_CLASS:
                            result->value = ctx->get_class(ctx->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_1:
                            result->value = (ctx->get_serial_number(ctx->arg) >> 16) & 0xFFFF;
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_2:
                            result->value = ctx->get_serial_number(ctx->arg) & 0xFFFF;
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_ALARMS:
                            if (!safety_ok()) {
                                result->value |= 0x01;
                            }
                            if (model_get_output_attempts_exceeded(ctx->arg)) {
                                result->value |= 0x02;
                            }
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_STATE:
                            result->value = rele_is_on();
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_LOGS_COUNTER:
                            result->value = event_log_get_count();
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_LOGS ... HOLDING_REGISTER_SAFETY_MESSAGE - 1: {
                            size_t event_index =
                                (args->index - EASYCONNECT_HOLDING_REGISTER_LOGS) / EVENT_LOG_SERIALIZED_SIZE;
                            uint8_t buffer[EVENT_LOG_SERIALIZED_SIZE] = {0};
                            event_log_serialize_event(buffer, event_index);

                            size_t buffer_index = (args->index - EASYCONNECT_HOLDING_REGISTER_LOGS) % 8;
                            result->value       = (buffer[buffer_index] << 8) | buffer[buffer_index + 1];

                            break;
                        }

                        case HOLDING_REGISTER_SAFETY_MESSAGE ... HOLDING_REGISTER_FEEDBACK_MESSAGE - 1: {
                            char msg[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
                            model_get_safety_message(ctx->arg, msg);
                            size_t i      = args->index - HOLDING_REGISTER_SAFETY_MESSAGE;
                            result->value = msg[i * 2] << 8 | msg[i * 2 + 1];
                            break;
                        }

                        case HOLDING_REGISTER_FEEDBACK_MESSAGE ... HOLDING_REGISTER_FEEDBACK_MESSAGE +
                            EASYCONNECT_MESSAGE_NUM_REGISTERS - 1: {
                            char msg[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
                            model_get_feedback_message(ctx->arg, msg);
                            size_t i      = args->index - HOLDING_REGISTER_SAFETY_MESSAGE;
                            result->value = msg[i] << 8 | msg[i];
                            break;
                        }

                        case HOLDING_REGISTER_WORK_HOURS:
                            result->value = model_get_work_hours(ctx->arg);
                            break;
                    }
                    break;
                }
                case MODBUS_INPUT_REGISTER:
                    break;
                case MODBUS_COIL:
                    result->value = digout_get();
                    break;
                case MODBUS_DISCRETE_INPUT:
                    result->value = digin_get(args->index);
                    break;
            }
            break;

        // Write register
        case MODBUS_REGQ_W:
            switch (args->type) {
                case MODBUS_HOLDING_REGISTER: {
                    switch (args->index) {
                        case EASYCONNECT_HOLDING_REGISTER_ADDRESS:
                            ctx->save_address(ctx->arg, args->value);
                            break;
                        case EASYCONNECT_HOLDING_REGISTER_CLASS:
                            ctx->save_class(ctx->arg, args->value);
                            break;
                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_1: {
                            uint32_t current_serial_number = ctx->get_serial_number(ctx->arg);
                            ctx->save_serial_number(ctx->arg, (args->value << 16) | (current_serial_number & 0xFFFF));
                            break;
                        }
                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER_2: {
                            uint32_t current_serial_number = ctx->get_serial_number(ctx->arg);
                            ctx->save_serial_number(ctx->arg, args->value | (current_serial_number & 0xFFFF0000));
                            break;
                        }
                        case HOLDING_REGISTER_WORK_HOURS:
                            model_reset_work_seconds(ctx->arg);
                            break;
                    }
                    break;
                }

                case MODBUS_COIL:
                    switch (args->index) {
                        case COIL_RELE_STATE:
                            // printf("state coil %i\n", args->value);
                            if (rele_update(ctx->arg, args->value)) {
                                result->exceptionCode = MODBUS_EXCEP_SLAVE_FAILURE;
                            }
                            break;

                        case COIL_SAFETY_BYPASS:
                            model_set_safety_bypass(ctx->arg, args->value);
                            // printf("coil %i\n", model_get_safety_bypass(ctx->arg));
                            break;

                        default:
                            break;
                    }
                    break;

                case MODBUS_INPUT_REGISTER:
                    break;

                default:
                    break;
            }
            break;
    }

    // Always return MODBUS_OK
    return MODBUS_OK;
}


static ModbusError exception_callback(const ModbusSlave *minion, uint8_t function, ModbusExceptionCode code) {
    ESP_LOGI(TAG, "Slave reports an exception %d (function %d)\n", code, function);
    // Always return MODBUS_OK
    return MODBUS_OK;
}


static LIGHTMODBUS_RET_ERROR initialization_function(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                     uint8_t requestLength) {
    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);
    rele_update(ctx->arg, 0);
    ESP_LOGI(TAG, "rele off");
    return MODBUS_NO_ERROR();
}


static LIGHTMODBUS_RET_ERROR set_class_output(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                              uint8_t requestLength) {
    // Check request length
    if (requestLength < 4) {
        return modbusBuildException(minion, function, MODBUS_EXCEP_ILLEGAL_VALUE);
    }

    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);
    uint16_t class               = requestPDU[1] << 8 | requestPDU[2];
    uint8_t safety_bypass        = requestPDU[4];
    model_set_safety_bypass(ctx->arg, safety_bypass);
    // printf("class %i\n", safety_bypass);
    if (class == ctx->get_class(ctx->arg)) {
        rele_update(ctx->arg, requestPDU[3]);
    }

    return MODBUS_NO_ERROR();
}


static LIGHTMODBUS_RET_ERROR heartbeat_received(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                uint8_t requestLength) {
    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);
    ESP_LOGD(TAG, "Heartbeat");

    timestamp = get_millis();
    model_set_missing_heartbeat(ctx->arg, 0);
    rele_refresh(ctx->arg);
    return MODBUS_NO_ERROR();
}


static LIGHTMODBUS_RET_ERROR set_datetime(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                          uint8_t requestLength) {
    // Check request length
    if (requestLength < 8) {
        return modbusBuildException(minion, function, MODBUS_EXCEP_ILLEGAL_VALUE);
    }

    uint64_t timestamp = 0;
    deserialize_uint64_be(&timestamp, (uint8_t *)requestPDU);

    struct timeval timeval = {0};
    timeval.tv_sec         = timestamp;

    settimeofday(&timeval, NULL);

    return MODBUS_NO_ERROR();
}
