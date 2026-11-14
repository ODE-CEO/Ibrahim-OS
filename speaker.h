#ifndef SPEAKER_H
#define SPEAKER_H

#include "os.h"

#define NOTE_C4  261
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  493
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  987
#define NOTE_C6  1046

extern int sound_enabled;

void sleep_ms(uint32_t ms);
void speaker_play_freq(uint32_t freq);
void speaker_stop(void);
void speaker_beep(uint32_t freq, uint32_t duration_ms);
void play_startup_melody(void);

void sound_click(void);
void sound_open(void);
void sound_close(void);
void sound_error(void);

#endif
