#ifndef RING_BUFFER_H
#define RING_BUFFER_H



#include <aubio/aubio.h>

#include "context.h"



struct ring_buffer {
    size_t size, position;
    fvec_t *samples;

    struct context *context;

    void (*on_cycle)(fvec_t *samples, struct context *context);
};



int ring_buffer_create(struct ring_buffer **ring_buffer_ref, size_t size, struct context *context, void (*on_cycle)(fvec_t *samples, struct context *context));
int ring_buffer_write(struct ring_buffer **ring_buffer_ref, smpl_t *write_source, size_t write_size);
int ring_buffer_destroy(struct ring_buffer **ring_buffer_ref);



#endif