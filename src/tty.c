/*
 * tio - a simple serial terminal I/O tool
 *
 * Copyright (c) 2014-2022  Martin Lund
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include "serialport.h"
#include "configfile.h"
#include "tty.h"
#include "print.h"
#include "options.h"
#include "misc.h"
#include "log.h"
#include "error.h"
#include "misc.h"
#include "setspeed.h"
#include "alert.h"
#include "timestamp.h"
#include "cpoll.h"
#include "ring.h"
#include "enumport.h"
#include "script.h"

#define LINE_SIZE_MAX 1000

#define KEY_0 0x30
#define KEY_1 0x31
#define KEY_2 0x32
#define KEY_3 0x33
#define KEY_4 0x34
#define KEY_5 0x35
#define KEY_QUESTION 0x3f
#define KEY_B 0x62
#define KEY_C 0x63
#define KEY_E 0x65
#define KEY_F 0x66
#define KEY_SHIFT_F 0x46
#define KEY_G 0x67
#define KEY_H 0x68
#define KEY_L 0x6C
#define KEY_SHIFT_L 0x4C
#define KEY_M 0x6D
#define KEY_P 0x70
#define KEY_Q 0x71
#define KEY_R 0x72
#define KEY_S 0x73
#define KEY_T 0x74
#define KEY_U 0x55
#define KEY_V 0x76
#define KEY_X 0x78
#define KEY_Y 0x79
#define KEY_Z 0x7a

typedef enum
{
    LINE_OFF,
    LINE_TOGGLE,
    LINE_PULSE
} tty_line_mode_t;

const char random_array[] =
{
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x28, 0x20, 0x28, 0x0A, 0x20,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x29, 0x20, 0x29, 0x0A, 0x20,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,
0x2E, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x7C, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x7C, 0x5D, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
0x5C, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x2F, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x60, 0x2D, 0x2D, 0x2D, 0x2D, 0x27, 0x0A, 0x0A, 0x54,
0x69, 0x6D, 0x65, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x61, 0x20, 0x63, 0x6F, 0x66,
0x66, 0x65, 0x65, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6B, 0x21, 0x0A, 0x20, 0x0A,
0x00
};

bool interactive_mode = true;
bool map_i_nl_cr = false;
bool map_i_cr_nl = false;
bool map_ign_cr = false;

char key_hit = 0xff;

// static struct termios tio, tio_old, stdout_new, stdout_old, stdin_new, stdin_old;
static unsigned long rx_total = 0, tx_total = 0;
static __LONG32 connected = false;
static void (*print)(char c);
static struct sp_port *hPort;
static struct sp_port_config* cfgPort, *cfgPort_old;
static struct sp_event_set *sp_event = NULL;
static bool map_i_ff_escc = false;
static bool map_i_nl_crnl = false;
static bool map_o_cr_nl = false;
static bool map_o_nl_crnl = false;
static bool map_o_del_bs = false;
static bool map_o_ltu = false;
static bool map_o_msblsb = false;
static char hex_chars[2];
static unsigned char hex_char_index = 0;
static char tty_buffer[BUFSIZ*2];
static size_t tty_buffer_count = 0;
static char *tty_buffer_write_ptr = tty_buffer;
static pthread_t thread;
static RING_Handle_t ring;
static pthread_mutex_t mutex_input_ready = PTHREAD_MUTEX_INITIALIZER;
static char line[LINE_SIZE_MAX];
static HANDLE ev_exit;


static void optional_local_echo(char c)
{
    if (!option.local_echo)
    {
        return;
    }
    print(c);
    if (option.log)
    {
        log_putc(c);
    }
}

inline static bool is_valid_hex(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

inline static unsigned char char_to_nibble(char c)
{
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    else
    {
        return 0;
    }
}

void tty_sync()
{
    ssize_t count;

    while (tty_buffer_count > 0)
    {
        count = sp_blocking_write(hPort, tty_buffer, tty_buffer_count, 0);
        if (count < 0)
        {
            // Error
            tio_debug_printf("Write error while flushing tty buffer (%s)", GetErrorMessage(GetLastError()));
            break;
        }
        tty_buffer_count -= count;
        sp_drain(hPort);
    }

    // Reset
    tty_buffer_write_ptr = tty_buffer;
    tty_buffer_count = 0;
}

ssize_t tty_write(const void *buffer, size_t count)
{
    ssize_t retval = 0, bytes_written = 0;
    size_t i;

    if (map_o_ltu)
    {
        // Convert lower case to upper case
        for (i = 0; i<count; i++)
        {
            *((unsigned char*)buffer+i) = toupper(*((unsigned char*)buffer+i));
        }
    }

    if (option.output_delay || option.output_line_delay)
    {
        // Write byte by byte with output delay
        for (i=0; i<count; i++)
        {
            retval = sp_blocking_write(hPort, buffer, 1, 0);
            if (retval < 0)
            {
                // Error
                tio_debug_printf("Write error (%s)", GetErrorMessage(GetLastError()));
                break;
            }
            bytes_written += retval;

            if (option.output_line_delay && *(unsigned char*)buffer == '\n')
            {
                delay(option.output_line_delay);
            }

            sp_drain(hPort);

            if (option.output_delay)
            {
                delay(option.output_delay);
            }
        }
    }
    else
    {
        // Force write of tty buffer if too full
        if ((tty_buffer_count + count) > BUFSIZ)
        {
            tty_sync(hPort);
        }

        // Copy bytes to tty write buffer
        memcpy(tty_buffer_write_ptr, buffer, count);
        tty_buffer_write_ptr += count;
        tty_buffer_count += count;
        bytes_written = count;
    }

    return bytes_written;
}

void *tty_stdin_input_thread(void *arg)
{
    UNUSED(arg);
    char input_buffer[BUFSIZ];
    ssize_t byte_count;

    // Create FIFO
    ring = RING_Init(0x8000);

    // Signal that input pipe is ready
    pthread_mutex_unlock(&mutex_input_ready);

    // Input loop for stdin
    while (1)
    {
        /* Input from stdin ready */
        byte_count = read(STDIN_FILENO, input_buffer, BUFSIZ);
        if (byte_count < 0)
        {
            /* No error actually occurred */
            if (errno == EINTR)
            {
                continue;
            }
            tio_warning_printf("Could not read from stdin (%s)", strerror(errno));
        }
        else if (byte_count == 0)
        {
            SetEvent(ev_exit);
            pthread_exit(0);
        }

        if (interactive_mode)
        {
            static char previous_char = 0;
            char input_char;

            // Process quit and flush key command
            for (int i = 0; i<byte_count; i++)
            {
                // first do key hit check for xmodem abort
                if (!key_hit) {
                    key_hit = input_buffer[i];
                    byte_count--;
                    memcpy(input_buffer+i, input_buffer+i+1, byte_count-i);
                    continue;
                }

                input_char = input_buffer[i];

                if (option.prefix_enabled && previous_char == option.prefix_code)
                {
                    if (input_char == option.prefix_code)
                    {
                        previous_char = 0;
                        continue;
                    }

                    switch (input_char)
                    {
                        case KEY_Q:
                            SetEvent(ev_exit);
                            exit(EXIT_SUCCESS);
                            break;
                        case KEY_SHIFT_F:
                            tio_printf("Flushed data I/O channels")
                            sp_drain(hPort);
                            break;
                        default:
                            break;
                    }
                }
                previous_char = input_char;
            }
        }

        // Write all bytes read to pipe
        RING_Write_Blocking(ring, input_buffer, byte_count);
    }

    SetEvent(ev_exit);
    pthread_exit(0);
}

void tty_input_thread_create(void)
{
    pthread_mutex_lock(&mutex_input_ready);

    ev_exit = CreateEventA(NULL, TRUE, FALSE, NULL);

    if (pthread_create(&thread, NULL, tty_stdin_input_thread, NULL) != 0) {
        tio_error_printf("pthread_create() error");
        exit(1);
    }
}

void tty_input_thread_wait_ready(void)
{
    pthread_mutex_lock(&mutex_input_ready);
}

static void output_hex(char c)
{
    hex_chars[hex_char_index++] = c;

    printf("%c", c);

    if (hex_char_index == 2)
    {
        usleep(100*1000);
        printf("\b \b");
        printf("\b \b");

        unsigned char hex_value = char_to_nibble(hex_chars[0]) << 4 | (char_to_nibble(hex_chars[1]) & 0x0F);
        hex_char_index = 0;

        optional_local_echo(hex_value);

        ssize_t status = tty_write(&hex_value, 1);
        if (status < 0)
        {
            tio_warning_printf("Could not write to tty device");
        }
        else
        {
            tx_total++;
        }
    }
}

void tty_line_set(int mask, int value)
{
    struct sp_port_config* config;
    sp_new_config(&config);
    sp_get_config(hPort, config);
    if(mask & TIOCM_DTR) {
        sp_set_config_dtr(config, (value & TIOCM_DTR) ? SP_DTR_ON : SP_DTR_OFF);
        tio_printf("Setting DTR to %s", (value & TIOCM_DTR) ? "LOW" : "HIGH");
    }
    if(mask & TIOCM_RTS) {
        sp_set_config_rts(config, (value & TIOCM_RTS) ? SP_RTS_ON : SP_RTS_OFF);
        tio_printf("Setting RTS to %s", (value & TIOCM_RTS) ? "LOW" : "HIGH");
    }
    sp_set_config(hPort, config);
    sp_free_config(config);
}

void tty_line_toggle(int mask)
{
    struct sp_port_config* config;
    sp_new_config(&config);
    sp_get_config(hPort, config);
    enum sp_dtr dtr;
    enum sp_rts rts;
    sp_get_config_dtr(config, &dtr);
    sp_get_config_rts(config, &rts);
    if(mask & TIOCM_DTR) {
        sp_set_config_dtr(config, dtr == SP_DTR_OFF ? SP_DTR_ON : SP_DTR_OFF);
        tio_printf("Setting DTR to %s", dtr == SP_DTR_OFF ? "LOW" : "HIGH");
    }
    if(mask & TIOCM_RTS) {
        sp_set_config_rts(config, rts == SP_RTS_OFF ? SP_RTS_ON : SP_RTS_OFF);
        tio_printf("Setting RTS to %s", rts == SP_RTS_OFF ? "LOW" : "HIGH");
    }
    sp_set_config(hPort, config);
    sp_free_config(config);
}

static void tty_line_pulse(int mask, unsigned int duration)
{
    tty_line_toggle(mask);

    if (duration > 0)
    {
        tio_printf("Waiting %d ms", duration);
        delay(duration);
    }

    tty_line_toggle(mask);
}

static void tty_line_poke(int mask, tty_line_mode_t mode, unsigned int duration)
{
    switch (mode)
    {
        case LINE_TOGGLE:
            tty_line_toggle(mask);
            break;

        case LINE_PULSE:
            tty_line_pulse(mask, duration);
            break;

        case LINE_OFF:
            break;
    }
}

static int tio_readln(void)
{
    char *p = line;

    /* Read line, accept BS and DEL as rubout characters */
    for (p = line ; p < &line[LINE_SIZE_MAX-1]; )
    {
        if (RING_Read_Blocking(ring, p, 1) == 0)
        {
            if (*p == 0x08 || *p == 0x7f)
            {
                if (p > line )
                {
                    write(STDOUT_FILENO, "\b \b", 3);
                    p--;
                }
                continue;
            }
            write(STDOUT_FILENO, p, 1);
            if (*p == '\r') break;
            p++;
        }
    }
    *p = 0;
    return (p - line);
}

void handle_command_sequence(char input_char, char *output_char, bool *forward)
{
    char unused_char;
    bool unused_bool;
    static tty_line_mode_t line_mode = LINE_OFF;
    static char previous_char = 0;

    /* Ignore unused arguments */
    if (output_char == NULL)
    {
        output_char = &unused_char;
    }

    if (forward == NULL)
    {
        forward = &unused_bool;
    }

    // Handle tty line toggle and pulse action
    if (line_mode)
    {
        *forward = false;
        switch (input_char)
        {
            case KEY_0:
                tty_line_poke(TIOCM_DTR, line_mode, option.dtr_pulse_duration);
                break;
            case KEY_1:
                tty_line_poke(TIOCM_RTS, line_mode, option.rts_pulse_duration);
                break;
            case KEY_2:
                tty_line_poke(TIOCM_DTR | TIOCM_RTS, line_mode, option.pulse_duration);
                break;
            default:
                tio_warning_printf("Invalid line number");
                break;
        }

        line_mode = LINE_OFF;

        return;
    }

    /* Handle escape key commands */
    if (option.prefix_enabled && previous_char == option.prefix_code)
    {
        /* Do not forward input char to output by default */
        *forward = false;

        /* Handle special double prefix key input case */
        if (input_char == option.prefix_code)
        {
            /* Forward prefix character to tty */
            *output_char = option.prefix_code;
            *forward = true;
            previous_char = 0;
            return;
        }

        switch (input_char)
        {
            case KEY_QUESTION:
                tio_printf("Key commands:");
                tio_printf(" ctrl-%c ?       List available key commands", option.prefix_key);
                tio_printf(" ctrl-%c b       Send break", option.prefix_key);
                tio_printf(" ctrl-%c c       Show configuration", option.prefix_key);
                tio_printf(" ctrl-%c e       Toggle local echo mode", option.prefix_key);
                tio_printf(" ctrl-%c f       Toggle log to file", option.prefix_key);
                tio_printf(" ctrl-%c F       Flush data I/O buffers", option.prefix_key);
                tio_printf(" ctrl-%c g       Toggle serial port line", option.prefix_key);
                tio_printf(" ctrl-%c h       Toggle hexadecimal mode", option.prefix_key);
                tio_printf(" ctrl-%c l       Clear screen", option.prefix_key);
                tio_printf(" ctrl-%c L       Show line states", option.prefix_key);
                tio_printf(" ctrl-%c m       Toggle MSB to LSB bit order", option.prefix_key);
                tio_printf(" ctrl-%c p       Pulse serial port line", option.prefix_key);
                tio_printf(" ctrl-%c q       Quit", option.prefix_key);
                tio_printf(" ctrl-%c r       Run script", option.prefix_key);
                tio_printf(" ctrl-%c s       Show statistics", option.prefix_key);
                tio_printf(" ctrl-%c t       Toggle line timestamp mode", option.prefix_key);
                tio_printf(" ctrl-%c U       Toggle conversion to uppercase on output", option.prefix_key);
                tio_printf(" ctrl-%c v       Show version", option.prefix_key);
                tio_printf(" ctrl-%c x       Send file via Xmodem-1K", option.prefix_key);
                tio_printf(" ctrl-%c y       Send file via Ymodem", option.prefix_key);
                tio_printf(" ctrl-%c ctrl-%c Send ctrl-%c character", option.prefix_key, option.prefix_key, option.prefix_key);
                break;

            case KEY_SHIFT_L:
            {
                enum sp_signal signal;
                if (sp_get_signals(hPort, &signal) < 0)
                {
                    tio_warning_printf("Could not get line state (%s)", GetErrorMessage(GetLastError()));
                    break;
                }
                struct sp_port_config* config;
                sp_new_config(&config);
                sp_get_config(hPort, config);
                enum sp_dtr dtr;
                enum sp_rts rts;
                enum sp_cts cts;
                enum sp_dsr dsr;
                sp_get_config_dtr(config,&dtr);
                sp_get_config_rts(config,&rts);
                sp_get_config_cts(config,&cts);
                sp_get_config_dsr(config,&dsr);
                sp_free_config(config);

                tio_printf("Line states:");
                tio_printf(" DTR: %s", (dtr == SP_DTR_OFF) ? "HIGH" : "LOW");
                tio_printf(" RTS: %s", (rts == SP_RTS_OFF) ? "HIGH" : "LOW");
                tio_printf(" CTS: %s", (signal & SP_SIG_CTS) ? "LOW" : "HIGH");
                tio_printf(" DSR: %s", (signal & SP_SIG_DSR) ? "LOW" : "HIGH");
                tio_printf(" DCD: %s", (signal & SP_SIG_DCD) ? "LOW" : "HIGH");
                tio_printf(" RI : %s", (signal & SP_SIG_RI) ? "LOW" : "HIGH");
                break;
            }
            case KEY_F:
                if (option.log)
                {
                    log_close();
                    option.log = false;
                }
                else
                {
                    if (log_open(option.log_filename) == 0)
                    {
                        option.log = true;
                    }
                }
                tio_printf("Switched log to file %s", option.log ? "on" : "off");
                break;

            case KEY_SHIFT_F:
                break;

            case KEY_G:
                tio_printf("Please enter which serial line number to toggle:");
                tio_printf(" DTR        (0)");
                tio_printf(" RTS        (1)");
                tio_printf(" DTR+RTS    (2)");
                // Process next input character as part of the line toggle step
                line_mode = LINE_TOGGLE;
                break;

            case KEY_P:
                tio_printf("Please enter which serial line number to pulse:");
                tio_printf(" DTR        (0)");
                tio_printf(" RTS        (1)");
                tio_printf(" DTR+RTS    (2)");
                // Process next input character as part of the line pulse step
                line_mode = LINE_PULSE;
                break;

            case KEY_B:
                sp_start_break(hPort);
                delay(100);
                sp_end_break(hPort);
                break;

            case KEY_C:
                tio_printf("Configuration:");
                options_print();
                config_file_print();
                break;

            case KEY_E:
                option.local_echo = !option.local_echo;
                tio_printf("Switched local echo %s", option.local_echo ? "on" : "off");
                break;

            case KEY_H:
                /* Toggle hexadecimal printing mode */
                if (!option.hex_mode)
                {
                    print = print_hex;
                    option.hex_mode = true;
                    tio_printf("Switched to hexadecimal mode");
                }
                else
                {
                    print = print_normal;
                    option.hex_mode = false;
                    tio_printf("Switched to normal mode");
                }
                break;

            case KEY_L:
                /* Clear screen using ANSI/VT100 escape code */
                printf("\033c");
                break;

            case KEY_M:
                /* Toggle bit order */
                if (!map_o_msblsb)
                {
                    map_o_msblsb = true;
                    tio_printf("Switched to reverse bit order");
                }
                else
                {
                    map_o_msblsb = false;
                    tio_printf("Switched to normal bit order");
                }
                break;

            case KEY_Q:
                /* Exit upon ctrl-t q sequence */
                exit(EXIT_SUCCESS);

            case KEY_R:
                /* Run script */
                script_run();
                break;

            case KEY_S:
                /* Show tx/rx statistics upon ctrl-t s sequence */
                tio_printf("Statistics:");
                tio_printf(" Sent %lu bytes", tx_total);
                tio_printf(" Received %lu bytes", rx_total);
                break;

            case KEY_T:
                option.timestamp += 1;
                switch (option.timestamp)
                {
                    case TIMESTAMP_NONE:
                        break;
                    case TIMESTAMP_24HOUR:
                        tio_printf("Switched to 24hour timestamp mode");
                        break;
                    case TIMESTAMP_24HOUR_START:
                        tio_printf("Switched to 24hour-start timestamp mode");
                        break;
                    case TIMESTAMP_24HOUR_DELTA:
                        tio_printf("Switched to 24hour-delta timestamp mode");
                        break;
                    case TIMESTAMP_ISO8601:
                        tio_printf("Switched to iso8601 timestamp mode");
                        break;
                    case TIMESTAMP_END:
                        option.timestamp = TIMESTAMP_NONE;
                        tio_printf("Switched timestamp off");
                        break;
                }
                break;

            case KEY_U:
                map_o_ltu = !map_o_ltu;
                break;

            case KEY_V:
                tio_printf("tio v%s", VERSION);
                break;

            case KEY_X:
            case KEY_Y:
                tio_printf("Send file with %cMODEM", toupper(input_char));
                tio_printf_raw("Enter file name: ");
                if (tio_readln()) {
                    tio_printf("Sending file '%s'  ", line);
                    tio_printf("Press any key to abort transfer");
                    tio_printf("%s", xymodem_send(hPort, line, input_char) < 0 ? "Aborted" : "Done");
                }
                break;

            case KEY_Z:
                tio_printf_array(random_array);
                break;

            default:
                /* Ignore unknown ctrl-t escaped keys */
                break;
        }
    }

    previous_char = input_char;
}

void stdin_restore(void)
{
    // tcsetattr(STDIN_FILENO, TCSANOW, &stdin_old);
}

void stdin_configure(void)
{
    // int status;

    // /* Save current stdin settings */
    // if (tcgetattr(STDIN_FILENO, &stdin_old) < 0)
    // {
    //     tio_error_printf("Saving current stdin settings failed");
    //     exit(EXIT_FAILURE);
    // }

    // /* Prepare new stdin settings */
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hConsole, &mode); //


    // /* Reconfigure stdin (RAW configuration) */
    mode &=~ENABLE_ECHO_INPUT;
    mode &=~ENABLE_LINE_INPUT;
    mode &=~ENABLE_PROCESSED_INPUT;

    // /* Control characters */
    // stdin_new.c_cc[VTIME] = 0; /* Inter-character timer unused */
    // stdin_new.c_cc[VMIN]  = 1; /* Blocking read until 1 character received */

    // /* Activate new stdin settings */
    SetConsoleMode(hConsole, mode);

    // /* Make sure we restore old stdin settings on exit */
    // atexit(&stdin_restore);
}

void stdout_restore(void)
{
    // tcsetattr(STDOUT_FILENO, TCSANOW, &stdout_old);
}

void stdout_configure(void)
{
    /* Disable line buffering in stdout. This is necessary if we
     * want things like local echo to work correctly. */
    setvbuf(stdout, NULL, _IONBF, 0);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hConsole, &mode); //
    SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    /* At start use normal print function */
    print = print_normal;

    /* Make sure we restore old stdout settings on exit */
    atexit(&stdout_restore);
}

void tty_configure(void)
{
    sp_new_config(&cfgPort);
    sp_new_config(&cfgPort_old);

    /* Set speed */
    sp_set_config_baudrate(cfgPort, option.baudrate);

    /* Set databits */
    sp_set_config_bits(cfgPort, option.databits);

    /* Set flow control */
    if (strcmp("hard", option.flow) == 0)
    {
        sp_set_config_flowcontrol(cfgPort, SP_FLOWCONTROL_RTSCTS);
    }
    else if (strcmp("soft", option.flow) == 0)
    {
        sp_set_config_flowcontrol(cfgPort, SP_FLOWCONTROL_XONXOFF);
    }
    else if (strcmp("none", option.flow) == 0)
    {
        sp_set_config_flowcontrol(cfgPort, SP_FLOWCONTROL_NONE);
    }
    else
    {
        tio_error_printf("Invalid flow control");
        exit(EXIT_FAILURE);
    }

    /* Set stopbits */
    sp_set_config_stopbits(cfgPort, option.stopbits);

    /* Set parity */
    if (strcmp("odd", option.parity) == 0)
    {
        sp_set_config_parity(cfgPort, SP_PARITY_ODD);
    }
    else if (strcmp("even", option.parity) == 0)
    {
       sp_set_config_parity(cfgPort, SP_PARITY_EVEN);
    }
    else if (strcmp("none", option.parity) == 0)
    {
        sp_set_config_parity(cfgPort, SP_PARITY_NONE);
    }
    else if ( strcmp("mark", option.parity) == 0)
    {
        sp_set_config_parity(cfgPort, SP_PARITY_MARK);
    }
    else if ( strcmp("space", option.parity) == 0)
    {
        sp_set_config_parity(cfgPort, SP_PARITY_SPACE);
    }
    else
    {
        tio_error_printf("Invalid parity");
        exit(EXIT_FAILURE);
    }
}

void tty_wait_for_device(void)
{
    int    status;
    int    timeout;
    static char input_char;
    static bool first = true;
    static DWORD last_errno = 0;

    /* Loop until device pops up */
    while (true)
    {
        if (interactive_mode)
        {
            /* In interactive mode, while waiting for tty device, we need to
             * read from stdin to react on input key commands. */
            if (first)
            {
                /* Don't wait first time */
                timeout = 0;
                first = false;
            }
            else
            {
                /* Wait up to 1 second for input */
                timeout = 1000;
            }

            pollfd_t pollfd[2];
            pollfd[0].fd = RING_GetWaitable(ring, RING_Available);
            pollfd[0].events = POLL_IN;
            pollfd[1].fd = ev_exit;
            pollfd[1].events = POLL_IN;

            /* Block until input becomes available or timeout */
            status = poll(pollfd, 2, timeout);
            if (status > 0)
            {
                /* Input from stdin ready */
                if (pollfd[0].revents & POLL_IN)
                {
                    /* Read one character */
                    status = RING_Read(ring, &input_char, 1);
                    if (status == 0)
                    {
                        tio_error_printf("Could not read from stdin");
                        exit(EXIT_FAILURE);
                    }

                    /* Handle commands */
                    handle_command_sequence(input_char, NULL, NULL);
                }
                /* Exit called */
                else if (pollfd[1].revents == POLL_IN)
                {
                    exit(EXIT_SUCCESS);
                }
            }
            else if (status == -1)
            {
                tio_error_printf("poll() failed (%s)", GetErrorMessage(GetLastError()));
                exit(EXIT_FAILURE);
            }
        }

        /* Open tty device */
        if (sp_get_port_by_name(option.tty_device, &hPort) == SP_OK)
        {
            if (sp_open(hPort, SP_MODE_READ_WRITE) == SP_OK)
            {
                last_errno = 0;
                return;
            }
            else
            {
                sp_free_port(hPort);
            }
        }

        if (last_errno != GetLastError())
        {
            tio_warning_printf("Could not open tty device (%s)", GetErrorMessage(GetLastError()));
            tio_printf("Waiting for tty device..");
            last_errno = GetLastError();
        }

        if (!interactive_mode)
        {
            /* In non-interactive mode we do not need to handle input key
             * commands so we simply sleep 1 second between checking for
             * presence of tty device */
            sleep(1);
        }
    }
}

void tty_disconnect(void)
{
    if (InterlockedCompareExchange(&connected, false, true))
    {
        tio_printf("Disconnected");

        sp_close(hPort);
        sp_free_port(hPort);

        /* Fire alert action */
        alert_disconnect();
    }
}

void tty_restore(void)
{
    sp_set_config(hPort, cfgPort_old);

    if (connected)
    {
        tty_disconnect();
    }
}

void forward_to_tty(char output_char)
{
    int status;

    /* Map output character */
    if ((output_char == 127) && (map_o_del_bs))
    {
        output_char = '\b';
    }
    if ((output_char == '\r') && (map_o_cr_nl))
    {
        output_char = '\n';
    }

    /* Map newline character */
    if ((output_char == '\n' || output_char == '\r') && (map_o_nl_crnl))
    {
        const char *crlf = "\r\n";

        optional_local_echo(crlf[0]);
        optional_local_echo(crlf[1]);
        status = tty_write(crlf, 2);
        if (status < 0)
        {
            tio_warning_printf("Could not write to tty device");
        }

        tx_total += 2;
    }
    else
    {
        if (option.hex_mode)
        {
            output_hex(output_char);
        }
        else
        {
            /* Send output to tty device */
            optional_local_echo(output_char);
            status = tty_write(&output_char, 1);
            if (status < 0)
            {
                tio_warning_printf("Could not write to tty device");
            }

            /* Update transmit statistics */
            tx_total++;
        }
    }
}

int tty_connect(void)
{
    char   input_char, output_char;
    char   input_buffer[BUFSIZ];
    static bool first = true;
    int    status;
    bool   next_timestamp = false;
    char*  now = NULL;
    bool ignore_stdin = false;

    /* Flush stale I/O data (if any) */
    sp_drain(hPort);

    /* Print connect status */
    tio_printf("Connected");
    connected = true;
    print_tainted = false;

    /* Fire alert action */
    alert_connect();

    if (option.timestamp)
    {
        next_timestamp = true;
    }

    /* Manage print output mode */
    if (option.hex_mode)
    {
        print = print_hex;
    }
    else
    {
        print = print_normal;
    }

    /* Save current port settings */
    if (sp_get_config(hPort, cfgPort_old) < 0)
    {
        tio_error_printf_silent("Could not get port settings (%s)", GetErrorMessage(GetLastError()));
        goto error_tcgetattr;
    }

    /* Make sure we restore tty settings on exit */
    if (first)
    {
        atexit(&tty_restore);
        first = false;
    }

    /* Activate new port settings */
    if (sp_set_config(hPort,cfgPort) < 0)
    {
        tio_error_printf_silent("Could not apply port settings (%s)", GetErrorMessage(GetLastError()));
        goto error_tcsetattr;
    }

    if(sp_event)
        sp_free_event_set(sp_event);
    sp_new_event_set(&sp_event);
    sp_add_port_events(sp_event, hPort, SP_EVENT_RX_READY);

    /* Manage script activation */
    if (option.script_run != SCRIPT_RUN_NEVER)
    {
        script_run();

        if (option.script_run == SCRIPT_RUN_ONCE)
        {
            option.script_run = SCRIPT_RUN_NEVER;
        }
    }

    /* Input loop */
    while (true)
    {
        pollfd_t pollfd[3];
        if (!ignore_stdin)
        {
            pollfd[2].fd = RING_GetWaitable(ring, RING_Available);
            pollfd[2].events = POLL_IN;
        }
        else
        {
            pollfd[2].fd = 0;
            pollfd[2].events = 0;
            pollfd[2].revents = 0;
        }
        pollfd[0].fd = ((HANDLE*)sp_event->handles)[0];
        pollfd[0].events = POLL_IN;
        pollfd[1].fd = ev_exit;
        pollfd[1].events = POLL_IN;

        /* Block until input becomes available */
        status = poll(pollfd, ignore_stdin ? 2 : 3,
            (option.response_wait) && (option.response_timeout != 0) ? option.response_timeout : -1);
        if (status > 0)
        {
            bool forward = false;
            if (pollfd[1].revents == POLL_IN)
            {
                /* Exit called */
                exit(EXIT_SUCCESS);
            }
            else if (pollfd[0].revents == POLL_IN)
            {
                /* Input from tty device ready */
                ssize_t bytes_read = sp_nonblocking_read(hPort, input_buffer, BUFSIZ);
                if (bytes_read < 0)
                {
                    /* Error reading - device is likely unplugged */
                    tio_error_printf_silent("Could not read from tty device");
                    goto error_read;
                }

                /* Update receive statistics */
                rx_total += bytes_read;

                /* Process input byte by byte */
                for (int i=0; i<bytes_read; i++)
                {
                    input_char = input_buffer[i];

                    /* Print timestamp on new line if enabled */
                    if ((next_timestamp && input_char != '\n' && input_char != '\r') && !option.hex_mode)
                    {
                        now = timestamp_current_time();
                        if (now)
                        {
                            ansi_printf_raw("[%s] ", now);
                            if (option.log)
                            {
                                log_printf("[%s] ", now);
                            }
                            next_timestamp = false;
                        }
                    }

                    /* Convert MSB to LSB bit order */
                    if (map_o_msblsb)
                    {
                        char ch = input_char;
                        input_char = 0;
                        for (int j = 0; j < 8; ++j)
                        {
                            input_char |= ((1 << j) & ch) ? (1 << (7 - j)) : 0;
                        }
                    }

                    /* Map input character */
                    if ((input_char == '\n') && (map_i_nl_crnl) && (!map_o_msblsb))
                    {
                        print('\r');
                        print('\n');
                        if (option.timestamp)
                        {
                            next_timestamp = true;
                        }
                    }
                    else if ((input_char == '\f') && (map_i_ff_escc) && (!map_o_msblsb))
                    {
                        print('\e');
                        print('c');
                    }
                    else
                    {
                        /* Print received tty character to stdout */
                        print(input_char);
                    }

                    /* Write to log */
                    if (option.log)
                    {
                        log_putc(input_char);
                    }

                    //socket_write(input_char);

                    print_tainted = true;

                    if (input_char == '\n' && option.timestamp)
                    {
                        next_timestamp = true;
                    }

                    if (option.response_wait)
                    {
                        if (input_char == '\n')
                        {
                             tty_sync(hPort);
                             exit(EXIT_SUCCESS);
                        }
                    }
                }
            }
            else if (pollfd[2].revents == POLL_IN)
            {
                /* Input from stdin ready */
                ssize_t bytes_read = RING_Read(ring, input_buffer, BUFSIZ);
                if (bytes_read == 0)
                {
                    tio_error_printf_silent("Could not read from stdin");
                    goto error_read;
                }

                /* Process input byte by byte */
                for (int i=0; i<bytes_read; i++)
                {
                    input_char = input_buffer[i];

                    /* Forward input to output */
                    output_char = input_char;
                    forward = true;

                    if (interactive_mode)
                    {
                        /* Do not forward prefix key */
                        if (option.prefix_enabled && input_char == option.prefix_code)
                        {
                            forward = false;
                        }

                        /* Handle commands */
                        handle_command_sequence(input_char, &output_char, &forward);

                        if ((option.hex_mode) && (forward))
                        {
                            if (!is_valid_hex(input_char))
                            {
                                tio_warning_printf("Invalid hex character: '%d' (0x%02x)", input_char, input_char);
                                forward = false;
                            }
                        }
                    }

                    if (forward)
                    {
                        forward_to_tty(output_char);
                    }
                }

                tty_sync(hPort);
            }
            else
            {
                /* Input from socket ready */
                // forward = socket_handle_input(&rdfs, &output_char);

                // if (forward)
                // {
                //     forward_to_tty(hPort, output_char);
                // }

                // tty_sync(hPort);
            }
        }
        else if (status == -1)
        {

            tio_error_printf("poll() failed (%s)", GetErrorMessage(GetLastError()));
            exit(EXIT_FAILURE);
        }
        else
        {
            // Timeout (only happens in response wait mode)
            exit(EXIT_FAILURE);
        }
    }

    return TIO_SUCCESS;

error_tcsetattr:
error_tcgetattr:
error_read:
    tty_disconnect();
    return TIO_ERROR;
}

#define MAX_PORT_NUM (256)
#define MAX_STR_LEN (256 * sizeof(char))

void list_serial_devices(void)
{
    char portName[MAX_PORT_NUM][MAX_STR_LEN];
    char friendlyName[MAX_PORT_NUM][MAX_STR_LEN];
    unsigned int i;
    unsigned int n;

    for (i = 0; i < MAX_PORT_NUM; i++)
    {
        ZeroMemory(&portName[i][0], MAX_STR_LEN);
        ZeroMemory(&friendlyName[i][0], MAX_STR_LEN);
    } /*for i[i MAX_PORT_NUM]*/

    EnumerateComPortSetupAPISetupDiClassGuidsFromNamePort(&n, &portName[0][0],
                                                          MAX_STR_LEN, &friendlyName[0][0]);

    for (i = 0; i < n; i++)
        printf(TEXT("%s\t <%s> \n"), &portName[i][0], &friendlyName[i][0]);
}
