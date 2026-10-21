#include <stdlib.h>

#include "errors.h"
#include "ring-buffer.h"



int ring_buffer_create(struct ring_buffer **ring_buffer_ref, size_t size, struct context *context, void (*on_cycle)(fvec_t *samples, struct context *context)) {
    if (ring_buffer_ref == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer ref is NULL");
        return ERR_CODE_RUNTIME;
    }
    if (*ring_buffer_ref != NULL) {
        PRINT_ERR_RUNTIME("Ring buffer ref does not point to NULL");
        return ERR_CODE_RUNTIME;
    }
    if (size == 0u) {
        PRINT_ERR_RUNTIME("Ring buffer size is zero");
        return ERR_CODE_RUNTIME;
    }
    if (context == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer context is NULL");
        return ERR_CODE_RUNTIME;
    }

    struct ring_buffer *ring_buffer = malloc(sizeof(struct ring_buffer));
    if (ring_buffer == NULL) {
        PRINT_ERR_CREATION_FAILED("Ring buffer");
        return ERR_CODE_CREATION_FAILED;
    }

    ring_buffer->size = size;
    ring_buffer->position = 0u;
    ring_buffer->samples = new_fvec(size);
    ring_buffer->context = context;
    ring_buffer->on_cycle = on_cycle;
    if (ring_buffer->samples == NULL) {
        PRINT_ERR_CREATION_FAILED("Ring buffer samples buffer");
        return ERR_CODE_CREATION_FAILED;
    }

    fvec_zeros(ring_buffer->samples);

    *ring_buffer_ref = ring_buffer;

    return 0;
}

int ring_buffer_write(struct ring_buffer **ring_buffer_ref, smpl_t *write_source, size_t write_size) {
    if (ring_buffer_ref == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer ref is NULL");
        return ERR_CODE_RUNTIME;
    }

    struct ring_buffer *ring_buffer = *ring_buffer_ref;

    if (ring_buffer == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer is NULL");
        return ERR_CODE_RUNTIME;
    }
    if (ring_buffer->samples == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer samples buffer is NULL");
        return ERR_CODE_RUNTIME;
    }
    if (write_source == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer write source is NULL");
        return ERR_CODE_RUNTIME;
    }
    // if (write_size > ring_buffer->size) {
    //     PRINT_ERR_RUNTIME("Ring buffer write size is greater than ring buffer size");
    //     printf("Write size = %ld, Buffer size = %ld\n", write_size, ring_buffer->size);
    //     return ERR_CODE_RUNTIME;
    // }

    for (size_t i = 0u; i < write_size; i++) {
        fvec_set_sample(ring_buffer->samples, write_source[i], ring_buffer->position++);

        ring_buffer->position %= ring_buffer->size;

        if (ring_buffer->on_cycle != NULL && ring_buffer->position == 0u) {
            ring_buffer->on_cycle(ring_buffer->samples, ring_buffer->context);
        }
    }

    return 0;
}

int ring_buffer_destroy(struct ring_buffer **ring_buffer_ref) {
    if (ring_buffer_ref == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer ref is NULL");
        return ERR_CODE_RUNTIME;
    }

    struct ring_buffer *ring_buffer = *ring_buffer_ref;

    if (ring_buffer == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer is NULL");
        return ERR_CODE_RUNTIME;
    }
    if (ring_buffer->samples == NULL) {
        PRINT_ERR_RUNTIME("Ring buffer samples buffer is NULL");
        return ERR_CODE_RUNTIME;
    }

    del_fvec(ring_buffer->samples);
    free(ring_buffer);

    *ring_buffer_ref = NULL;

    return 0;
}
