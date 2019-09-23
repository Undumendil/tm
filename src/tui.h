/* 
 * Copyright (c) 2019 Daniil Fomichev <azathtoth@protonmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#include <ncurses.h>
#include <time.h>
#include "interpreter.h"

void TUI_init(void (*timer_upd)(uint8_t), uint8_t speed, bool *paused, int64_t *block);

void TUI_deinit();

void TUI_render(TMTape* tape, TMDict* states, TMDict* chars, uint64_t i);
