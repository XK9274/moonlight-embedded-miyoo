/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2017 Iwan Timmer
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

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

typedef void (*OutputCallback)(SDLContext *ctx, const char *line);

int read_and_match_output(FILE *fp, SDLContext *ctx, int operation_type) {
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("Received reply: %s", buffer);

        if (operation_type == 1) {
            if (strstr(buffer, "Successfully unpaired") != NULL) {
                ctx->state.unPairedNoti = 1;
                printf("Matched Successfully unpaired\n");
                return 1;
            } else if (strstr(buffer, "Failed to unpair to server") != NULL) {
                ctx->state.unPairedNoti = 2;
                printf("Matched Failed to unpair to server\n");
                return 2;
            }
        } else if (operation_type == 2) {
            const char *pin_prefix = "Please enter the following PIN on the target PC: ";
            if (strstr(buffer, pin_prefix) != NULL) {
                strncpy(ctx->state.received_pin, buffer + strlen(pin_prefix), 4);
                ctx->state.received_pin[4] = '\0';
                
                printf("Received PIN: %s\n", ctx->state.received_pin);
                char banner_msg[128];
                snprintf(banner_msg, sizeof(banner_msg), "On the host, enter the pin %s", ctx->state.received_pin);
                sdl_banner(ctx, banner_msg, "orange");
            } else if (strstr(buffer, "Successfully paired") != NULL) {
                printf("Matched Successfully paired\n");
                return 3;
            }
        }
    }
    return 0;
}

void unpair_callback(SDLContext *ctx, const char *line) {
    if (strstr(line, "Successfully unpaired") != NULL) {
        ctx->state.unPairedNoti = 1;
        printf("Matched Successfully unpaired\n");
    } else if (strstr(line, "Failed to unpair to server") != NULL) {
        ctx->state.unPairedNoti = 2;
        printf("Matched Failed to unpair to server\n");
    }
}

void pair_callback(SDLContext *ctx, const char *buffer) {
    const char *pin_prefix = "Please enter the following PIN on the target PC: ";
    
    if (strstr(buffer, pin_prefix) != NULL) {
        strncpy(ctx->state.received_pin, buffer + strlen(pin_prefix), 4);
        ctx->state.received_pin[4] = '\0';
        printf("Received PIN: %s\n", ctx->state.received_pin);
    }
}

void is_file_exist_and_remove(const char *filepath) {
    if (access(filepath, F_OK) != -1) {
        remove(filepath);
    }
}

void is_dir_exist_and_remove(const char *dirpath) {
    if (access(dirpath, F_OK) != -1) {
        char command[256];
        snprintf(command, sizeof(command), "rm -rf %s", dirpath);
        system(command);
    }
}

int write_bool(char *path, bool val) {
  int fd = open(path, O_RDWR);

  if(fd >= 0) {
    int ret = write(fd, val ? "1" : "0", 1);
    if (ret < 0)
      fprintf(stderr, "Failed to write %d to %s: %d\n", val ? 1 : 0, path, ret);

    close(fd);
    return 0;
  } else
    return -1;
}

int read_file(char *path, char* output, int output_len) {
  int fd = open(path, O_RDONLY);

  if(fd >= 0) {
    output_len = read(fd, output, output_len);
    close(fd);
    return output_len;
  } else
    return -1;
}

bool ensure_buf_size(void **buf, size_t *buf_size, size_t required_size) {
  if (*buf_size >= required_size)
    return false;

  *buf_size = required_size;
  *buf = realloc(*buf, *buf_size);
  if (!*buf) {
    fprintf(stderr, "Failed to allocate %zu bytes\n", *buf_size);
    abort();
  }

  return true;
}