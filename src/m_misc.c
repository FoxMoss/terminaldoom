// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// $Log:$
//
// DESCRIPTION:
//	Main loop menu stuff.
//	Default Config File.
//	PCX Screenshots.
//
//-----------------------------------------------------------------------------

// static const char
// rcsid[] = "$Id: m_misc.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctype.h>

#include "doomdef.h"

#include "z_zone.h"

#include "m_argv.h"
#include "m_swap.h"

#include "w_wad.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "hu_stuff.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

#include "m_misc.h"

//
// M_DrawText
// Returns the final X coordinate
// HU_Init must have been called to init the font
//
extern patch_t *hu_font[HU_FONTSIZE];

int M_DrawText(int x, int y, boolean direct, char *string) {
  int c;
  int w;

  while (*string) {
    c = toupper(*string) - HU_FONTSTART;
    string++;
    if (c < 0 || c > HU_FONTSIZE) {
      x += 4;
      continue;
    }

    w = SHORT(hu_font[c]->width);
    if (x + w > SCREENWIDTH)
      break;
    if (direct)
      V_DrawPatchDirect(x, y, 0, hu_font[c]);
    else
      V_DrawPatch(x, y, 0, hu_font[c]);
    x += w;
  }

  return x;
}

//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

boolean M_WriteFile(char const *name, void *source, int length) {
  return false;
}

//
// M_ReadFile
//
int M_ReadFile(char const *name, byte **buffer) { return 0; }

//
// DEFAULTS
//
int usemouse;
int usejoystick;

extern int key_right;
extern int key_left;
extern int key_up;
extern int key_down;

extern int key_strafeleft;
extern int key_straferight;

extern int key_fire;
extern int key_use;
extern int key_strafe;
extern int key_speed;

extern int mousebfire;
extern int mousebstrafe;
extern int mousebforward;

extern int joybfire;
extern int joybstrafe;
extern int joybuse;
extern int joybspeed;

extern int viewwidth;
extern int viewheight;

extern int mouseSensitivity;
extern int showMessages;

extern int detailLevel;

extern int screenblocks;

extern int showMessages;

// machine-independent sound params
extern int numChannels;

// UNIX hack, to be removed.
#ifdef SNDSERV
extern char *sndserver_filename;
extern int mb_used;
#endif

#ifdef LINUX
char *mousetype;
char *mousedev;
#endif

extern char *chat_macros[];

typedef struct {
  char *name;
  int *location;
  void *defaultvalue;
  int scantranslate; // PC scan code hack
  int untranslated;  // lousy hack
} default_t;

default_t defaults[] = {
    {"mouse_sensitivity", &mouseSensitivity, 5},
    {"sfx_volume", &snd_SfxVolume, 8},
    {"music_volume", &snd_MusicVolume, 8},
    {"show_messages", &showMessages, 1},

#ifdef NORMALUNIX
    {"key_right", &key_right, KEY_RIGHTARROW},
    {"key_left", &key_left, KEY_LEFTARROW},
    {"key_up", &key_up, KEY_UPARROW},
    {"key_down", &key_down, KEY_DOWNARROW},
    {"key_strafeleft", &key_strafeleft, ','},
    {"key_straferight", &key_straferight, '.'},

    {"key_fire", &key_fire, KEY_RCTRL},
    {"key_use", &key_use, ' '},
    {"key_strafe", &key_strafe, KEY_RALT},
    {"key_speed", &key_speed, KEY_RSHIFT},

// UNIX hack, to be removed.
#ifdef SNDSERV
    {"sndserver", (int *)&sndserver_filename, "sndserver"},
    {"mb_used", &mb_used, 2},
#endif

#endif

#ifdef LINUX
    {"mousedev", (int *)&mousedev, "/dev/ttyS0"},
    {"mousetype", (int *)&mousetype, "microsoft"},
#endif

    {"use_mouse", &usemouse, 1},
    {"mouseb_fire", &mousebfire, 0},
    {"mouseb_strafe", &mousebstrafe, 1},
    {"mouseb_forward", &mousebforward, 2},

    {"use_joystick", &usejoystick, 0},
    {"joyb_fire", &joybfire, 0},
    {"joyb_strafe", &joybstrafe, 1},
    {"joyb_use", &joybuse, 3},
    {"joyb_speed", &joybspeed, 2},

    {"screenblocks", &screenblocks, 10},
    {"detaillevel", &detailLevel, 0},

    {"snd_channels", &numChannels, 3},

    {"usegamma", &usegamma, 0},

    {"chatmacro0", (int *)&chat_macros[0], HUSTR_CHATMACRO0},
    {"chatmacro1", (int *)&chat_macros[1], HUSTR_CHATMACRO1},
    {"chatmacro2", (int *)&chat_macros[2], HUSTR_CHATMACRO2},
    {"chatmacro3", (int *)&chat_macros[3], HUSTR_CHATMACRO3},
    {"chatmacro4", (int *)&chat_macros[4], HUSTR_CHATMACRO4},
    {"chatmacro5", (int *)&chat_macros[5], HUSTR_CHATMACRO5},
    {"chatmacro6", (int *)&chat_macros[6], HUSTR_CHATMACRO6},
    {"chatmacro7", (int *)&chat_macros[7], HUSTR_CHATMACRO7},
    {"chatmacro8", (int *)&chat_macros[8], HUSTR_CHATMACRO8},
    {"chatmacro9", (int *)&chat_macros[9], HUSTR_CHATMACRO9}

};

int numdefaults;
// char*	defaultfile;

//
// M_SaveDefaults
//
void M_SaveDefaults(void) {
  int i;
  int v;
  /*
      FILE*	f;
      f = fopen (defaultfile, "w");
      if (!f)
          return; // can't write the file, but don't complain

      for (i=0 ; i<numdefaults ; i++)
      {
          if (defaults[i].defaultvalue > -0xfff
              && defaults[i].defaultvalue < 0xfff)
          {
              v = *defaults[i].location;
              fprintf (f,"%s\t\t%i\n",defaults[i].name,v);
          } else {
              fprintf (f,"%s\t\t\"%s\"\n",defaults[i].name,
                       * (char **) (defaults[i].location));
          }
      }

      fclose (f);
  */
}

//
// M_LoadDefaults
//
extern byte scantokey[128];

void M_LoadDefaults(void) {
  int i;
  int len;
  FILE *f;
  char def[80];
  char strparm[100];
  char *newstring;
  int parm;
  boolean isstring;

  // set everything to base values
  numdefaults = sizeof(defaults) / sizeof(defaults[0]);
  for (i = 0; i < numdefaults; i++)
    *defaults[i].location = defaults[i].defaultvalue;
  /*
      // check for a custom default file
      i = M_CheckParm ("-config");
      if (i && i<myargc-1)
      {
          defaultfile = myargv[i+1];
          printf ("	default file: %s\n",defaultfile);
      }
      else
          defaultfile = basedefault;

      // read the file in, overriding any set defaults
      f = fopen (defaultfile, "r");
      if (f)
      {
          while (!feof(f))
          {
              isstring = false;
              if (fscanf (f, "%79s %[^\n]\n", def, strparm) == 2)
              {
                  if (strparm[0] == '"')
                  {
                      // get a string default
                      isstring = true;
                      len = strlen(strparm);
                      newstring = (char *) malloc(len);
                      strparm[len-1] = 0;
                      strcpy(newstring, strparm+1);
                  }
                  else if (strparm[0] == '0' && strparm[1] == 'x')
                      sscanf(strparm+2, "%x", &parm);
                  else
                      sscanf(strparm, "%i", &parm);
                  for (i=0 ; i<numdefaults ; i++)
                      if (!strcmp(def, defaults[i].name))
                      {
                          if (!isstring)
                              *defaults[i].location = parm;
                          else
                              *defaults[i].location =
                                  (int) newstring;
                          break;
                      }
              }
          }

          fclose (f);
      }*/
}

//
// SCREEN SHOTS
//

typedef struct {
  char manufacturer;
  char version;
  char encoding;
  char bits_per_pixel;

  unsigned short xmin;
  unsigned short ymin;
  unsigned short xmax;
  unsigned short ymax;

  unsigned short hres;
  unsigned short vres;

  unsigned char palette[48];

  char reserved;
  char color_planes;
  unsigned short bytes_per_line;
  unsigned short palette_type;

  char filler[58];
  unsigned char data; // unbounded
} pcx_t;

//
// WritePCXfile
//
void WritePCXfile(char *filename, byte *data, int width, int height,
                  byte *palette) {}

//
// M_ScreenShot
//
void M_ScreenShot(void) {}
