#include <stdio.h>
#include <math.h>

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>



#define PRINT_MSG(...) fprintf(stdout, __VA_ARGS__)
#define PRINT_ERR(...) fprintf(stderr, __VA_ARGS__)

#define PRINT_ERR_CREATION_FAILED_SIMPLE(target_description) PRINT_ERR("Error: Creation of %s failed!\n", (target_description))
#define PRINT_ERR_CONNECTION_FAILED_SIMPLE(target_description) PRINT_ERR("Error: Connection to %s failed!\n", (target_description))
#define PRINT_ERR_ACCESS_FAILED_SIMPLE(target_description) PRINT_ERR("Error: Access of %s failed!\n", (target_description))
#define PRINT_ERR_RUNTIME_SIMPLE(error_description) PRINT_ERR("Error: %s!\n", (error_description))

#define PRINT_ERR_CREATION_FAILED(target_description, internal_error_code) PRINT_ERR("Error: Creation of %s failed! (Internal error code: %#010x)\n", (target_description), (internal_error_code))
#define PRINT_ERR_CONNECTION_FAILED(target_description, internal_error_code) PRINT_ERR("Error: Connection to %s failed! (Internal error code: %#010x)\n", (target_description), (internal_error_code))
#define PRINT_ERR_ACCESS_FAILED(target_description, internal_error_code) PRINT_ERR("Error: Access of %s failed! (Internal error code: %#010x)\n", (target_description), (internal_error_code))
#define PRINT_ERR_RUNTIME(error_description, internal_error_code) PRINT_ERR("Error: %s at runtime! (Internal error code: %#010x)\n", (error_description), (internal_error_code))

#define ERR_CREATION_FAILED_SIMPLE(target_description, error_label) { PRINT_ERR_CREATION_FAILED_SIMPLE(target_description); error_code = 1; goto error_label; }
#define ERR_CONNECTION_FAILED_SIMPLE(target_description, error_label) { PRINT_ERR_CONNECTION_FAILED_SIMPLE(target_description); error_code = 2; goto error_label; }
#define ERR_ACCESS_FAILED_SIMPLE(target_description, error_label) { PRINT_ERR_ACCESS_FAILED_SIMPLE(target_description); error_code = 3; goto error_label; }
#define ERR_RUNTIME_SIMPLE(error_description, error_label) { PRINT_ERR_RUNTIME_SIMPLE(error_description); error_code = 4; goto error_label; }

#define ERR_CREATION_FAILED(target_description, internal_error_code, error_label) { PRINT_ERR_CREATION_FAILED(target_description, internal_error_code); error_code = 1; goto error_label; }
#define ERR_CONNECTION_FAILED(target_description, internal_error_code, error_label) { PRINT_ERR_CONNECTION_FAILED(target_description, internal_error_code); error_code = 2; goto error_label; }
#define ERR_ACCESS_FAILED(target_description, internal_error_code, error_label) { PRINT_ERR_ACCESS_FAILED(target_description, internal_error_code); error_code = 3; goto error_label; }
#define ERR_RUNTIME(error_description, internal_error_code, error_label) { PRINT_ERR_RUNTIME(error_description, internal_error_code); error_code = 4; goto error_label; }



struct user_data {
  struct pw_main_loop *pw_main_loop;
  struct pw_stream *pw_stream;

  struct spa_audio_info spa_audio_info;

  uint32_t move;
};



static void on_sigint_or_sigterm(void *data, int signal_number) {
  struct user_data *user_data = (struct user_data *)data;

  // TODO: Unsure what the return value indicates...
  pw_main_loop_quit(user_data->pw_main_loop);
}

static void on_pw_stream_param_changed(void *data, uint32_t id, const struct spa_pod *param) {
  PRINT_MSG("Here 1\n");

  struct user_data *user_data = (struct user_data *)data;

  if (param == NULL || id != SPA_PARAM_Format) {
    PRINT_MSG("Here 2\n");
    return;
  }

  if (spa_format_parse(param, &user_data->spa_audio_info.media_type, &user_data->spa_audio_info.media_subtype) < 0) {
    PRINT_MSG("Here 3\n");
    return;
  }
  if (
    user_data->spa_audio_info.media_type != SPA_MEDIA_TYPE_audio ||
    user_data->spa_audio_info.media_subtype != SPA_MEDIA_SUBTYPE_raw
  ) {
    PRINT_MSG("Here 4\n");
    return;
  }

  PRINT_MSG("Here\n");
  spa_format_audio_raw_parse(param, &user_data->spa_audio_info.info.raw);
}

static void on_pw_stream_process(void *data) {
  struct user_data *user_data = (struct user_data *)data;

  struct pw_buffer *pw_buffer = NULL;
  struct spa_buffer *spa_buffer = NULL;
  float
    *samples = NULL,
    max = 0.0f;
  uint32_t
    num_channels = 0u,
    channel = 0u,
    num_samples = 0u,
    sample = 0u,
    peak = 0u;

  pw_buffer = pw_stream_dequeue_buffer(user_data->pw_stream);
  if (pw_buffer == NULL) {
    pw_log_warn("Processing error: Out of buffers! (Internal error message: %m)");
    return;
  }

  spa_buffer = pw_buffer->buffer;
  samples = (float *)spa_buffer->datas[0].data;
  if (samples == NULL) {
    pw_log_warn("Processing error: Could not access samples!");
    return;
  }

  num_channels = user_data->spa_audio_info.info.raw.channels;
  num_samples = spa_buffer->datas[0].chunk->size / sizeof(float);

  if (user_data->move == 0u) {
    PRINT_MSG("Processing: %c[%dA", 0x1b, num_channels + 1);
  }

  PRINT_MSG("Processing: Captured %d samples\n", num_samples / num_channels);

  for (channel = 0u; channel < num_channels; channel++) {
    max = 0.0f;
    
    for (sample = channel; sample < num_samples; sample += num_channels) {
      max = fmaxf(max, fabsf(samples[sample]));
    }

    peak = (uint32_t)SPA_CLAMPF(max * 30.0f, 0.0f, 39.0f);

    PRINT_MSG("Processing: Channel %d: |%*s%*s| peak:%f\n", channel, peak + 1u, "*", 40u - peak, "", max);
  }

  user_data->move = 1u;

  fflush(stdout);

  pw_stream_queue_buffer(user_data->pw_stream, pw_buffer);
}



static const struct pw_stream_events pw_stream_events = {
  PW_VERSION_STREAM_EVENTS,
  .param_changed = on_pw_stream_param_changed,
  .process = on_pw_stream_process
};



int main(int argc, char **argv) {
  int error_code = 0;

  struct user_data user_data = { 0 };

  struct pw_main_loop *pw_main_loop = NULL;
  struct pw_loop *pw_loop = NULL;
  struct pw_properties *pw_properties = NULL;
  struct pw_stream *pw_stream = NULL;

  uint8_t spa_pod_buffer[1024];
  const struct spa_pod *pw_stream_params[1];
  struct spa_pod_builder spa_pod_builder = SPA_POD_BUILDER_INIT(spa_pod_buffer, sizeof(spa_pod_buffer));

  pw_init(&argc, &argv);

  pw_main_loop = pw_main_loop_new(NULL);
  if (pw_main_loop == NULL) {
    ERR_CREATION_FAILED_SIMPLE("PipeWire main loop", cleanup_pw);
  }

  pw_loop = pw_main_loop_get_loop(pw_main_loop);
  if (pw_loop == NULL) {
    ERR_ACCESS_FAILED_SIMPLE("PipeWire loop", cleanup_pw_main_loop);
  }

  pw_loop_add_signal(pw_loop, SIGINT, on_sigint_or_sigterm, &user_data);
  pw_loop_add_signal(pw_loop, SIGTERM, on_sigint_or_sigterm, &user_data);

  pw_properties = pw_properties_new(
    PW_KEY_MEDIA_TYPE, "Audio",
    PW_KEY_MEDIA_CATEGORY, "Capture",
    PW_KEY_MEDIA_ROLE, "Music",
    PW_KEY_STREAM_CAPTURE_SINK, "true",
    NULL
  );
  if (pw_properties == NULL) {
    ERR_CREATION_FAILED_SIMPLE("PipeWire stream properties", cleanup_pw_main_loop);
  }

  pw_stream = pw_stream_new_simple(pw_loop, "beat-detection", pw_properties, &pw_stream_events, &user_data);
  if (pw_stream == NULL) {
    ERR_CREATION_FAILED_SIMPLE("PipeWire stream", cleanup_pw_main_loop);
  }

  int pw_stream_connect_error_code = pw_stream_connect(
    pw_stream,
    PW_DIRECTION_INPUT,
    PW_ID_ANY,
    PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
    pw_stream_params,
    sizeof(pw_stream_params) / sizeof(pw_stream_params[0])
  );
  if (pw_stream_connect_error_code < 0) {
    ERR_CONNECTION_FAILED("PipeWire stream", pw_stream_connect_error_code, cleanup_pw_stream);
  }

  user_data.pw_main_loop = pw_main_loop;
  user_data.pw_stream = pw_stream;
  user_data.move = 1u;

  int pw_main_loop_run_error_code = pw_main_loop_run(pw_main_loop);
  if (pw_main_loop_run_error_code < 0) {
    ERR_RUNTIME("PipeWire main loop failed", pw_main_loop_run_error_code, cleanup_pw_stream);
  }

cleanup_pw_stream:
  pw_stream_destroy(pw_stream);

cleanup_pw_main_loop:
  pw_main_loop_destroy(user_data.pw_main_loop);

cleanup_pw:
  pw_deinit();

  return error_code;
}
