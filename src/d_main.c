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
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//
//-----------------------------------------------------------------------------

// static const char rcsid[] = "$Id: d_main.c,v 1.8 1997/02/03 22:45:09 b1 Exp
// $";

#define BGCOLOR 7
#define FGCOLOR 8

#ifdef NORMALUNIX
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "sounds.h"

#include "s_sound.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "f_finale.h"
#include "f_wipe.h"

#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"

#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"

#include "g_game.h"

#include "am_map.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "wi_stuff.h"

#include "p_setup.h"
#include "r_local.h"

#include "d_main.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop(void);

// char*		wadfiles[MAXWADFILES];

boolean devparm;     // started game with -devparm
boolean nomonsters;  // checkparm of -nomonsters
boolean respawnparm; // checkparm of -respawn
boolean fastparm;    // checkparm of -fast

boolean drone;

boolean singletics = false; // debug flag to cancel adaptiveness

// extern int soundVolume;
// extern  int	sfxVolume;
// extern  int	musicVolume;

extern boolean inhelpscreens;

skill_t startskill;
int startepisode;
int startmap;
boolean autostart;

FILE *debugfile;

boolean advancedemo;

void D_CheckNetGame(void);
void D_ProcessEvents(void);
void G_BuildTiccmd(ticcmd_t *cmd);
void D_DoAdvanceDemo(void);

//
// EVENT HANDLING
//
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//
event_t events[MAXEVENTS];
int eventhead;
int eventtail;

//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent(event_t *ev) {
  events[eventhead] = *ev;
  eventhead = (++eventhead) & (MAXEVENTS - 1);
}

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents(void) {
  event_t *ev;

  // IF STORE DEMO, DO NOT ACCEPT INPUT
  if ((gamemode == commercial) && (W_CheckNumForName("map01") < 0))
    return;

  for (; eventtail != eventhead; eventtail = (++eventtail) & (MAXEVENTS - 1)) {
    ev = &events[eventtail];
    if (M_Responder(ev))
      continue; // menu ate the event
    G_Responder(ev);
  }
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean setsizeneeded;
extern int showMessages;
void R_ExecuteSetViewSize(void);

void D_Display(void) {
  static boolean viewactivestate = false;
  static boolean menuactivestate = false;
  static boolean inhelpscreensstate = false;
  static boolean fullscreen = false;
  static gamestate_t oldgamestate = -1;
  static int borderdrawcount;
  int nowtime;
  int tics;
  int wipestart;
  int y;
  boolean done;
  boolean wipe;
  boolean redrawsbar;

  if (nodrawers)
    return; // for comparative timing / profiling

  redrawsbar = false;

  // change the view size if needed
  if (setsizeneeded) {
    R_ExecuteSetViewSize();
    oldgamestate = -1; // force background redraw
    borderdrawcount = 3;
  }

  // save the current screen if about to wipe
  if (gamestate != wipegamestate) {
    wipe = true;
    wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
  } else
    wipe = false;

  if (gamestate == GS_LEVEL && gametic)
    HU_Erase();

  // do buffered drawing
  switch (gamestate) {
  case GS_LEVEL:
    if (!gametic)
      break;
    if (automapactive)
      AM_Drawer();
    if (wipe || (viewheight != 200 && fullscreen))
      redrawsbar = true;
    if (inhelpscreensstate && !inhelpscreens)
      redrawsbar = true; // just put away the help screen
    ST_Drawer(viewheight == 200, redrawsbar);
    fullscreen = viewheight == 200;
    break;

  case GS_INTERMISSION:
    WI_Drawer();
    break;

  case GS_FINALE:
    F_Drawer();
    break;

  case GS_DEMOSCREEN:
    D_PageDrawer();
    break;
  }

  // draw buffered stuff to screen
  I_UpdateNoBlit();

  // draw the view directly
  if (gamestate == GS_LEVEL && !automapactive && gametic)
    R_RenderPlayerView(&players[displayplayer]);

  if (gamestate == GS_LEVEL && gametic)
    HU_Drawer();

  // clean up border stuff
  if (gamestate != oldgamestate && gamestate != GS_LEVEL)
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));

  // see if the border needs to be initially drawn
  if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL) {
    viewactivestate = false; // view was not active
    R_FillBackScreen();      // draw the pattern into the back screen
  }

  // see if the border needs to be updated to the screen
  if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != 320) {
    if (menuactive || menuactivestate || !viewactivestate)
      borderdrawcount = 3;
    if (borderdrawcount) {
      R_DrawViewBorder(); // erase old menu stuff
      borderdrawcount--;
    }
  }

  menuactivestate = menuactive;
  viewactivestate = viewactive;
  inhelpscreensstate = inhelpscreens;
  oldgamestate = wipegamestate = gamestate;

  // draw pause pic
  if (paused) {
    if (automapactive)
      y = 4;
    else
      y = viewwindowy + 4;
    V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y, 0,
                      W_CacheLumpName("M_PAUSE", PU_CACHE));
  }

  // menus go directly to the screen
  M_Drawer();  // menu is drawn even on top of everything
  NetUpdate(); // send out any new accumulation

  // normal update
  if (!wipe) {
    I_FinishUpdate(); // page flip or blit buffer
    return;
  }

  // wipe update
  wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

  wipestart = I_GetTime() - 1;

  do {
    do {
      nowtime = I_GetTime();
      tics = nowtime - wipestart;
    } while (!tics);
    wipestart = nowtime;
    done = wipe_ScreenWipe(wipe_Melt, 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
    I_UpdateNoBlit();
    M_Drawer();       // menu is drawn even on top of wipes
    I_FinishUpdate(); // page flip or blit buffer
  } while (!done);
}

//
//  D_DoomLoop
//
extern boolean demorecording;

void D_DoomLoop(void) {
  if (demorecording)
    G_BeginRecording();

  if (M_CheckParm("-debugfile")) {
    char filename[20];
    sprintf(filename, "debug%i.txt", consoleplayer);
    printf("debug output to: %s\n", filename);
    debugfile = fopen(filename, "w");
  }

  I_InitGraphics();

  while (1) {
    // frame syncronous IO operations
    I_StartFrame();

    // process one or more tics
    if (singletics) {
      D_ProcessEvents();
      G_BuildTiccmd(&netcmds[consoleplayer][maketic % BACKUPTICS]);
      if (advancedemo)
        D_DoAdvanceDemo();
      M_Ticker();
      G_Ticker();
      gametic++;
      maketic++;
    } else {
      TryRunTics(); // will run at least one tic
    }

    S_UpdateSounds(players[consoleplayer].mo); // move positional sounds

    // Update display, next frame, with current state.
    D_Display();

#ifndef SNDSERV
    // Sound mixing for the buffer is snychronous.
    I_UpdateSound();
#endif
    // Synchronous sound output is explicitly called.
#ifndef SNDINTR
    // Update sound output.
    I_SubmitSound();
#endif
  }
}

//
//  DEMO LOOP
//
int demosequence;
int pagetic;
char *pagename;

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void) {
  if (--pagetic < 0)
    D_AdvanceDemo();
}

//
// D_PageDrawer
//
void D_PageDrawer(void) {
  V_DrawPatch(0, 0, 0, W_CacheLumpName(pagename, PU_CACHE));
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo(void) { advancedemo = true; }

//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
void D_DoAdvanceDemo(void) {
  players[consoleplayer].playerstate = PST_LIVE; // not reborn
  advancedemo = false;
  usergame = false; // no save / end game here
  paused = false;
  gameaction = ga_nothing;

  if (gamemode == retail)
    demosequence = (demosequence + 1) % 7;
  else
    demosequence = (demosequence + 1) % 6;

  switch (demosequence) {
  case 0:
    if (gamemode == commercial)
      pagetic = 35 * 11;
    else
      pagetic = 170;
    gamestate = GS_DEMOSCREEN;
    pagename = "TITLEPIC";
    if (gamemode == commercial)
      S_StartMusic(mus_dm2ttl);
    else
      S_StartMusic(mus_intro);
    break;
  case 1:
    G_DeferedPlayDemo("demo1");
    break;
  case 2:
    pagetic = 200;
    gamestate = GS_DEMOSCREEN;
    pagename = "CREDIT";
    break;
  case 3:
    G_DeferedPlayDemo("demo2");
    break;
  case 4:
    gamestate = GS_DEMOSCREEN;
    if (gamemode == commercial) {
      pagetic = 35 * 11;
      pagename = "TITLEPIC";
      S_StartMusic(mus_dm2ttl);
    } else {
      pagetic = 200;

      if (gamemode == retail)
        pagename = "CREDIT";
      else
        pagename = "HELP2";
    }
    break;
  case 5:
    G_DeferedPlayDemo("demo3");
    break;
    // THE DEFINITIVE DOOM Special Edition demo
  case 6:
    G_DeferedPlayDemo("demo4");
    break;
  }
}

//
// D_StartTitle
//
void D_StartTitle(void) {
  gameaction = ga_nothing;
  demosequence = -1;
  D_AdvanceDemo();
}

//      print title for every printed line
char title[128];

//
// IdentifyVersion
// Checks availability of IWAD files by name,
// to determine whether registered/commercial features
// should be executed (notably loading PWAD's).
//
void IdentifyVersion(void) {
  gamemode = shareware;
  // D_AddFile ("doom1.wad");
}

//
// D_DoomMain
//
void D_DoomMain(void) {
  int p;
  char file[256];

  IdentifyVersion();

  modifiedgame = false;

  nomonsters = M_CheckParm("-nomonsters");
  respawnparm = M_CheckParm("-respawn");
  fastparm = M_CheckParm("-fast");
  devparm = M_CheckParm("-devparm");
  if (M_CheckParm("-altdeath"))
    deathmatch = 2;
  else if (M_CheckParm("-deathmatch"))
    deathmatch = 1;

  switch (gamemode) {
  case retail:
    sprintf(title,
            "                         "
            "The Ultimate DOOM Startup v%i.%i"
            "                           ",
            VERSION / 100, VERSION % 100);
    break;
  case shareware:
    sprintf(title,
            "                            "
            "DOOM Shareware Startup v%i.%i"
            "                           ",
            VERSION / 100, VERSION % 100);
    break;
  case registered:
    sprintf(title,
            "                            "
            "DOOM Registered Startup v%i.%i"
            "                           ",
            VERSION / 100, VERSION % 100);
    break;
  case commercial:
    sprintf(title,
            "                         "
            "DOOM 2: Hell on Earth v%i.%i"
            "                           ",
            VERSION / 100, VERSION % 100);
    break;
    /*FIXME
           case pack_plut:
            sprintf (title,
                     "                   "
                     "DOOM 2: Plutonia Experiment v%i.%i"
                     "                           ",
                     VERSION/100,VERSION%100);
            break;
          case pack_tnt:
            sprintf (title,
                     "                     "
                     "DOOM 2: TNT - Evilution v%i.%i"
                     "                           ",
                     VERSION/100,VERSION%100);
            break;
    */
  default:
    sprintf(title,
            "                     "
            "Public DOOM - v%i.%i"
            "                           ",
            VERSION / 100, VERSION % 100);
    break;
  }

  printf("%s\n", title);

  if (devparm)
    printf(D_DEVSTR);

  // turbo option
  if ((p = M_CheckParm("-turbo"))) {
    int scale = 200;
    extern int forwardmove[2];
    extern int sidemove[2];

    if (p < myargc - 1)
      scale = atoi(myargv[p + 1]);
    if (scale < 10)
      scale = 10;
    if (scale > 400)
      scale = 400;
    printf("turbo scale: %i%%\n", scale);
    forwardmove[0] = forwardmove[0] * scale / 100;
    forwardmove[1] = forwardmove[1] * scale / 100;
    sidemove[0] = sidemove[0] * scale / 100;
    sidemove[1] = sidemove[1] * scale / 100;
  }

  // add any files specified on the command line with -file wadfile
  // to the wad list
  //
  // convenience hack to allow -wart e m to add a wad file
  // prepend a tilde to the filename so wadfile will be reloadable
  p = M_CheckParm("-wart");
  if (p) {
    myargv[p][4] = 'p'; // big hack, change to -warp

    // Map name handling.
    switch (gamemode) {
    case shareware:
    case retail:
    case registered:
      sprintf(file, "~" DEVMAPS "E%cM%c.wad", myargv[p + 1][0],
              myargv[p + 2][0]);
      printf("Warping to Episode %s, Map %s.\n", myargv[p + 1], myargv[p + 2]);
      break;

    case commercial:
    default:
      p = atoi(myargv[p + 1]);
      if (p < 10)
        sprintf(file, "~" DEVMAPS "cdata/map0%i.wad", p);
      else
        sprintf(file, "~" DEVMAPS "cdata/map%i.wad", p);
      break;
    }
    // D_AddFile (file);
  }

  /*
      p = M_CheckParm ("-file");
      if (p)
      {
          // the parms after p are wadfile/lump names,
          // until end of parms or another - preceded parm
          modifiedgame = true;            // homebrew levels
          while (++p != myargc && myargv[p][0] != '-')
              D_AddFile (myargv[p]);
      }

      p = M_CheckParm ("-playdemo");

      if (!p)
          p = M_CheckParm ("-timedemo");

      if (p && p < myargc-1)
      {
          sprintf (file,"%s.lmp", myargv[p+1]);
          D_AddFile (file);
          printf("Playing demo %s.lmp.\n",myargv[p+1]);
      }
  */
  // get skill / episode / map from parms
  startskill = sk_medium;
  startepisode = 1;
  startmap = 1;
  autostart = false;

  p = M_CheckParm("-skill");
  if (p && p < myargc - 1) {
    startskill = myargv[p + 1][0] - '1';
    autostart = true;
  }

  p = M_CheckParm("-episode");
  if (p && p < myargc - 1) {
    startepisode = myargv[p + 1][0] - '0';
    startmap = 1;
    autostart = true;
  }

  p = M_CheckParm("-timer");
  if (p && p < myargc - 1 && deathmatch) {
    int time;
    time = atoi(myargv[p + 1]);
    printf("Levels will end after %d minute", time);
    if (time > 1)
      printf("s");
    printf(".\n");
  }

  p = M_CheckParm("-avg");
  if (p && p < myargc - 1 && deathmatch)
    printf("Austin Virtual Gaming: Levels will end after 20 minutes\n");

#if E1M1ONLY
  p = 1;
  startepisode = 1;
  startmap = 1;
  autostart = true;
#else
  p = M_CheckParm("-warp");
  if (p && p < myargc - 1) {
    if (gamemode == commercial)
      startmap = atoi(myargv[p + 1]);
    else {
      startepisode = myargv[p + 1][0] - '0';
      startmap = myargv[p + 2][0] - '0';
    }
    autostart = true;
  }
#endif

  // init subsystems
  printf("V_Init: allocate screens.\n");
  V_Init();

  printf("M_LoadDefaults: Load system defaults.\n");
  M_LoadDefaults(); // load before initing other systems

  printf("Z_Init: Init zone memory allocation daemon. \n");
  Z_Init();

  //    printf ("W_Init: Init WADfiles.\n");
  //    W_InitMultipleFiles (wadfiles);

  // Check for -file in shareware
  if (modifiedgame) {
    // These are the lumps that will be checked in IWAD,
    // if any one is not present, execution will be aborted.
    char name[23][8] = {"e2m1",   "e2m2",   "e2m3",    "e2m4",   "e2m5",
                        "e2m6",   "e2m7",   "e2m8",    "e2m9",   "e3m1",
                        "e3m3",   "e3m3",   "e3m4",    "e3m5",   "e3m6",
                        "e3m7",   "e3m8",   "e3m9",    "dphoof", "bfgga0",
                        "heada1", "cybra1", "spida1d1"};
    int i;

    if (gamemode == shareware)
      I_Error("\nYou cannot -file with the shareware "
              "version. Register!");

    // Check for fake IWAD with right name,
    // but w/o all the lumps of the registered version.
    if (gamemode == registered)
      for (i = 0; i < 23; i++)
        if (W_CheckNumForName(name[i]) < 0)
          I_Error("\nThis is not the registered version.");
  }

  // Iff additonal PWAD files are used, print modified banner
  if (modifiedgame) {
    /*m*/ printf(
        "======================================================================"
        "=====\n"
        "ATTENTION:  This version of DOOM has been modified.  If you would "
        "like to\n"
        "get a copy of the original game, call 1-800-IDGAMES or see the readme "
        "file.\n"
        "        You will not receive technical support for modified games.\n"
        "                      press enter to continue\n"
        "======================================================================"
        "=====\n");
    getchar();
  }

  // Check and print which version is executed.
  switch (gamemode) {
  case shareware:
  case indetermined:
    printf("==================================================================="
           "========\n"
           "                                Shareware!\n"
           "==================================================================="
           "========\n");
    break;
  case registered:
  case retail:
  case commercial:
    printf("==================================================================="
           "========\n"
           "                 Commercial product - do not distribute!\n"
           "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
           "==================================================================="
           "========\n");
    break;

  default:
    // Ouch.
    break;
  }

  printf("M_Init: Init miscellaneous info.\n");
  M_Init();

  printf("R_Init: Init DOOM refresh daemon - ");
  R_Init();

  printf("\nP_Init: Init Playloop state.\n");
  P_Init();

  printf("I_Init: Setting up machine state.\n");
  I_Init();

  printf("D_CheckNetGame: Checking network game status.\n");
  D_CheckNetGame();

  printf("S_Init: Setting up sound.\n");
  S_Init(snd_SfxVolume /* *8 */, snd_MusicVolume /* *8*/);

  printf("HU_Init: Setting up heads up display.\n");
  HU_Init();

  printf("ST_Init: Init status bar.\n");
  ST_Init();

  // check for a driver that wants intermission stats
  p = M_CheckParm("-statcopy");
  if (p && p < myargc - 1) {
    // for statistics driver
    extern void *statcopy;

    statcopy = (void *)atoi(myargv[p + 1]);
    printf("External statistics registered.\n");
  }

  // start the apropriate game based on parms
  p = M_CheckParm("-record");

  if (p && p < myargc - 1) {
    G_RecordDemo(myargv[p + 1]);
    autostart = true;
  }

  p = M_CheckParm("-playdemo");
  if (p && p < myargc - 1) {
    singledemo = true; // quit after one demo
    G_DeferedPlayDemo(myargv[p + 1]);
    D_DoomLoop(); // never returns
  }

  p = M_CheckParm("-timedemo");
  if (p && p < myargc - 1) {
    G_TimeDemo(myargv[p + 1]);
    D_DoomLoop(); // never returns
  }

  p = M_CheckParm("-loadgame");
  if (p && p < myargc - 1) {
    if (M_CheckParm("-cdrom"))
      sprintf(file, "c:\\doomdata\\" SAVEGAMENAME "%c.dsg", myargv[p + 1][0]);
    else
      sprintf(file, SAVEGAMENAME "%c.dsg", myargv[p + 1][0]);
    G_LoadGame(file);
  }

  if (gameaction != ga_loadgame) {
    if (autostart || netgame)
      G_InitNew(startskill, startepisode, startmap);
    else
      D_StartTitle(); // start up intro loop
  }

#ifdef GENERATE_BAKED
  int i;
  extern int numtextures;
  printf("NRT: %d\n", numtextures);
  for (i = 0; i < numtextures; i++) {
    R_GenerateComposite(i);
  }

  extern const int *texturewidthmask;
  extern const int *texturecompositesize;
  extern const short **texturecolumnlump;
  extern const unsigned short **texturecolumnofs;
  extern const byte **texturecomposite;
  extern const texture_t **textures;

  FILE *bf = fopen("support/baked_texture_data.c", "w");
  fprintf(bf, "//This file is autogenerated when compiled against "
              "GENERATE_BAKED and run.\n");
  fprintf(bf, "#include \"../r_data.h\"\n");
  fprintf(bf, "const int	numtextures = %d;\n", numtextures);

  for (i = 0; i < numtextures; i++) {
    int j;
    fprintf(bf, "static const unsigned char textureData%d[] = { ", i);
    for (j = 0; j < sizeof(texpatch_t) * textures[i]->patchcount + 14; j++) {
      fprintf(bf, "0x%02x, ", ((unsigned char *)textures[i])[j]);
    }
    fprintf(bf, "};\n");
  }

  fprintf(bf, "const texture_t*	textures[%d] = {", numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x03) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "(const texture_t*)&textureData%d, ", i);
  }
  fprintf(bf, "}; \n\n");

  fprintf(bf, "const int texturewidthmask[%d] = {", numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x1f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "%d,", texturewidthmask[i]);
  }
  fprintf(bf, "}; \n\n");

  fprintf(bf, "const fixed_t textureheight[%d] = {", numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "%d,", textureheight[i]);
  }
  fprintf(bf, "}; \n\n");

  fprintf(bf, "const int texturecompositesize[%d] = {", numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "%d,", texturecompositesize[i]);
  }
  fprintf(bf, "};\n\n");

  for (i = 0; i < numtextures; i++) {
    if (texturecolumnlump[i]) {
      int j;
      fprintf(bf, "static const short tclump_%d[] = {", i);
      for (j = 0; j < textures[i]->width; j++) {
        if ((i & 0x0f) == 0)
          fprintf(bf, "\n\t");
        fprintf(bf, "%d,", texturecolumnlump[i][j]);
      }
      fprintf(bf, "};\n");
    }
  }
  fprintf(bf, "\n");
  fprintf(bf, "const short*			texturecolumnlump[%d] = {",
          numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x07) == 0)
      fprintf(bf, "\n\t");
    if (texturecolumnlump[i])
      fprintf(bf, "tclump_%d, ", i);
    else
      fprintf(bf, "0, ");
  }
  fprintf(bf, "}; \n\n");

  for (i = 0; i < numtextures; i++) {
    if (texturecolumnofs[i]) {
      int j;
      fprintf(bf, "static const short tccolofs_%d[] = {", i);
      for (j = 0; j < textures[i]->width; j++) {
        if ((i & 0x0f) == 0)
          fprintf(bf, "\n\t");
        fprintf(bf, "%d,", texturecolumnofs[i][j]);
      }
      fprintf(bf, "};\n");
    }
  }
  fprintf(bf, "const short * const		texturecolumnofs[%d] = {",
          numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x07) == 0)
      fprintf(bf, "\n\t");
    if (texturecolumnofs[i])
      fprintf(bf, "tccolofs_%d, ", i);
    else
      fprintf(bf, "0, ");
  }
  fprintf(bf, "}; \n\n");

  for (i = 0; i < numtextures; i++) {
    if (texturecomposite[i]) {
      fprintf(bf, "static const unsigned char tcd_%d[] = {", i);
      int j;
      for (j = 0; j < texturecompositesize[i]; j++) {
        if ((j & 0x0f) == 0)
          fprintf(bf, "\n\t");
        fprintf(bf, "0x%02x, ", texturecomposite[i][j]);
      }
      fprintf(bf, "};\n");
    }
  }
  fprintf(bf, "\n");
  fprintf(bf, "const unsigned char *	texturecomposite[%d] = {", numtextures);
  for (i = 0; i < numtextures; i++) {
    if ((i & 0x07) == 0)
      fprintf(bf, "\n\t");
    if (texturecomposite[i])
      fprintf(bf, "tcd_%d, ", i);
    else
      fprintf(bf, "0, ");
  }
  fprintf(bf, "}; \n");
  int j;
  extern int numspritelumps;
  extern int firstspritelump;
  extern int lastspritelump;
  fprintf(bf, "const int numspritelumps = %d;\n", numspritelumps);
  fprintf(bf, "const int lastspritelump = %d;\n", lastspritelump);
  fprintf(bf, "const int firstspritelump = %d;\n", firstspritelump);
  fprintf(bf, "const fixed_t spritewidth[] = {");
  for (j = 0; j < numspritelumps; j++) {
    if ((j & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "0x%02x, ", spritewidth[j]);
  }
  fprintf(bf, "};\n");
  fprintf(bf, "const fixed_t spritetopoffset[] = {");
  for (j = 0; j < numspritelumps; j++) {
    if ((j & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "0x%02x, ", spritetopoffset[j]);
  }
  fprintf(bf, "};\n");
  fprintf(bf, "const fixed_t spriteoffset[] = {");
  for (j = 0; j < numspritelumps; j++) {
    if ((j & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "0x%02x, ", spriteoffset[j]);
  }
  fprintf(bf, "};\n");

  fprintf(bf, "const lighttable_t colormaps[] = {");
  for (j = 0; j < colormapsize; j++) {
    if ((j & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "0x%02x, ", colormaps[j]);
  }
  fprintf(bf, "};\n");

  fprintf(bf, "const unsigned char translationtables[] = {");
  for (j = 0; j < 256 * 3 + 255; j++) {
    if ((j & 0x0f) == 0)
      fprintf(bf, "\n\t");
    fprintf(bf, "0x%02x, ", translationtables[j]);
  }
  fprintf(bf, "};\n");

  R_ExecuteSetViewSize();

  fprintf(bf, "const int viewangletox[] = { ");
  for (i = 0; i < FINEANGLES / 2; i++)
    fprintf(bf, "%d, ", viewangletox[i]);
  fprintf(bf, " };\n");

  fprintf(bf, "const angle_t	xtoviewangle[] = { ");
  for (i = 0; i < SCREENWIDTH + 1; i++)
    fprintf(bf, "%d, ", xtoviewangle[i]);
  fprintf(bf, " };\n");

  fclose(bf);

  // Store VERTEXES, ... and hopefully some day LINEDEFS, SIDEDEFS, etc.
  //
  {
    printf("Done creating baked_texture_data.c, moving onto maps.\n");

    bf = fopen("support/baked_map_data.c", "w");
    fprintf(bf, "//This file is autogenerated when compiled against "
                "GENERATE_BAKED and run.\n");
    fprintf(bf, "#include \"../r_defs.h\"\n\n");

#ifdef GENERATE_BAKED
    extern int numvertexes;
    extern vertex_t *vertexes;
    extern int numnodes;
    extern node_t *nodes;
#endif

    int j;
#if E1M1ONLY
#define BAKE_MAPS 1
    const char *bakemaps[BAKE_MAPS + 1] = {"E1M1", 0};
#else
#define BAKE_MAPS 9
    const char *bakemaps[BAKE_MAPS + 1] = {"E1M1", "E1M2", "E1M3", "E1M4",
                                           "E1M5", "E1M6", "E1M7", "E1M8",
                                           "E1M9", 0};
#endif

    // For if we want to select a subset of maps.
    p = M_CheckParm("-strikemap");
    while (p && p < myargc - 1) {
      int ai = atoi(myargv[p + 1]);
      printf("SKIPPING MAP %d\n", ai);
      if (ai == -1)
        break;
      bakemaps[ai] = "SKIP";
      p++;
    }

    int numvertexes_baked[BAKE_MAPS];
    int numnodes_baked[BAKE_MAPS];
    for (i = 0; i < BAKE_MAPS; i++) {
      numvertexes = 0;
      numnodes = 0;
      if (strcmp(bakemaps[i], "SKIP") != 0) {
        fprintf(stderr, "$$$$ %s\n", bakemaps[i]);
        P_SetupLevel(1, (intptr_t)bakemaps[i], 0, 0);
      }

      numvertexes_baked[i] = numvertexes;
      numnodes_baked[i] = numnodes;

      fprintf(bf, "static const vertex_t __vertexes%d[%d] = { ", i,
              numvertexes * sizeof(vertex_t));
      for (j = 0; j < numvertexes * sizeof(vertex_t); j++)
        fprintf(bf, " { 0x%08x, 0x%08x }, ", vertexes[j].x, vertexes[j].y);
      fprintf(bf, "};\n");

      fprintf(bf, "static const unsigned char __nodes%d[] = { ", i);
      for (j = 0; j < numnodes * sizeof(node_t); j++)
        fprintf(bf, "0x%02x, ", ((unsigned char *)nodes)[j]);
      fprintf(bf, "};\n");
    }

    fprintf(bf, "const int numnodes_baked[] = { ");
    for (i = 0; i < BAKE_MAPS; i++)
      fprintf(bf, "%d, ", numnodes_baked[i]);
    fprintf(bf, " };\n");

    fprintf(bf, "const int numvertexes_baked[] = { ");
    for (i = 0; i < BAKE_MAPS; i++)
      fprintf(bf, "%d, ", numvertexes_baked[i]);
    fprintf(bf, " };\n");

    fprintf(bf, "const char * bakemaps[] = { ");
    for (i = 0; i < BAKE_MAPS; i++)
      fprintf(bf, "\"%s\", ", bakemaps[i]);
    fprintf(bf, "0 };\n");

    fprintf(bf, "const vertex_t* vertexes_baked[] = { ");
    for (i = 0; i < BAKE_MAPS; i++)
      fprintf(bf, "__vertexes%d, ", i);
    fprintf(bf, " };\n");

    fprintf(bf, "const node_t* nodes_baked[] = { ");
    for (i = 0; i < BAKE_MAPS; i++)
      fprintf(bf, "(const node_t*)__nodes%d, ", i);
    fprintf(bf, " };\n");

    fclose(bf);
  }

  exit(0);
#endif

  D_DoomLoop(); // never returns
}
