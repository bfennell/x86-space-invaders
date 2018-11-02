/*
  Copyright (c) 2018 Brendan Fennell <bfennell@skynet.ie>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "x86.h"

#include "keyboard.h"

struct key {
    uint16_t code;
    unsigned press_cnt;
    unsigned release_cnt;
};

static struct key key_table[] = {
    {KEY_SPACE,   0, 0},
    {KEY_CONTROL, 0, 0},
    {KEY_LEFT,    0, 0},
    {KEY_RIGHT,   0, 0},
    {KEY_5,       0, 0},
    {KEY_1,       0, 0},
    {KEY_2,       0, 0},
    {KEY_ESCAPE,  0, 0},
};

static uint32_t keycode;

static key_event_handler_t key_event_handler;

static inline bool is_2byte_key (const uint8_t key)
{
    return (key == 0xe0);
}

static inline struct key* find_key (const uint16_t keycode)
{
    for (unsigned i = 0; i < (sizeof(key_table)/sizeof(key_table[0])); ++i) {
        if ((keycode & ~0x80) == key_table[i].code) {
            return &key_table[i];
        }
    }
    return NULL;
}

static inline int count_pressed (void)
{
    int count = 0;
    for (unsigned i = 0; i < (sizeof(key_table)/sizeof(key_table[0])); ++i) {
        if (key_table[i].press_cnt > key_table[i].release_cnt) {
            count++;
        }
    }
    return count;
}

static inline bool is_key_release (const uint16_t keycode)
{
    return ((keycode & 0x80) != 0);
}

void keyboard_irq_handler (void)
{
    uint8_t status = inport8 (0x64);
    if (status & 1) {
        uint8_t k = inport8 (0x60);

        keycode <<= 8;
        keycode |= k;

        if (is_2byte_key(k)) {
            /* wait for second keycode byte */
            return;
        }

        struct key* key = find_key (keycode);
        if (key == NULL) {
            /* ignore all other keys */
            keycode = 0;
            return;
        }

        if (is_key_release(keycode)) {
            key->release_cnt++;
        } else {
            key->press_cnt++;
        }

        if (count_pressed() > 1) {
            /* missed release events: release all */
            for (unsigned i = 0; i < (sizeof(key_table)/sizeof(key_table[0])); ++i) {
                if (key_table[i].press_cnt > key_table[i].release_cnt) {
                    key_event_handler (key_table[i].code, KEY_RELEASE_EVENT);
                    key_table[i].press_cnt = key_table[i].release_cnt;
                }
            }

            if (!is_key_release(keycode)) {
                key->press_cnt++;
            }
        }

        /* normal press/release */
        if (is_key_release (keycode)) {
            key_event_handler (key->code, KEY_RELEASE_EVENT);
        } else {
            key_event_handler (key->code, KEY_PRESS_EVENT);
        }

        keycode = 0;
    }
}

void keyboard_init (key_event_handler_t handler)
{
    key_event_handler = handler;
}
