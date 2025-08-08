#include <libdragon.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------
   [0] Funkcja pomocnicza: zamiana compression_level na nazwę
   --------------------------------------------------------- */
const char* get_compression_name(int level) {
    switch (level) {
        case 0: return "PCM (brak kompresji)";
        case 1: return "VADPCM (domyslny)";
        case 3: return "Opus";
        default: return "Nieznany / nieobslugiwany";
    }
}

int main(void) {
    /* [1] Inicjalizacja wyświetlania */
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);

    /* [2] Inicjalizacja systemu plików */
    if (dfs_init(DFS_DEFAULT_LOCATION) != DFS_ESUCCESS) {
        surface_t *disp = display_get();
        graphics_fill_screen(disp, graphics_make_color(0, 0, 0, 255));
        graphics_set_color(graphics_make_color(255, 0, 0, 255), 0);
        graphics_draw_text(disp, 10, 10, "Blad: nie mozna zainicjowac DFS");
        display_show(disp);
        return 1;
    }

    /* [3] Audio i mikser */
    audio_init(48000, 4);
    mixer_init(32);

    /* [4] Timer */
    timer_init();

    /* [5] Kompresja Opus */
    int compression_level = 3;
    wav64_init_compression(compression_level);

    /* [6] Wczytanie pliku */
    const char *filename = "rom:/sound.wav64";
    wav64_t sound;
    wav64_open(&sound, filename);
    wav64_set_loop(&sound, true);
    wav64_play(&sound, 0);
    mixer_ch_set_vol(0, 1.0f, 1.0f);

    /* [7] Parametry audio */
    float wave_frequency = sound.wave.frequency;
    uint32_t sample_rate = (uint32_t)(wave_frequency + 0.5f);
    uint8_t channels = sound.wave.channels;
    int total_samples = sound.wave.len;
    uint8_t bits_per_sample = sound.wave.bits;

    /* [8] Czas trwania */
    uint32_t total_seconds = 0;
    if (sample_rate > 0 && total_samples > 0) {
        total_seconds = total_samples / sample_rate;
    }
    uint32_t total_minutes = total_seconds / 60;
    uint32_t total_secs_rem = total_seconds % 60;

    /* [9] Bitrate */
    int bitrate_bps = wav64_get_bitrate(&sound);
    uint32_t bitrate_kbps = (bitrate_bps > 0)
        ? (uint32_t)(bitrate_bps / 1000)
        : (sample_rate * channels * bits_per_sample) / 1000;

    /* [10] Start pętli */
    uint32_t start_ticks = timer_ticks();

    while (1) {
        /* [11] Buforowanie audio + analiza amplitudy */
        int max_amp = 0;
        if (audio_can_write()) {
            short *outbuf = audio_write_begin();
            mixer_poll(outbuf, audio_get_buffer_length());
            int samples = audio_get_buffer_length();
            for (int i = 0; i < samples; i++) {
                int amp = abs(outbuf[i]);
                if (amp > max_amp) max_amp = amp;
            }
            audio_write_end();
        }

        /* [12] Czas odtwarzania */
        uint32_t elapsed_ticks = timer_ticks() - start_ticks;
        uint32_t elapsed_ms = (uint32_t)TICKS_TO_MS(elapsed_ticks);
        uint32_t elapsed_sec = elapsed_ms / 1000;
        uint32_t play_min = elapsed_sec / 60;
        uint32_t play_sec = elapsed_sec % 60;

        /* [13] Przygotowanie ekranu */
        surface_t *disp = display_get();
        graphics_fill_screen(disp, graphics_make_color(0, 0, 64, 255));
        graphics_set_color(graphics_make_color(255, 255, 255, 255), 0);

        char linebuf[80];

        /* [14] Tytuł aplikacji */
        const char *title = "mca64Player";
        int title_x = (320 - strlen(title) * 8) / 2;
        graphics_draw_text(disp, title_x, 4, title);

        /* [15] Informacje audio */
        snprintf(linebuf, sizeof(linebuf), "Plik: %s", filename);
        graphics_draw_text(disp, 10, 28, linebuf);

        snprintf(linebuf, sizeof(linebuf), "Czas: %02lu:%02lu / %02lu:%02lu",
                 (unsigned long)play_min, (unsigned long)play_sec,
                 (unsigned long)total_minutes, (unsigned long)total_secs_rem);
        graphics_draw_text(disp, 10, 46, linebuf);

        /* [16] Pasek postępu */
        int bar_x = 10;
        int bar_y = 58;
        int bar_width = 300;
        int bar_height = 8;

        graphics_draw_box(disp, bar_x, bar_y, bar_width, bar_height,
                          graphics_make_color(32, 32, 32, 255));

        if (total_seconds > 0) {
            int progress_width = (int)((float)elapsed_sec / total_seconds * bar_width);
            graphics_draw_box(disp, bar_x, bar_y, progress_width, bar_height,
                              graphics_make_color(0, 255, 0, 255));
        }

        /* [17] Pozostałe informacje */
        snprintf(linebuf, sizeof(linebuf), "Probkowanie: %lu Hz", (unsigned long)sample_rate);
        graphics_draw_text(disp, 10, 76, linebuf);

        snprintf(linebuf, sizeof(linebuf), "Bitrate: %lu kbps", (unsigned long)bitrate_kbps);
        graphics_draw_text(disp, 10, 94, linebuf);

        snprintf(linebuf, sizeof(linebuf), "Kanaly: %s (%u)", (channels == 1 ? "Mono" : "Stereo"), (unsigned)channels);
        graphics_draw_text(disp, 10, 112, linebuf);

        snprintf(linebuf, sizeof(linebuf), "Bity: %u bit", (unsigned)bits_per_sample);
        graphics_draw_text(disp, 10, 130, linebuf);

        snprintf(linebuf, sizeof(linebuf), "Kompresja: %s", get_compression_name(compression_level));
        graphics_draw_text(disp, 10, 148, linebuf);

        snprintf(linebuf, sizeof(linebuf), "Probek: %d", total_samples);
        graphics_draw_text(disp, 10, 166, linebuf);

		snprintf(linebuf, sizeof(linebuf), "Debug: Freq = %.2f Hz", sound.wave.frequency);
		graphics_draw_text(disp, 10, 184, linebuf);
		
        /* [18] Pasek natężenia dźwięku (VU meter) */
        int vu_x = 320 - 20;
        int vu_y = 180;
        int vu_width = 8;
        int vu_max_height = 40;

        int vu_height = (max_amp * vu_max_height) / 32767;
        int vu_top = vu_y + (vu_max_height - vu_height);

        graphics_draw_box(disp, vu_x, vu_top, vu_width, vu_height,
                          graphics_make_color(255, 255, 0, 255));  // żółty pasek

        /* [19] Wyświetlenie ramki */
        display_show(disp);

        /* [20] Odświeżanie */
        wait_ms(100);
    }

    /* [21] Czyszczenie zasobów */
    wav64_close(&sound);
    audio_close();
    return 0;
}