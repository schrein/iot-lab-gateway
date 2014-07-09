#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif // _BSD_SOURCE
// getline (for glibc > 2.10)
#define _POSIX_C_SOURCE  200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>
#include "common.h"

#include "time_ref.h"
#include "command_reader.h"
#include "constants.h"
#include "utils.h"

#include "command_parser.h"


struct command_buffer {
    union {
        struct _pkt{
            unsigned char sync;
            unsigned char len;
            unsigned char payload[256];
        } s;
        unsigned char pkt[258];
    } u;
};

struct dict_entry alim_d[] = {
    {"dc", DC},
    {"battery", BATTERY},
    {NULL, 0},
};


/* Consumption dicts */
struct dict_entry periods_d[] = {
    {"140",  PERIOD_140us},
    {"204",  PERIOD_204us},
    {"332",  PERIOD_332us},
    {"588",  PERIOD_588us},
    {"1100", PERIOD_1100us},
    {"2116", PERIOD_2116us},
    {"4156", PERIOD_4156us},
    {"8244", PERIOD_8244us},
    {NULL, 0},
};
struct dict_entry average_d[] = {
    {"1",    AVERAGE_1},
    {"4",    AVERAGE_4},
    {"16",   AVERAGE_16},
    {"64",   AVERAGE_64},
    {"128",  AVERAGE_128},
    {"256",  AVERAGE_256},
    {"512",  AVERAGE_512},
    {"1024", AVERAGE_1024},
    {NULL, 0},
};
struct dict_entry power_source_d[] = {
    {"3.3V",  PW_SRC_3_3V},
    {"5V",    PW_SRC_5V},
    {"BATT",  PW_SRC_BATT},
    {NULL, 0},
};


/* Radio dicts */
struct dict_entry radio_power_d[] = {
    {"3.0",   POWER_3dBm},
    {"2.8", POWER_2_8dBm},
    {"2.3", POWER_2_3dBm},
    {"1.8", POWER_1_8dBm},
    {"1.3", POWER_1_3dBm},
    {"0.7", POWER_0_7dBm},
    {"0.0",   POWER_0dBm},
    {"-1.0",  POWER_m1dBm},
    {"-2.0",  POWER_m2dBm},
    {"-3.0",  POWER_m3dBm},
    {"-4.0",  POWER_m4dBm},
    {"-5.0",  POWER_m5dBm},
    {"-7.0",  POWER_m7dBm},
    {"-9.0",  POWER_m9dBm},
    {"-12.0", POWER_m12dBm},
    {"-17.0", POWER_m17dBm},
    {NULL, 0},
};

struct dict_entry ack_d[] = {
    {"ACK", ACK},
    {"NACK", NACK},
    {NULL, 0},
};

struct dict_entry commands_d[] = {
    {"error",      LOGGER_FRAME},

    {"start",      OPEN_NODE_START},
    {"stop",       OPEN_NODE_STOP},
    {"set_time",   SET_TIME},

    {"green_led_on",    GREEN_LED_ON},
    {"green_led_blink", GREEN_LED_BLINK},


    {"config_radio_stop",    CONFIG_RADIO_STOP},
    {"config_radio_measure", CONFIG_RADIO_MEAS},
    {"config_radio_sniffer", CONFIG_RADIO_SNIFFER},
    //{"config_radio_noise",   CONFIG_RADIO_NOISE},

    //{"config_fake_sensor", CONFIG_SENSOR},

    {"config_consumption_measure", CONFIG_CONSUMPTION},


    {"test_radio_ping_pong", TEST_RADIO_PING_PONG},
    {"test_gpio",    TEST_GPIO},
    {"test_i2c",     TEST_I2C2},
    {"test_pps",     TEST_PPS},
    {"test_got_pps", TEST_GOT_PPS},
    {NULL, 0},
};

struct dict_entry state_d[] = {
    {"start", START},
    {"stop",  STOP},
    {NULL, 0},
};

static void *read_commands(void *attr);
static void append_data(struct command_buffer *cmd_buff, void *data,
        size_t size);
static uint32_t parse_channels_list(char *channels_list);


struct state {
    int       serial_fd;
    pthread_t reader_thread;
} reader_state;


int command_reader_start(int serial_fd)
{
    int ret;

    reader_state.serial_fd = serial_fd;
    ret = pthread_create(&reader_state.reader_thread, NULL, read_commands,
            &reader_state);
    return ret;
}


static int cmd_no_args(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str == "%s" */
    (void)cmd_str;
    (void)cmd_buff;
    (void)command;
    return 0;
}

static int cmd_set_time(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str == "%s" */
    (void)cmd_str;
    (void)command;

    gettimeofday(&set_time_ref, NULL);
    append_data(cmd_buff, &set_time_ref.tv_sec,  sizeof(uint32_t));
    append_data(cmd_buff, &set_time_ref.tv_usec, sizeof(uint32_t));
    return 0;
}

static int cmd_alim(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str == "<start|stop> <dc|battery>" */
    int ret = 0;
    uint8_t val;
    char arg[32];

    sscanf(cmd_str, command->fmt, arg);
    // <dc|battery>
    ret |= get_val(arg, alim_d, &val);
    cmd_buff->u.s.payload[cmd_buff->u.s.len++] = val;
    return ret;
}


static int cmd_start_stop_args(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str == "%s <start|stop>" */

    int ret = 0;
    uint8_t val;
    char arg[32];

    sscanf(cmd_str, command->fmt, arg);
    // <start|stop>
    ret |= get_val(arg, state_d, &val);
    cmd_buff->u.s.payload[cmd_buff->u.s.len++] = val;
    return ret;
}

static int cmd_consumption(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str ==
     * "config_consumption_measure stop"
     * "config_consumption_measure %8s %8s p %i v %i c %i -p %8s -a %8s"
     */

    int ret = 0;
    uint8_t val;

    char start_stop[8];
    char pw_src[8];
    int p, v, c;
    char period[8], average[8];
    uint8_t state = 0;

    sscanf(cmd_str, command->fmt, start_stop, pw_src, &p, &v, &c,
            period, average);
    /*
     * 1B: start/stop
     * 1B: source | measures
     * 1B: period | average
     */

    // start/stop
    if (get_val(start_stop, state_d, &state))
        return 1;
    append_data(cmd_buff, &state, sizeof(uint8_t));

    uint8_t config_0, config_1;
    if (state == START) {
        // source | measures
        ret |= get_val(pw_src, power_source_d, &val);
        config_0  = val;
        config_0 |= (!!p * MEASURE_POWER);
        config_0 |= (!!v * MEASURE_VOLTAGE);
        config_0 |= (!!c * MEASURE_CURRENT);

        // period | average
        config_1  = 0;
        ret      |= get_val(period, periods_d, &val);
        config_1 |= val;
        ret      |= get_val(average, average_d, &val);
        config_1 |= val;
    } else { // state == STOP
        config_0 = 0;
        config_1 = 0;
    }
    append_data(cmd_buff, &config_0, sizeof(uint8_t));
    append_data(cmd_buff, &config_1, sizeof(uint8_t));

    return ret;
}


static int cmd_radio_measure(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str ==
     * "config_radio_measure <channels_list> <period> <num_per_channel>"
     */

    int ret = 0;
    char channels_list[256] = {'\0'};
    unsigned int period, num_per_channel;

    sscanf(cmd_str, command->fmt, channels_list, &period, &num_per_channel);

    uint32_t channels_flag = parse_channels_list(channels_list);
    ret |= (0 == channels_flag);
    ret |= ((period & 0xFFFF) == 0);  // non zero and hold on 16bit
    ret |= (num_per_channel > 0xFF);

    append_data(cmd_buff, &channels_flag, sizeof(uint32_t));
    append_data(cmd_buff, &period, sizeof(uint16_t));
    append_data(cmd_buff, &num_per_channel, sizeof(uint8_t));
    return ret;
}

static int cmd_radio_sniffer(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str ==
     * "config_radio_measure <channels_list> <period>"
     */

    int ret = 0;
    char channels_list[256] = {'\0'};
    unsigned int period;

    sscanf(cmd_str, command->fmt, channels_list, &period);

    uint32_t channels_flag = parse_channels_list(channels_list);
    ret |= (0 == channels_flag);

    if (channels_flag & (channels_flag -1)) {
        // not a power of two => several channels
        ret |= ((period & 0xFFFF) == 0);  // non zero and hold on 16bit
    } else {
        // power of two => only one channel
        ret |= (period != 0);
    }

    append_data(cmd_buff, &channels_flag, sizeof(uint32_t));
    append_data(cmd_buff, &period, sizeof(uint16_t));
    return ret;
}


static int cmd_radio_pp(char *cmd_str, struct command_buffer *cmd_buff,
        struct command_description *command)
{
    /* cmd_str ==
     * "test_radio_ping_pong stop"
     * "test_radio_ping_pong start <channel> <tx_power>
     */

    int ret = 0;
    uint8_t state;
    char start_stop[8];
    char tx_power[8];
    unsigned int channel;

    sscanf(cmd_str, command->fmt, start_stop, &channel, tx_power);

    // <start|stop>
    ret |= get_val(start_stop, state_d, &state);
    append_data(cmd_buff, &state, sizeof(uint8_t));

    // <channel> <tx_power>
    uint8_t aux = 0;
    if (state == START) {
        ret |= (channel < 11 || channel > 26);
        append_data(cmd_buff, &channel, sizeof(uint8_t));
        ret |= get_val(tx_power, radio_power_d, &aux);
        append_data(cmd_buff, &aux, sizeof(uint8_t));
    } else {  // state == STOP
        append_data(cmd_buff, &aux, sizeof(uint8_t));
        append_data(cmd_buff, &aux, sizeof(uint8_t));
    }
    return ret;
}

struct command_description commands[] = {
    {"start %8s",                        1, (cmd_fct_t)cmd_alim},
    {"stop %8s",                         1, (cmd_fct_t)cmd_alim},

    {"config_consumption_measure %8s %8s p %i v %i c %i -p %8s -a %8s",
                                         7, (cmd_fct_t)cmd_consumption},
    {"config_consumption_measure %8s",   1, (cmd_fct_t)cmd_consumption},

    {"config_radio_measure %256s %i %i", 3, (cmd_fct_t)cmd_radio_measure},
    {"config_radio_sniffer %256s %i",    2, (cmd_fct_t)cmd_radio_sniffer},
    {"config_radio_stop",                0, (cmd_fct_t)cmd_no_args},

    {"set_time",                         0, (cmd_fct_t)cmd_set_time},
    {"green_led_on",                     0, (cmd_fct_t)cmd_no_args},
    {"green_led_blink",                  0, (cmd_fct_t)cmd_no_args},

    {"test_gpio %8s",                    1, (cmd_fct_t)cmd_start_stop_args},
    {"test_i2c %8s",                     1, (cmd_fct_t)cmd_start_stop_args},

    {"test_pps %8s",                     1, (cmd_fct_t)cmd_start_stop_args},
    {"test_got_pps",                     0, (cmd_fct_t)cmd_no_args},

    {"test_radio_ping_pong %8s %i %8s",  3, (cmd_fct_t)cmd_radio_pp},
    {"test_radio_ping_pong %8s",         1, (cmd_fct_t)cmd_radio_pp},

    {NULL, 0, NULL}
};

static char *cstrtok(const char *s)
{
    static char token[32];
    size_t index = 0;
    index = strcspn(s, " ");
    if (index > sizeof(token))
        return NULL;

    strncpy(token, s, index);
    token[index] = '\0';
    return token;
}

static int parse_cmd(char *line_buff, struct command_buffer *cmd_buff)
{
    struct command_description *cmd = NULL;
    uint8_t cmd_type;
    int ret = 0;

    cmd = command_parse(line_buff, commands);
    if (cmd == NULL)
        return 1;

    // Init command buffer
    cmd_buff->u.s.sync = SYNC_BYTE;
    cmd_buff->u.s.len  = 0;
    memset(&cmd_buff->u.s.payload, 0, sizeof(cmd_buff->u.s.payload));

    // Add command
    // keep line_buff unmodified as it's used with scanf in command handler
    char *command = cstrtok(line_buff);
    ret |= get_val(command, commands_d, &cmd_type);
    append_data(cmd_buff, &cmd_type, sizeof(uint8_t));

    // Call command args handler
    return ret | cmd->command(line_buff, cmd_buff, cmd);
}

static void append_data(struct command_buffer *cmd_buff, void *data,
        size_t size)
{
    memcpy(&cmd_buff->u.s.payload[cmd_buff->u.s.len], data, size);
    cmd_buff->u.s.len += size;
}

static uint32_t parse_channels_list(char *channels_list)
{
    uint32_t channels_flag = 0;
    char *chan;
    int channel;

    chan = strtok(channels_list, ",");
    while (chan != NULL) {
        // channel 11 in bit num 11
        // channel 26 in bit num 26
        channel = atoi(chan);
        if (channel >= 11 && channel <= 26)
            channels_flag |= 1 << channel;

        chan = strtok(NULL, ",");
    }
    return channels_flag;
}

int write_answer(unsigned char *data, size_t len)
{
    DEBUG_PRINT("write answer, pkt len  %zu\n", len);

    uint8_t type;
    int got_error;
    char *cmd = NULL;


    if (len != 2)
        return -1;

    type = data[0];

    got_error = 0;
    if (type == LOGGER_FRAME) {
        /*
         * I'm only printing the error number and not what it means
         * It's done on purpose as I don't want to add it for the moment
         * when the code is not stable.
         *
         * As errors should 'never' happen, I prefer that it gets
         * searched in the source code to find what it means
         *
         * It also allows adding new temporary errors on control node
         * without intervention on this code.
         */
        DEBUG_PRINT("error frame\n");
        int error_code;
        // handle error frame
        got_error |= get_key(type, commands_d, &cmd);
        error_code = (int) ((char) data[1]); // sign extend
        PRINT_MSG("%s %d\n", cmd, error_code);
    } else {
        DEBUG_PRINT("Commands ACKS\n");
        char *arg;
        got_error |= get_key(type, commands_d, &cmd);
        got_error |= get_key(data[1], ack_d, &arg);
        // CMDs acks
        if (got_error) {
            PRINT_ERROR("invalid answer: %02X %02X\n",
                    data[0], data[1]);
            return -3;
        }
        PRINT_MSG("%s %s\n", cmd, arg);
    }
    return 0;
}


static void *read_commands(void *attr)
{
    struct state *reader_state = (struct state *) attr;

    struct command_buffer cmd_buff;
    size_t buff_size = 2048;
    char *line_buff  = (char *)malloc(buff_size);
    char *command_save  = (char *)malloc(2048);
    int ret;

    int n;
    PRINT_MSG("cn_serial_ready\n");
    while ((n = getline(&line_buff, &buff_size, stdin)) != -1) {
        DEBUG_PRINT("Command: %s: ", line_buff);
        line_buff[n - 1] = '\0'; // remove new line
        strncpy(command_save, line_buff, 2048);
        DEBUG_PRINT("Command: %s: ", line_buff);
        ret = parse_cmd(line_buff, &cmd_buff);
        if (ret) {
            PRINT_ERROR("Invalid command: '%s'\n", command_save);
        } else {
            DEBUG_PRINT("    ");
            DEBUG_PRINT_PACKET(cmd_buff.u.pkt,
                    2 + cmd_buff.u.s.len);
            ret = write(reader_state->serial_fd, cmd_buff.u.pkt,
                    cmd_buff.u.s.len + 2);
            DEBUG_PRINT("    write ret: %i\n", ret);
        }
    }
    exit(0);
    return NULL;
}
