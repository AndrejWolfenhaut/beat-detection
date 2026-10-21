#include "definitions.h"
#include "errors.h"
#include "context.h"
#include "ring-buffer.h"



static void on_pw_main_loop_sigint_or_sigterm(void *data, int) {
    struct context *context = (struct context *)data;

    // TODO: Unsure what the return value indicates...
    pw_main_loop_quit(context->pw_main_loop);
}

static void on_pw_stream_param_changed(void *data, uint32_t id, const struct spa_pod *param) {
    struct context *context = (struct context *)data;

    if (param == NULL || id != SPA_PARAM_Format) {
        return;
    }

    if (spa_format_parse(param, &context->spa_audio_info.media_type, &context->spa_audio_info.media_subtype) < 0) {
        return;
    }
    if (
        context->spa_audio_info.media_type != SPA_MEDIA_TYPE_audio ||
        context->spa_audio_info.media_subtype != SPA_MEDIA_SUBTYPE_raw
    ) {
        return;
    }

    spa_format_audio_raw_parse(param, &context->spa_audio_info.info.raw);
}

static void on_pw_stream_process(void *data) {
    struct context *context = (struct context *)data;

    struct pw_buffer *pw_buffer = pw_stream_dequeue_buffer(context->pw_stream);
    if (pw_buffer == NULL) {
        pw_log_warn("Processing error: Out of buffers!");
        return;
    }

    struct spa_buffer *spa_buffer = pw_buffer->buffer;
    smpl_t *samples = (smpl_t *)spa_buffer->datas[0].data;
    if (samples == NULL) {
        pw_log_warn("Processing error: Could not access samples!");
        return;
    }

    uint32_t num_channels = context->spa_audio_info.info.raw.channels;
    uint32_t num_samples = spa_buffer->datas[0].chunk->size / sizeof(smpl_t);
    if (num_channels != AUDIO_NUM_CHANNELS) {
        pw_log_warn("Processing error: Invalid number of channels!");
        return;
    }

    ring_buffer_write(&context->ring_buffer, samples, num_samples);

    pw_stream_queue_buffer(context->pw_stream, pw_buffer);
}

static void on_ring_buffer_cycle(fvec_t *samples, struct context *context) {
    aubio_tempo_do(context->aubio_tempo, samples, context->aubio_output);

    if (context->aubio_output->data[0] != 0) {
        fprintf(
            stdout,
            "{ \"miliseconds\": %.3f, \"seconds\": %.3f, \"frame\": %d, \"bpm\": %.2f, \"confidence\": %.2f }\n",
            aubio_tempo_get_last_ms(context->aubio_tempo),
            aubio_tempo_get_last_s(context->aubio_tempo),
            aubio_tempo_get_last(context->aubio_tempo),
            aubio_tempo_get_bpm(context->aubio_tempo),
            aubio_tempo_get_confidence(context->aubio_tempo)
        );
        fflush(stdout);
    }
}



int main(int argc, char **argv) {
    int error_code = 0;

    struct ring_buffer *ring_buffer = NULL;
    struct context context = { 0 };

    struct pw_main_loop *pw_main_loop = NULL;
    struct pw_loop *pw_loop = NULL;
    struct pw_properties *pw_properties = NULL;
    struct pw_stream *pw_stream = NULL;
    const struct pw_stream_events pw_stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = on_pw_stream_param_changed,
        .process = on_pw_stream_process
    };

    uint8_t spa_pod_buffer[1024];
    const struct spa_pod *pw_stream_params[1];
    struct spa_pod_builder spa_pod_builder = SPA_POD_BUILDER_INIT(spa_pod_buffer, sizeof(spa_pod_buffer));

    aubio_tempo_t *aubio_tempo = NULL;
    fvec_t *aubio_output = NULL;

    //
    // Initialize PipeWire
    //

    pw_init(&argc, &argv);

    //
    // Create PipeWire main loop
    //

    PRINT_MSG("Creating PipeWire main loop...\n");
    pw_main_loop = pw_main_loop_new(NULL);
    if (pw_main_loop == NULL) {
        error_code = ERR_CODE_CREATION_FAILED;
        PRINT_ERR_CREATION_FAILED("PipeWire main loop");
        goto cleanup_pw_main_loop;
    }
    PRINT_MSG("Successful.\n");

    //
    // Get PipeWire loop
    //

    PRINT_MSG("Getting PipeWire loop...\n");
    pw_loop = pw_main_loop_get_loop(pw_main_loop);
    if (pw_loop == NULL) {
        error_code = ERR_CODE_ACCESS_FAILED;
        PRINT_ERR_ACCESS_FAILED("PipeWire loop");
        goto cleanup_pw_main_loop;
    }
    PRINT_MSG("Successful.\n");

    //
    // Register PipeWire signal handlers
    //

    PRINT_MSG("Registering PipeWire signal handlers...\n");
    pw_loop_add_signal(pw_loop, SIGINT, on_pw_main_loop_sigint_or_sigterm, &context);
    pw_loop_add_signal(pw_loop, SIGTERM, on_pw_main_loop_sigint_or_sigterm, &context);
    PRINT_MSG("Successful.\n");

    //
    // Create PipeWire stream properties
    //

    PRINT_MSG("Creating PipeWire stream properties...\n");
    pw_properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE,          "Audio",
        PW_KEY_MEDIA_CATEGORY,      "Capture",
        PW_KEY_MEDIA_ROLE,          "Music",
        PW_KEY_STREAM_CAPTURE_SINK, "true",
        PW_KEY_NODE_PASSIVE,        "true",
        PW_KEY_NODE_VIRTUAL,        "true",
        PW_KEY_STREAM_DONT_REMIX,   "false",
        "channelmix.upmix",         "true",
        NULL
    );
    if (pw_properties == NULL) {
        error_code = ERR_CODE_CREATION_FAILED;
        PRINT_ERR_CREATION_FAILED("PipeWire stream properties");
        goto cleanup_pw_main_loop;
    }
    PRINT_MSG("Successful.\n");

    //
    // Create PipeWire stream
    //

    PRINT_MSG("Creating PipeWire stream...\n");
    pw_stream = pw_stream_new_simple(pw_loop, "beat-detection", pw_properties, &pw_stream_events, &context);
    if (pw_stream == NULL) {
        error_code = ERR_CODE_CREATION_FAILED;
        PRINT_ERR_CREATION_FAILED("PipeWire stream");
        goto cleanup_pw_stream;
    }
    PRINT_MSG("Successful.\n");

    //
    // Create PipeWire stream parameters
    //

    PRINT_MSG("Creating PipeWire stream parameters...\n");
    pw_stream_params[0] = spa_format_audio_raw_build(
        &spa_pod_builder,
        SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = AUDIO_FORMAT,
            .rate = AUDIO_SAMPLE_RATE,
            .channels = AUDIO_NUM_CHANNELS
        )
    );
    if (pw_stream_params[0] == NULL) {
        error_code = ERR_CODE_CREATION_FAILED;
        PRINT_ERR_CREATION_FAILED("PipeWire stream parameters");
        goto cleanup_pw_stream;
    }
    PRINT_MSG("Successful.\n");

    //
    // Connect to PipeWire stream
    //

    PRINT_MSG("Connecting to PipeWire stream...\n");
    int pw_stream_connect_error_code = pw_stream_connect(
        pw_stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
        pw_stream_params,
        sizeof(pw_stream_params) / sizeof(pw_stream_params[0])
    );
    if (pw_stream_connect_error_code < 0) {
        error_code = ERR_CODE_CONNECTION_FAILED;
        PRINT_ERR_CONNECTION_FAILED("PipeWire stream");
        goto cleanup_pw_stream;
    }
    PRINT_MSG("Successful.\n");

    //
    // Creat ring buffer
    //

    PRINT_MSG("Creating ring buffer...\n");
    int ring_buffer_create_error_code = ring_buffer_create(&ring_buffer, AUDIO_HOP_SIZE, &context, on_ring_buffer_cycle);
    if (ring_buffer_create_error_code < 0) {
        error_code = ring_buffer_create_error_code;
        goto cleanup_ring_buffer;
    }
    PRINT_MSG("Successful.\n");

    //
    // Create Aubio tempo
    //

    PRINT_MSG("Creating Aubio tempo...\n");
    aubio_tempo = new_aubio_tempo("default", AUDIO_WINDOW_SIZE, AUDIO_HOP_SIZE, AUDIO_SAMPLE_RATE);
    if (aubio_tempo == NULL) {
        error_code = ERR_CODE_CREATION_FAILED;
        PRINT_ERR_CREATION_FAILED("Aubio tempo");
        goto cleanup_aubio_tempo;
    }
    PRINT_MSG("Successful.\n");

    //
    // Create Aubio output
    //

    PRINT_MSG("Creating Aubio output...\n");
    aubio_output = new_fvec(1);
    if (aubio_output == NULL) {
        error_code = ERR_CODE_CREATION_FAILED;
        PRINT_ERR_CREATION_FAILED("Aubio output");
        goto cleanup_aubio_output;
    }
    PRINT_MSG("Successful.\n");

    //
    // Set up context
    //

    context.pw_main_loop = pw_main_loop;
    context.pw_stream = pw_stream;
    context.ring_buffer = ring_buffer;
    context.aubio_tempo = aubio_tempo;
    context.aubio_output = aubio_output;

    //
    // Start PipeWire main loop
    //

    PRINT_MSG("Starting PipeWire main loop...\n");
    int pw_main_loop_run_error_code = pw_main_loop_run(pw_main_loop);
    if (pw_main_loop_run_error_code < 0) {
        error_code = ERR_CODE_RUNTIME;
        PRINT_ERR_RUNTIME("PipeWire main loop failed");
        goto cleanup_aubio_output;
    }
    PRINT_MSG("Successful.\n");

cleanup_aubio_output:
    del_fvec(aubio_output);

cleanup_aubio_tempo:
    del_aubio_tempo(aubio_tempo);
    
cleanup_ring_buffer:
    ring_buffer_destroy(&ring_buffer);

cleanup_pw_stream:
    pw_stream_destroy(pw_stream);

cleanup_pw_main_loop:
    pw_main_loop_destroy(context.pw_main_loop);

    aubio_cleanup();
    pw_deinit();

    return error_code;
}
