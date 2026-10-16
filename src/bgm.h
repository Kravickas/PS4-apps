#pragma once
/* Looping background music (bgm.c). */

typedef void* (*bgm_alloc_fn)(unsigned long size, unsigned long align);

/* Loads a 16-bit stereo 48 kHz PCM WAV (tools/make_bgm.py) with alloc and
   starts a thread that loops it on the main audio port. 0 on success; a
   negative bgm.c code or a libSceAudioOut error otherwise. */
int bgm_start(const char* path, bgm_alloc_fn alloc);
