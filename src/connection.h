/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
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

#include "loop.h"
#include "configuration.h"
#include "platform.h"
#include "config.h"
#include "sdl.h"
#include "util.h"

#include "audio/audio.h"
#include "video/video.h"
#include "input/mapping.h"
#include "input/evdev.h"
#include "input/udev.h"
#include <Limelight.h>

#include <client.h>
#include <discover.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/rand.h>

extern SERVER_DATA server;
extern CONFIGURATION config;

extern CONNECTION_LISTENER_CALLBACKS connection_callbacks;
extern pthread_t main_thread_id;
extern bool connection_debug;
extern ConnListenerRumble rumble_handler;
extern ConnListenerRumbleTriggers rumble_triggers_handler;
extern ConnListenerSetMotionEventState set_motion_event_state_handler;
extern ConnListenerSetControllerLED set_controller_led_handler;

int launchConnectRemoteThread(PSERVER_DATA server, CONFIGURATION *config, SDLContext *ctx);
int applist(PSERVER_DATA server, char ***appNames);
int get_app_id(PSERVER_DATA server, const char *name);
void stream(PSERVER_DATA server, PCONFIGURATION config, enum platform system);
int pair_check(PSERVER_DATA server);
void connectRemote(PSERVER_DATA server, CONFIGURATION *config, SDLContext *ctx);
void pairClient(SDLContext *ctx);
void unPairClient(SDLContext *ctx);
void handleStreaming(PSERVER_DATA server, CONFIGURATION *config);