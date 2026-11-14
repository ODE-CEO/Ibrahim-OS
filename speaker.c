#include "speaker.h"

int sound_enabled = 1;

void sleep_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile int j = 0; j < 1000; j++) {
            inb(0x80);
        }
    }
}

void speaker_play_freq(uint32_t freq) {
    if (!sound_enabled || freq == 0) return;

    uint32_t div = 1193180 / freq;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, tmp | 3);
    }
}

void speaker_stop(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void speaker_beep(uint32_t freq, uint32_t duration_ms) {
    if (!sound_enabled) {
        sleep_ms(duration_ms + 15);
        return;
    }
    if (freq > 0) speaker_play_freq(freq);
    sleep_ms(duration_ms);
    speaker_stop();
    sleep_ms(15);
}

void play_startup_melody(void) {
    if (!sound_enabled) return;
    speaker_beep(NOTE_C5, 80);
    speaker_beep(NOTE_E5, 80);
    speaker_beep(NOTE_G5, 100);
    speaker_beep(NOTE_C6, 250);
}

void sound_click(void) {
    if (!sound_enabled) return;
    speaker_play_freq(1400);
    sleep_ms(8);
    speaker_stop();
}

void sound_open(void) {
    if (!sound_enabled) return;
    speaker_play_freq(520);
    sleep_ms(15);
    speaker_play_freq(780);
    sleep_ms(20);
    speaker_stop();
}

void sound_close(void) {
    if (!sound_enabled) return;
    speaker_play_freq(680);
    sleep_ms(15);
    speaker_play_freq(340);
    sleep_ms(20);
    speaker_stop();
}

void sound_error(void) {
    if (!sound_enabled) return;
    speaker_play_freq(220);
    sleep_ms(40);
    speaker_stop();
}
