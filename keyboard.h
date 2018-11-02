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

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <stdint.h>

#define KEY_SPACE   0x0039
#define KEY_CONTROL 0x001d
#define KEY_LEFT    0xe04b
#define KEY_RIGHT   0xe04d
#define KEY_5       0x0006
#define KEY_1       0x0002
#define KEY_2       0x0003
#define KEY_ESCAPE  0x0001

typedef uint16_t key_t;

typedef enum {
    KEY_PRESS_EVENT=1,
    KEY_RELEASE_EVENT=0,
} keyevent_t;

typedef void (*key_event_handler_t) (const key_t key, const keyevent_t event);

void keyboard_init (key_event_handler_t handler);

#endif /* __KEYBOARD_H__ */
