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

#include "connection.h"

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

#ifdef HAVE_SDL
#include <SDL.h>
#endif

SERVER_DATA server;
CONFIGURATION config;

typedef struct {
    PSERVER_DATA server;
    CONFIGURATION *config;
    SDLContext *ctx;
} ConnectRemoteArgs;

pthread_t main_thread_id = 0;
bool connection_debug;
ConnListenerRumble rumble_handler = NULL;
ConnListenerRumbleTriggers rumble_triggers_handler = NULL;
ConnListenerSetMotionEventState set_motion_event_state_handler = NULL;
ConnListenerSetControllerLED set_controller_led_handler = NULL;

    // pair_check(&server);
    // applist(&server);
    
int applist(PSERVER_DATA server, char ***appNames) {
    PAPP_LIST list = NULL;
    int count = 0;

    if (gs_applist(server, &list) != GS_OK) {
        fprintf(stderr, "Can't get app list\n");
        return 0;
    }

    PAPP_LIST originalList = list;

    while (list != NULL) {
        count++;
        list = list->next;
    }

    *appNames = malloc(count * sizeof(char *));
    if (*appNames == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }

    list = originalList;
    for (int i = 0; list != NULL; i++) {
        (*appNames)[i] = strdup(list->name);
        if ((*appNames)[i] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return 0;
        }
        list = list->next;
    }

    return count;
}

int get_app_id(PSERVER_DATA server, const char *name) {
  PAPP_LIST list = NULL;
  if (gs_applist(server, &list) != GS_OK) {
    fprintf(stderr, "Can't get app list\n");
    return -1;
  }

  while (list != NULL) {
    if (strcmp(list->name, name) == 0)
      return list->id;

    list = list->next;
  }
  return -1;
}

void stream(PSERVER_DATA server, PCONFIGURATION config, enum platform system) {
  int appId = get_app_id(server, config->app);
  if (appId<0) {
    fprintf(stderr, "Can't find app %s\n", config->app);
    
    exit(-1);
  }
  
  fprintf(stderr, "User selected app: %d, and %s\n", appId, config->app);

  int gamepads = 0;
  gamepads += evdev_gamepads;
  #ifdef HAVE_SDL
  gamepads += sdl_gamepads;
  #endif
  int gamepad_mask = 0;
  for (int i = 0; i < gamepads; i++)
    gamepad_mask = (gamepad_mask << 1) + 1;

  int ret = gs_start_app(server, &config->stream, appId, config->sops, config->localaudio, gamepad_mask);
  if (ret < 0) {
    if (ret == GS_NOT_SUPPORTED_4K)
      fprintf(stderr, "Server doesn't support 4K\n");
    else if (ret == GS_NOT_SUPPORTED_MODE)
      fprintf(stderr, "Server doesn't support %dx%d (%d fps) or remove --nounsupported option\n", config->stream.width, config->stream.height, config->stream.fps);
    else if (ret == GS_NOT_SUPPORTED_SOPS_RESOLUTION)
      fprintf(stderr, "Optimal Playable Settings isn't supported for the resolution %dx%d, use supported resolution or add --nosops option\n", config->stream.width, config->stream.height);
    else if (ret == GS_ERROR)
      fprintf(stderr, "Gamestream error: %s\n", gs_error);
    else
      fprintf(stderr, "Errorcode starting app: %d\n", ret);
    
    exit(-1);
  }

  int drFlags = 0;
  if (config->fullscreen)
    drFlags |= DISPLAY_FULLSCREEN;

  switch (config->rotate) {
  case 0:
    break;
  case 90:
    drFlags |= DISPLAY_ROTATE_90;
    break;
  case 180:
    drFlags |= DISPLAY_ROTATE_180;
    break;
  case 270:
    drFlags |= DISPLAY_ROTATE_270;
    break;
  default:
    printf("Ignoring invalid rotation value: %d\n", config->rotate);
  }

  if (config->debug_level > 0) {
    printf("Stream %d x %d, %d fps, %d kbps\n", config->stream.width, config->stream.height, config->stream.fps, config->stream.bitrate);
    connection_debug = true;
  }

  if (IS_EMBEDDED(system))
    loop_init();

  platform_start(system);
  LiStartConnection(&server->serverInfo, &config->stream, &connection_callbacks, platform_get_video(system), platform_get_audio(system, config->audio_device), NULL, drFlags, config->audio_device, 0);

  if (IS_EMBEDDED(system)) {
    if (!config->viewonly)
      evdev_start();
    loop_main();
    if (!config->viewonly)
      evdev_stop();
  }
  #ifdef HAVE_SDL
  else if (system == SDL)
    sdl_loop(&ctx);
  #endif

  LiStopConnection();

  if (config->quitappafter) {
    if (config->debug_level > 0)
      printf("Sending app quit request ...\n");
    gs_quit_app(server);
  }


  platform_stop(system);
}

int pair_check(PSERVER_DATA server) {
  if (!server->paired) {
    fprintf(stderr, "You must pair with the PC first\n");
    ctx.state.noPairStart = 1;
    return 1;
  }
  return 0;
}

void* connectRemoteWrapper(void* args) {
    ConnectRemoteArgs* unpacked_args = (ConnectRemoteArgs*) args;
    connectRemote(unpacked_args->server, unpacked_args->config, unpacked_args->ctx);
    free(unpacked_args);
    return NULL;
}


void connectRemote(PSERVER_DATA server, CONFIGURATION *config, SDLContext *ctx) {  
    sdl_banner(ctx, "Connecting to %s...\n", "orange", config->address);        
    printf("Connecting to %s...\n", config->address);

    int ret;
    
    ret = gs_init(server, config->address, config->port, config->key_dir, config->debug_level, config->unsupported);

    if (ret == GS_OUT_OF_MEMORY) {
        printf("Not enough memory\n");
        sdl_banner(ctx, "Not enough memory", "red");
    } else if (ret == GS_ERROR) {
        sdl_banner(ctx, "Gamestream error: %s", "red", gs_error);
        printf("Gamestream error\n");
    } else if (ret == GS_INVALID) {
        sdl_banner(ctx, "Invalid data received from server: %s", "red", gs_error);
        printf("Invalid data received from server: %s\n", gs_error);
    } else if (ret == GS_UNSUPPORTED_VERSION) {
        sdl_banner(ctx, "Unsupported version: %s", "red", gs_error);
        printf("Unsupported version: %s\n", gs_error);
    } else if (ret == GS_OK) {
        sdl_banner(ctx, "Connected to server: %s", "green", config->address);
        printf("Connected to server");
    } else {
        sdl_banner(ctx, "Unable to connect!", "red");
        printf("Unable to connect!");
    }
    if (config->debug_level > 0) {
        printf("GPU: %s, GFE: %s (%s, %s)\n", server->gpuType, server->serverInfo.serverInfoGfeVersion, server->gsVersion, server->serverInfo.serverInfoAppVersion);
        printf("Server codec flags: 0x%x\n", server->serverInfo.serverCodecModeSupport);
    }
}

int launchConnectRemoteThread(PSERVER_DATA server, CONFIGURATION *config, SDLContext *ctx) {
    pthread_t connect_thread;
    ConnectRemoteArgs* args = malloc(sizeof(ConnectRemoteArgs));
    if (args == NULL) {
        printf("Pthread memory alloc. error");
        return -1;
    }

    args->server = server;
    args->config = config;
    args->ctx = ctx;

    int ret = pthread_create(&connect_thread, NULL, connectRemoteWrapper, (void*)args);
    if (ret != 0) {
        printf("Pthread creation error");
        return -2;
    }

    pthread_detach(connect_thread);

    return 0;
}

void pairClient(SDLContext *ctx) {
    char pin[5];
    
    connectRemote(&server, &config, NULL);
    
    if (config.pin > 0 && config.pin <= 9999) {
      sprintf(pin, "%04d", config.pin);
    } else {
      sprintf(pin, "%d%d%d%d", (unsigned)random() % 10, (unsigned)random() % 10, (unsigned)random() % 10, (unsigned)random() % 10);
    }
    printf("Please enter the following PIN on the target PC: %s\n", pin);
    
    sdl_banner(ctx, "Enter the pin on the remote! %s", "orange", pin);

    fflush(stdout);
    if (gs_pair(&server, &pin[0]) != GS_OK) {
      fprintf(stderr, "Failed to pair to server: %s\n", gs_error);
      sdl_banner(ctx, "Failed: *s", "red", gs_error);
    } else {
      printf("Successfully paired\n");
      sdl_banner(ctx, "Successfully paired!", "green");
    }
}

void unPairClient(SDLContext *ctx) {
    
    int pair_eval = pair_check(&server);
    if (pair_eval == 1) {
        sdl_banner(ctx, "You must pair first!", "red");
        return;
    }
    char pairdone_path[128], cache_path[128];
    connectRemote(&server, &config, NULL);

    
    if (gs_unpair(&server) != GS_OK) {
        fprintf(stderr, "Failed to unpair to server: %s\n", gs_error);
        sdl_banner(ctx, "Failed: *s", "red", gs_error);
    } else {
        server.paired = 0;
        snprintf(pairdone_path, sizeof(pairdone_path), "%s/config/pairdone", MOONLIGHT_DIR);
        snprintf(cache_path, sizeof(cache_path), "%s/.cache", MOONLIGHT_DIR);

        is_file_exist_and_remove(pairdone_path);
        is_dir_exist_and_remove(cache_path);
        is_file_exist_and_remove("/tmp/launch");
        
        printf("Successfully unpaired\n");
        sdl_banner(ctx, "Successfully unpaired!", "green");
    }
}

void handleStreaming(PSERVER_DATA server, CONFIGURATION *config) {
    pair_check(server);
    enum platform system = platform_check(config->platform);
    if (config->debug_level > 0)
      printf("Platform %s\n", platform_name(system));

    if (system == 0) {
      fprintf(stderr, "Platform '%s' not found\n", config->platform);
      
      exit(-1);
    } else if (system == SDL && config->audio_device != NULL) {
      fprintf(stderr, "You can't select a audio device for SDL\n");
      
      exit(-1);
    }

    config->stream.supportedVideoFormats = VIDEO_FORMAT_H264;
    if (config->codec == CODEC_HEVC || (config->codec == CODEC_UNSPECIFIED && platform_prefers_codec(system, CODEC_HEVC))) {
      config->stream.supportedVideoFormats |= VIDEO_FORMAT_H265;
      if (config->hdr)
        config->stream.supportedVideoFormats |= VIDEO_FORMAT_H265_MAIN10;
    }
    if (config->codec == CODEC_AV1 || (config->codec == CODEC_UNSPECIFIED && platform_prefers_codec(system, CODEC_AV1))) {
      config->stream.supportedVideoFormats |= VIDEO_FORMAT_AV1_MAIN8;
      if (config->hdr)
        config->stream.supportedVideoFormats |= VIDEO_FORMAT_AV1_MAIN10;
    }

    if (config->hdr && !(config->stream.supportedVideoFormats & VIDEO_FORMAT_MASK_10BIT)) {
      fprintf(stderr, "HDR streaming requires HEVC or AV1 codec\n");
      
      exit(-1);
    }

    if (config->viewonly) {
      if (config->debug_level > 0)
        printf("View-only mode enabled, no input will be sent to the host computer\n");
    } else {
      if (IS_EMBEDDED(system)) {
        char* mapping_env = getenv("SDL_GAMECONTROLLERCONFIG");
        if (config->mapping == NULL && mapping_env == NULL) {
          fprintf(stderr, "Please specify mapping file as default mapping could not be found.\n");
          cleanupSDLContext(&ctx);
          exit(-1);
        }

        struct mapping* mappings = NULL;
        if (config->mapping != NULL)
          mappings = mapping_load(config->mapping, config->debug_level > 0);

        if (mapping_env != NULL) {
          struct mapping* map = mapping_parse(mapping_env);
          map->next = mappings;
          mappings = map;
        }

        for (int i=0;i<config->inputsCount;i++) {
          if (config->debug_level > 0)
            printf("Adding input device %s...\n", config->inputs[i]);

          evdev_create(config->inputs[i], mappings, config->debug_level > 0, config->rotate);
        }

        udev_init(!inputAdded, mappings, config->debug_level > 0, config->rotate);
        evdev_init(config->mouse_emulation);
        rumble_handler = evdev_rumble;
        #ifdef HAVE_LIBCEC
        cec_init();
        #endif /* HAVE_LIBCEC */
      }
      #ifdef HAVE_SDL
      else if (system == SDL) {
        if (config->inputsCount > 0) {
          fprintf(stderr, "You can't select input devices as SDL will automatically use all available controllers\n");
          
          exit(-1);
        }

        sdlinput_init(config->mapping);
        rumble_handler = sdlinput_rumble;
        rumble_triggers_handler = sdlinput_rumble_triggers;
        set_motion_event_state_handler = sdlinput_set_motion_event_state;
        set_controller_led_handler = sdlinput_set_controller_led;
      }
      #endif
    }
    
    stream(server, config, system);
}

static void connection_terminated(int errorCode) {
  switch (errorCode) {
  case ML_ERROR_GRACEFUL_TERMINATION:
    printf("Connection has been terminated gracefully.\n");
    break;
  case ML_ERROR_NO_VIDEO_TRAFFIC:
    printf("No video received from host. Check the host PC's firewall and port forwarding rules.\n");
    break;
  case ML_ERROR_NO_VIDEO_FRAME:
    printf("Your network connection isn't performing well. Reduce your video bitrate setting or try a faster connection.\n");
    break;
  case ML_ERROR_UNEXPECTED_EARLY_TERMINATION:
    printf("The connection was unexpectedly terminated by the host due to a video capture error. Make sure no DRM-protected content is playing on the host.\n");
    break;
  case ML_ERROR_PROTECTED_CONTENT:
    printf("The connection was terminated by the host due to DRM-protected content. Close any DRM-protected content on the host and try again.\n");
    break;
  default:
    printf("Connection terminated with error: %d\n", errorCode);
    break;
  }

  #ifdef HAVE_SDL
      SDL_Event event;
      event.type = SDL_QUIT;
      SDL_PushEvent(&event);
  #endif

  if (main_thread_id != 0)
    pthread_kill(main_thread_id, SIGTERM);
}

static void connection_log_message(const char* format, ...) {
  va_list arglist;
  va_start(arglist, format);
  vprintf(format, arglist);
  va_end(arglist);
}

static void rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor) {
  if (rumble_handler)
    rumble_handler(controllerNumber, lowFreqMotor, highFreqMotor);
}

static void rumble_triggers(unsigned short controllerNumber, unsigned short leftTrigger, unsigned short rightTrigger) {
  if (rumble_handler)
    rumble_triggers_handler(controllerNumber, leftTrigger, rightTrigger);
}

static void set_motion_event_state(unsigned short controllerNumber, unsigned char motionType, unsigned short reportRateHz) {
  if (set_motion_event_state_handler)
    set_motion_event_state_handler(controllerNumber, motionType, reportRateHz);
}

static void set_controller_led(unsigned short controllerNumber, unsigned char r, unsigned char g, unsigned char b) {
  if (set_controller_led_handler)
    set_controller_led_handler(controllerNumber, r, g, b);
}

static void connection_status_update(int status) {
  switch (status) {
    case CONN_STATUS_OKAY:
      printf("Connection is okay\n");
      break;
    case CONN_STATUS_POOR:
      printf("Connection is poor\n");
      break;
  }
}

CONNECTION_LISTENER_CALLBACKS connection_callbacks = {
  .stageStarting = NULL,
  .stageComplete = NULL,
  .stageFailed = NULL,
  .connectionStarted = NULL,
  .connectionTerminated = connection_terminated,
  .logMessage = connection_log_message,
  .rumble = rumble,
  .connectionStatusUpdate = connection_status_update,
  .setHdrMode = NULL,
  .rumbleTriggers = rumble_triggers,
  .setMotionEventState = set_motion_event_state,
  .setControllerLED = set_controller_led,
};
