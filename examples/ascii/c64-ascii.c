/*
    c64.c

    Stripped down C64 emulator running in a (xterm-256color) terminal.
*/
#include <stdint.h>
#include <stdbool.h>
#include <curses.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#define CHIPS_IMPL
#include "chips/m6502.h"
#include "chips/m6526.h"
#include "chips/m6569.h"
#include "chips/m6581.h"
#include "chips/beeper.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "systems/c64.h"
#include "c64-roms.h"

static c64_t c64;

// run the emulator and render-loop at 30fps
#define FRAME_USEC (33333)

// a signal handler for Ctrl-C, for proper cleanup 
static int quit_requested = 0;
static void catch_sigint(int signo) {
    quit_requested = 1;
}

// conversion table from C64 font index to ASCII (the 'x' is actually the pound sign)
static char font_map[65] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[x]   !\"#$%&`()*+,-./0123456789:;<=>?";

int main(int argc, char* argv[]) {
    c64_init(&c64, &(c64_desc_t){
        .rom_char = dump_c64_char,
        .rom_char_size = sizeof(dump_c64_char),
        .rom_basic = dump_c64_basic,
        .rom_basic_size = sizeof(dump_c64_basic),
        .rom_kernal = dump_c64_kernalv3,
        .rom_kernal_size = sizeof(dump_c64_kernalv3)
    });

    // install a Ctrl-C signal handler
    signal(SIGINT, catch_sigint);

    // setup curses
    initscr();
    noecho();
    curs_set(FALSE);
    cbreak();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    attron(A_BOLD);

    // run the emulation/input/render loop 
    while (!quit_requested) {
        // tick the emulator for 1 frame
        c64_exec(&c64, FRAME_USEC);

        // keyboard input
        int ch = getch();
        if (ch != ERR) {
            switch (ch) {
                case 10:  ch = 0x0D; break; // ENTER
                case 127: ch = 0x01; break; // BACKSPACE
                case 27:  ch = 0x03; break; // ESCAPE
                case 260: ch = 0x08; break; // LEFT
                case 261: ch = 0x09; break; // RIGHT
                case 259: ch = 0x0B; break; // UP
                case 258: ch = 0x0A; break; // DOWN
            }
            if (ch > 32) {
                if (islower(ch)) {
                    ch = toupper(ch);
                }
                else if (isupper(ch)) {
                    ch = tolower(ch);
                }
            }
            if (ch < 256) {
                c64_key_down(&c64, ch);
                c64_key_up(&c64, ch);
            }
        }
        // render the PETSCII buffer
        for (uint32_t y = 0; y < 25; y++) {
            for (uint32_t x = 0; x < 40; x++) {
                // get PETSCII code 
                // FIXME: compute proper SCREEN RAM address
                uint16_t addr = 0x0400 + y*40 + x;
                uint8_t font_code = mem_rd(&c64.mem_vic, addr);
                char chr = font_map[font_code & 63];
                // padding to get proper aspect ratio
                mvaddch(y, x*2, ' ');
                // character 
                mvaddch(y, x*2+1, chr);
            }
        }
        refresh();

        // pause until next frame
        usleep(FRAME_USEC);
    }
    endwin();
    return 0;
}