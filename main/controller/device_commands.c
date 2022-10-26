#include "esp_err.h"
#include "esp_console.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "device_commands.h"
#include "peripherals/digout.h"
#include "peripherals/digin.h"
#include "model/model.h"
#include "configuration.h"
#include "rele.h"


static int device_commands_set_rele(int argc, char **argv);
static int device_commands_read_inputs(int argc, char **argv);
static int device_commands_read_inputs(int argc, char **argv);
static int device_commands_read_feedback(int argc, char **argv);
static int device_commands_set_feedback(int argc, char **argv);
static int device_commands_read_safety_message(int argc, char **argv);
static int device_commands_set_safety_message(int argc, char **argv);
static int device_commands_read_feedback_message(int argc, char **argv);
static int device_commands_set_feedback_message(int argc, char **argv);


static model_t *model_ref = NULL;


void device_commands_register(model_t *pmodel) {
    model_ref = pmodel;

    const esp_console_cmd_t rele_cmd = {
        .command = "SetRele",
        .help    = "Set rele' level",
        .hint    = NULL,
        .func    = &device_commands_set_rele,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rele_cmd));

    const esp_console_cmd_t signal_cmd = {
        .command = "ReadSignals",
        .help    = "Read signals levels",
        .hint    = NULL,
        .func    = &device_commands_read_inputs,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&signal_cmd));

    const esp_console_cmd_t read_feedback = {
        .command = "ReadFB",
        .help    = "Print the current device feedback configuration",
        .hint    = NULL,
        .func    = &device_commands_read_feedback,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_feedback));

    const esp_console_cmd_t set_feedback = {
        .command = "SetFB",
        .help    = "Set a new feedback configuration",
        .hint    = NULL,
        .func    = &device_commands_set_feedback,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_feedback));

    const esp_console_cmd_t read_safety_message = {
        .command = "ReadSafetyMessage",
        .help    = "Print the configured safety warning",
        .hint    = NULL,
        .func    = &device_commands_read_safety_message,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_safety_message));

    const esp_console_cmd_t set_safety_message = {
        .command = "SetSafetyMessage",
        .help    = "Set a new safety warning",
        .hint    = NULL,
        .func    = &device_commands_set_safety_message,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_safety_message));

    const esp_console_cmd_t read_feedback_message = {
        .command = "ReadFeedbackMessage",
        .help    = "Print the configured feedback warning",
        .hint    = NULL,
        .func    = &device_commands_read_feedback_message,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&read_feedback_message));

    const esp_console_cmd_t set_feedback_message = {
        .command = "SetFeedbackMessage",
        .help    = "Set a new feedback warning",
        .hint    = NULL,
        .func    = &device_commands_set_feedback_message,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_feedback_message));
}


static int device_commands_set_rele(int argc, char **argv) {
    struct arg_end *end;
    struct arg_int *rele;
    /* the global arg_xxx structs are initialised within the argtable */
    void *argtable[] = {
        rele = arg_int1(NULL, NULL, "<value>", "Rele' level"),
        end  = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        rele_update(model_ref, rele->ival[0]);
    } else {
        arg_print_errors(stdout, end, "Set rele' level");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_read_inputs(int argc, char **argv) {
    struct arg_end *end;
    /* the global arg_xxx structs are initialised within the argtable */
    void *argtable[] = {
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        uint8_t value = (uint8_t)digin_get_inputs();
        printf("Safety=%i, Signal=%i\n", (value & 0x01) > 0, (value & 0x02) > 0);
    } else {
        arg_print_errors(stdout, end, "Read device inputs");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_read_feedback(int argc, char **argv) {
    struct arg_end *end;
    /* the global arg_xxx structs are initialised within the argtable */
    void *argtable[] = {
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        if (model_get_feedback_enabled(model_ref)) {
            printf("Feedback direction=%i, Activation attempts=%i, Feedback delay=%i\n",
                   model_get_feedback_direction(model_ref), model_get_output_attempts(model_ref),
                   model_get_feedback_delay(model_ref));
        } else {
            printf("Feedback disabled\n");
        }
    } else {
        arg_print_errors(stdout, end, "Read feedback parameters");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_set_feedback(int argc, char **argv) {
    // TODO: add --off
    struct arg_int *dir, *att, *del;
    struct arg_lit *help, *off;
    struct arg_end *end;
    void           *argtable[] = {
        help = arg_litn("h", "help", 0, 1, "display this help and exit"),
        off  = arg_litn("o", "off", 0, 1, "disable the feedback mechanism"),
        dir  = arg_int1(NULL, NULL, "<feedback direction>",
                                  "Direction of the feedback signal (0=active low, 1=active high)"),
        att = arg_int1(NULL, NULL, "<activation attempts>", "Number of attempts to activate the output (1-8)"),
        del = arg_int1(NULL, NULL, "<feedback delay>", "Delay of the feedback check (1-8 seconds)"),
        end = arg_end(8),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (help->count > 0) {
        printf("Usage:");
        arg_print_syntax(stdout, argtable, "\n");
        printf("\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return ESP_OK;
    } else if (off->count > 0) {
        configuration_save_feedback_enable(model_ref, 0);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return ESP_OK;
    } else if (nerrors == 0) {
        configuration_save_feedback_enable(model_ref, 1);

        uint8_t feedback_direction = (uint8_t)dir->ival[0];
        if (feedback_direction > EASYCONNECT_PARAMETER_MAX_FEEDBACK_DIRECTION) {
            printf("Invalid feedback direction value: %i\n", feedback_direction);
        } else {
            configuration_save_feedback_direction(model_ref, feedback_direction);
        }

        uint8_t activation_attempts = (uint8_t)att->ival[0];
        if (activation_attempts > EASYCONNECT_PARAMETER_MAX_ACTIVATION_ATTEMPTS) {
            printf("Invalid activation attempts value: %i\n", activation_attempts);
        } else {
            configuration_save_activation_attempts(model_ref, activation_attempts);
        }

        uint8_t feedback_delay = (uint8_t)del->ival[0];
        if (feedback_delay > EASYCONNECT_PARAMETER_MAX_FEEDBACK_DELAY) {
            printf("Invalid feedback delay value: %i\n", feedback_delay);
        } else {
            configuration_save_feedback_delay(model_ref, feedback_delay);
        }
    } else {
        arg_print_errors(stdout, end, "Set feedback");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_read_safety_message(int argc, char **argv) {
    struct arg_end *end;
    void           *argtable[] = {
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        char safety_message[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
        model_get_safety_message(model_ref, safety_message);
        printf("%s\n", safety_message);
    } else {
        arg_print_errors(stdout, end, "Read safety message");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_set_safety_message(int argc, char **argv) {
    struct arg_str *sm;
    struct arg_end *end;
    void           *argtable[] = {
        sm  = arg_str1(NULL, NULL, "<safety message>", "safety message"),
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        configuration_save_safety_message(model_ref, sm->sval[0]);
    } else {
        arg_print_errors(stdout, end, "Set safety message");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_read_feedback_message(int argc, char **argv) {
    struct arg_end *end;
    void           *argtable[] = {
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        char feedback_message[EASYCONNECT_MESSAGE_SIZE + 1] = {0};
        model_get_feedback_message(model_ref, feedback_message);
        printf("%s\n", feedback_message);
    } else {
        arg_print_errors(stdout, end, "Read feedback message");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}


static int device_commands_set_feedback_message(int argc, char **argv) {
    struct arg_str *sm;
    struct arg_end *end;
    void           *argtable[] = {
        sm  = arg_str1(NULL, NULL, "<feedback message>", "feedback message"),
        end = arg_end(1),
    };

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors == 0) {
        configuration_save_feedback_message(model_ref, sm->sval[0]);
    } else {
        arg_print_errors(stdout, end, "Set feedback message");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return nerrors ? -1 : 0;
}