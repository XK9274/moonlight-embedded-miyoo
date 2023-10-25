/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2019 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "loop.h"
#include "sdl.h"
#include "platform.h"
#include "config.h"
#include "configuration.h"

int main(int argc, char* argv[]) {
    printf("Moonlight Embedded %d.%d.%d (%s)\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, COMPILE_OPTIONS);
    
    config_default(config);
    config_file_parse(MOONLIGHT_CONF, &config);
    
    sdl_init(&ctx, 640, 480, true);
    
    if (config.address != NULL) {
        free(config.address);
        config.address = NULL;
    }
    
    return 0;
}