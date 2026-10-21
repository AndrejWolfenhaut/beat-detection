#ifndef CONTEXT_H
#define CONTEXT_H



#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <aubio/aubio.h>



struct context {
  struct pw_main_loop *pw_main_loop;
  struct pw_stream *pw_stream;

  struct spa_audio_info spa_audio_info;

  struct ring_buffer *ring_buffer;

  aubio_tempo_t *aubio_tempo;
  fvec_t *aubio_output;
};



#endif