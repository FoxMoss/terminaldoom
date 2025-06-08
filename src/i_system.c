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
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

// static const char
// rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

#include "doomdef.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_misc.h"

#include "d_net.h"
#include "g_game.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"

#ifdef COMBINE_SCREENS
unsigned char CombinedScreens[SCREENWIDTH * SCREENHEIGHT];
#else
unsigned char CombinedScreens[SCREENWIDTH * SCREENHEIGHT * 4];
#endif

int mb_used = 6;

void I_Tactile(int on, int off, int total) {
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void) { return &emptycmd; }

unsigned char DOOMHeap[FIXED_HEAP];

int I_GetHeapSize(void) { return FIXED_HEAP; }

byte *I_ZoneBase(int *size) {
  *size = FIXED_HEAP; // mb_used*1024*1024;
  return (byte *)DOOMHeap;
}

//
// I_GetTime
// returns time in 1/70th second tics
//
int I_GetTime(void) {
  struct timeval tp;
  struct timezone tzp;
  int newtics;
  static int basetime = 0;

  gettimeofday(&tp, &tzp);
  if (!basetime)
    basetime = tp.tv_sec;
  newtics = (tp.tv_sec - basetime) * 70;
  // printf("%i\n", newtics);
  return newtics;
}

//
// I_Init
//
void I_Init(void) {
  I_InitSound();
  //  I_InitGraphics();
}

//
// I_Quit
//
void I_Quit(void) {
  D_QuitNetGame();
  I_ShutdownSound();
  I_ShutdownMusic();
  M_SaveDefaults();
  I_ShutdownGraphics();
  exit(0);
}

void I_WaitVBL(int count) {}

void I_BeginRead(void) {}

void I_EndRead(void) {}

byte *I_AllocLow(int length) {
  byte *mem;
  mem = CombinedScreens;
  memset(mem, 0, length);
  return mem;
}

//
// I_Error
//
extern boolean demorecording;

void I_Error(char *error, ...) {

  // Message first.
  printf("%s\n", error);

  // Shutdown. Here might be other errors.
  if (demorecording)
    G_CheckDemoStatus();

  D_QuitNetGame();
  I_ShutdownGraphics();

  exit(-1);
}
