/**
 * CD-ROM support has been removed from SDL 2.
 *
 * This file provides stubs just to maintain compatibility.
 *
 * Original by Mark Baker <homer1@together.net>
 */

#include <SDL.h>

#include "quakedef.h"

static qboolean cdValid = false;
static qboolean initialized = false;
static qboolean enabled = false;
static qboolean playLooping = false;
static float cdvolume = 1.0;
static int current_track = 0;

static void CD_f();

static void CDAudio_Eject()
{
    if (!enabled) {
        return;
    }

    // CD-ROM support removed in SDL 2
    Con_DPrintf("CD-ROM support is not available in SDL 2.\n");
}

void CDAudio_Play(byte track, qboolean looping)
{
    if (!enabled) {
        return;
    }

    // CD-ROM support removed in SDL 2
    // Store track info for compatibility
    if ((track < 1) || (track > 99)) {
        Con_DPrintf("CDAudio: Bad track number: %d\n", track);

        return;
    }

    current_track = track;
    playLooping = looping;

    Con_DPrintf("CD-ROM playback is not available in SDL 2.\n");
}

void CDAudio_Stop()
{
    if (!enabled) {
        return;
    }

    current_track = 0;
    playLooping = false;
}

void CDAudio_Pause()
{
    if (!enabled) {
        return;
    }

    // Stub implementation
}

void CDAudio_Resume()
{
    if (!enabled) {
        return;
    }

    // Stub implementation
}

void CDAudio_Update()
{
    if (!enabled) {
        return;
    }

    if (bgmvolume.value != cdvolume) {
        if (cdvolume) {
            Cvar_SetValue("bgmvolume", 0.0);
            CDAudio_Pause();
        } else {
            Cvar_SetValue("bgmvolume", 1.0);
            CDAudio_Resume();
        }

        cdvolume = bgmvolume.value;

        return;
    }
}

int CDAudio_Init()
{
    if ((cls.state == ca_dedicated) || COM_CheckParm("-nocdaudio")) {
        return -1;
    }

    // SDL 2 removed CD-ROM support
    // Initialize as disabled but successful
    initialized = true;
    enabled = false;
    cdValid = false;

    Cmd_AddCommand("cd", CD_f);
    Con_Printf("CD Audio Initialized (SDL 2 - no CD-ROM support).\n");

    return 0;
}

void CDAudio_Shutdown()
{
    CDAudio_Stop();
    initialized = false;
}

static void CD_f()
{
    char* command;

    if (Cmd_Argc() < 2) {
        return;
    }

    command = Cmd_Argv(1);

    if (!Q_strcasecmp(command, "on")) {
        enabled = true;
        Con_Printf("CD Audio enabled (SDL 2 - no actual CD-ROM support).\n");

        return;
    }

    if (!Q_strcasecmp(command, "off")) {
        CDAudio_Stop();
        enabled = false;

        return;
    }

    if (!Q_strcasecmp(command, "play")) {
        CDAudio_Play(Q_atoi(Cmd_Argv(2)), false);

        return;
    }

    if (!Q_strcasecmp(command, "loop")) {
        CDAudio_Play(Q_atoi(Cmd_Argv(2)), true);

        return;
    }

    if (!Q_strcasecmp(command, "stop")) {
        CDAudio_Stop();

        return;
    }

    if (!Q_strcasecmp(command, "pause")) {
        CDAudio_Pause();

        return;
    }

    if (!Q_strcasecmp(command, "resume")) {
        CDAudio_Resume();

        return;
    }

    if (!Q_strcasecmp(command, "eject")) {
        CDAudio_Eject();

        return;
    }

    if (!Q_strcasecmp(command, "info")) {
        Con_Printf("CD-ROM support not available in SDL 2\n");
        if (current_track > 0) {
            Con_Printf("Would be %s track %d\n",
                playLooping ? "looping" : "playing", current_track);
        }

        return;
    }
}
