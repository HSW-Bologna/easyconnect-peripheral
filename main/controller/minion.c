#include <assert.h>
#include "minion.h"
#include "config/app_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
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


#define HOLDING_REGISTER_SAFETY_MESSAGE   EASYCONNECT_HOLDING_REGISTER_MESSAGE_1
#define HOLDING_REGISTER_FEEDBACK_MESSAGE (HOLDING_REGISTER_SAFETY_MESSAGE + EASYCONNECT_MESSAGE_NUM_REGISTERS)


static const char *TAG = "Minion";
ModbusSlave        minion;

static ModbusError           register_callback(const ModbusSlave *status, const ModbusRegisterCallbackArgs *args,
                                               ModbusRegisterCallbackResult *result);
static ModbusError           exception_callback(const ModbusSlave *minion, uint8_t function, ModbusExceptionCode code);
static LIGHTMODBUS_RET_ERROR initialization_function(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
                                                     uint8_t requestLength);
static LIGHTMODBUS_RET_ERROR set_class_output(ModbusSlave *minion, uint8_t function, const uint8_t *requestPDU,
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
                          13                          // Number of supported functions
    );

    // Check for errors
    assert(modbusIsOk(err) && "modbusSlaveInit() failed");

    modbusSlaveSetUserPointer(&minion, context);

    /*uint8_t mac_address[6] = {0};
    esp_read_mac(mac_address, ESP_MAC_WIFI_STA);
    unsigned int mac = mac_address[2] | mac_address[3] << 8 | mac_address[4] << 16 | mac_address[5] << 24;
    ESP_LOGI(TAG, "MAC Address: %i", mac);
    srand(mac);*/
}


void minion_manage(void) {
    uint8_t buffer[256] = {0};
    int     len         = rs485_read(buffer, 256);

    easyconnect_interface_t *context = modbusSlaveGetUserPointer(&minion);

    if (len > 0) {
        // ESP_LOG_BUFFER_HEX(TAG, buffer, len);

        ModbusErrorInfo err;
        err = modbusParseRequestRTU(&minion, context->get_address(context->arg), buffer, len);

        if (modbusIsOk(err)) {
            size_t rlen = modbusSlaveGetResponseLength(&minion);
            if (rlen > 0) {
                rs485_write((uint8_t *)modbusSlaveGetResponse(&minion), rlen);
            } else {
                ESP_LOGI(TAG, "Empty response");
            }
        } else if (err.error != MODBUS_ERROR_ADDRESS) {
            ESP_LOGW(TAG, "Invalid request with source %i and error %i", err.source, err.error);
            ESP_LOG_BUFFER_HEX(TAG, buffer, len);
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
                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER:
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

                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER:
                            result->value = ctx->get_serial_number(ctx->arg);
                            break;

                        case EASYCONNECT_HOLDING_REGISTER_ALARMS:
                            result->value = !safety_ok();
                            break;

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
                        case EASYCONNECT_HOLDING_REGISTER_SERIAL_NUMBER:
                            ctx->save_serial_number(ctx->arg, args->value);
                            break;
                    }
                    break;
                }

                case MODBUS_COIL:
                    if (rele_update(ctx->arg, args->value)) {
                        result->exceptionCode = MODBUS_EXCEP_SLAVE_FAILURE;
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
    if (requestLength < 3) {
        return modbusBuildException(minion, function, MODBUS_EXCEP_ILLEGAL_VALUE);
    }

    easyconnect_interface_t *ctx = modbusSlaveGetUserPointer(minion);
    uint16_t class               = requestPDU[1] << 8 | requestPDU[2];
    if (class == ctx->get_class(ctx->arg)) {
        rele_update(ctx->arg, requestPDU[3]);
    }

    return MODBUS_NO_ERROR();
}