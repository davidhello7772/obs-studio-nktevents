/*
 * NKT Custom RNNoise Filter Plugin for OBS Studio
 *
 * Provides noise filtering with custom RNNoise model support
 * and adjustable filter strength.
 *
 * Copyright (C) 2024 NKT Events
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include <obs-module.h>
#include <util/deque.h>
#include <util/darray.h>
#include <util/threading.h>
#include <util/platform.h>
#include <media-io/audio-resampler.h>

#ifdef _MSC_VER
#define ssize_t intptr_t
#endif

#include <rnnoise.h>

#include "nkt-rnnoise-filter.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("nkt-rnnoise-filter", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "NKT Custom RNNoise Filter with model upload and strength control";
}

/* ========================================================================= */
/* Logging Macros                                                            */
/* ========================================================================= */

#define do_log(level, format, ...) \
	blog(level, "[NKT RNNoise: '%s'] " format, \
	     obs_source_get_name(filter->context), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

#ifdef _DEBUG
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* ========================================================================= */
/* Localization Text                                                          */
/* ========================================================================= */

#define TEXT_FILTER_NAME    obs_module_text("NktRnnoiseFilter")
#define TEXT_MODEL_PATH     obs_module_text("ModelPath")
#define TEXT_MODEL_PATH_DESC obs_module_text("ModelPathDesc")
#define TEXT_MODEL_FILTER   obs_module_text("ModelFileFilter")
#define TEXT_STRENGTH       obs_module_text("Strength")
#define TEXT_STRENGTH_DESC  obs_module_text("StrengthDesc")

/* ========================================================================= */
/* Utility Functions                                                          */
/* ========================================================================= */

static inline enum speaker_layout convert_speaker_layout(uint8_t channels)
{
	switch (channels) {
	case 0:
		return SPEAKERS_UNKNOWN;
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

static inline void clear_deque(struct deque *buf)
{
	deque_pop_front(buf, NULL, buf->size);
}

static RNNModel *load_model_file(const char *path)
{
	if (!path || !*path)
		return NULL; /* Use built-in model */

	FILE *f = fopen(path, "rb");
	if (!f) {
		blog(LOG_WARNING, "[NKT RNNoise] Failed to open model file: %s", path);
		return NULL;
	}

	RNNModel *model = rnnoise_model_from_file(f);
	fclose(f);

	if (!model) {
		blog(LOG_WARNING, "[NKT RNNoise] Failed to parse model file: %s", path);
		return NULL;
	}

	blog(LOG_INFO, "[NKT RNNoise] Loaded custom model: %s", path);
	return model;
}

/* ========================================================================= */
/* Filter Implementation                                                      */
/* ========================================================================= */

static const char *nkt_rnnoise_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return TEXT_FILTER_NAME;
}

static void nkt_rnnoise_destroy(void *data)
{
	struct nkt_rnnoise_data *filter = data;

	if (!filter)
		return;

	/* Free RNNoise states */
	for (size_t i = 0; i < filter->channels; i++) {
		if (filter->rnn_states[i])
			rnnoise_destroy(filter->rnn_states[i]);
		deque_free(&filter->input_buffers[i]);
		deque_free(&filter->output_buffers[i]);
	}

	/* Free buffers */
	bfree(filter->copy_buffers[0]);
	bfree(filter->rnn_segment_buffers[0]);
	bfree(filter->original_buffers[0]);

	/* Free resamplers */
	if (filter->resampler_to_48k) {
		audio_resampler_destroy(filter->resampler_to_48k);
		audio_resampler_destroy(filter->resampler_from_48k);
	}

	/* Free model */
	pthread_mutex_lock(&filter->model_mutex);
	if (filter->model)
		rnnoise_model_free(filter->model);
	bfree(filter->model_path);
	pthread_mutex_unlock(&filter->model_mutex);
	pthread_mutex_destroy(&filter->model_mutex);

	/* Free other resources */
	deque_free(&filter->info_buffer);
	da_free(filter->output_data);

	bfree(filter);
}

static void alloc_channel(struct nkt_rnnoise_data *filter, size_t channel, size_t frames)
{
	filter->rnn_states[channel] = rnnoise_create(filter->model);
	deque_reserve(&filter->input_buffers[channel], frames * sizeof(float));
	deque_reserve(&filter->output_buffers[channel], frames * sizeof(float));
}

static void nkt_rnnoise_update(void *data, obs_data_t *settings)
{
	struct nkt_rnnoise_data *filter = data;

	uint32_t sample_rate = audio_output_get_sample_rate(obs_get_audio());
	size_t channels = audio_output_get_channels(obs_get_audio());
	size_t frames = (size_t)sample_rate / (1000 / NKT_BUFFER_SIZE_MSEC);

	/* Get strength setting (0-100 -> 0.0-1.0) */
	filter->strength = (float)obs_data_get_double(settings, S_STRENGTH) / 100.0f;

	/* Process 10 millisecond segments for RNNoise */
	filter->frames = frames;
	filter->channels = channels;
	filter->latency = 1000000000LL / (1000 / NKT_BUFFER_SIZE_MSEC);

	/* Handle model path change */
	const char *new_path = obs_data_get_string(settings, S_MODEL_PATH);
	bool model_changed = false;

	pthread_mutex_lock(&filter->model_mutex);
	if (!filter->model_path && new_path && *new_path) {
		model_changed = true;
	} else if (filter->model_path && (!new_path || !*new_path)) {
		model_changed = true;
	} else if (filter->model_path && new_path && strcmp(filter->model_path, new_path) != 0) {
		model_changed = true;
	}

	if (model_changed) {
		/* Free old model */
		if (filter->model) {
			rnnoise_model_free(filter->model);
			filter->model = NULL;
		}
		bfree(filter->model_path);
		filter->model_path = NULL;

		/* Load new model */
		if (new_path && *new_path) {
			filter->model_path = bstrdup(new_path);
			filter->model = load_model_file(new_path);
		}

		/* Recreate RNNoise states with new model */
		for (size_t i = 0; i < filter->channels; i++) {
			if (filter->rnn_states[i]) {
				rnnoise_destroy(filter->rnn_states[i]);
				filter->rnn_states[i] = rnnoise_create(filter->model);
			}
		}
	}
	pthread_mutex_unlock(&filter->model_mutex);

	/* Skip if already allocated */
	if (filter->rnn_states[0])
		return;

	/* Allocate processing buffers */
	filter->copy_buffers[0] = bmalloc(frames * channels * sizeof(float));
	filter->rnn_segment_buffers[0] = bmalloc(NKT_RNNOISE_FRAME_SIZE * channels * sizeof(float));
	filter->original_buffers[0] = bmalloc(frames * channels * sizeof(float));

	for (size_t c = 1; c < channels; ++c) {
		filter->copy_buffers[c] = filter->copy_buffers[c - 1] + frames;
		filter->rnn_segment_buffers[c] = filter->rnn_segment_buffers[c - 1] + NKT_RNNOISE_FRAME_SIZE;
		filter->original_buffers[c] = filter->original_buffers[c - 1] + frames;
	}

	/* Allocate channel buffers and RNNoise states */
	for (size_t i = 0; i < channels; i++)
		alloc_channel(filter, i, frames);

	/* Set up resamplers if sample rate differs from RNNoise requirement */
	if (sample_rate == NKT_RNNOISE_SAMPLE_RATE) {
		filter->resampler_to_48k = NULL;
		filter->resampler_from_48k = NULL;
	} else {
		struct resample_info src, dst;
		src.samples_per_sec = sample_rate;
		src.format = AUDIO_FORMAT_FLOAT_PLANAR;
		src.speakers = convert_speaker_layout((uint8_t)channels);

		dst.samples_per_sec = NKT_RNNOISE_SAMPLE_RATE;
		dst.format = AUDIO_FORMAT_FLOAT_PLANAR;
		dst.speakers = convert_speaker_layout((uint8_t)channels);

		filter->resampler_to_48k = audio_resampler_create(&dst, &src);
		filter->resampler_from_48k = audio_resampler_create(&src, &dst);
	}
}

static void *nkt_rnnoise_create(obs_data_t *settings, obs_source_t *source)
{
	struct nkt_rnnoise_data *filter = bzalloc(sizeof(struct nkt_rnnoise_data));

	filter->context = source;
	pthread_mutex_init(&filter->model_mutex, NULL);

	nkt_rnnoise_update(filter, settings);

	info("Filter created");
	return filter;
}

static inline void process_rnnoise(struct nkt_rnnoise_data *filter)
{
	/* Pop from input deque and save original for mixing */
	for (size_t i = 0; i < filter->channels; i++) {
		deque_pop_front(&filter->input_buffers[i], filter->copy_buffers[i],
				filter->frames * sizeof(float));
		/* Save original for wet/dry mixing */
		memcpy(filter->original_buffers[i], filter->copy_buffers[i],
		       filter->frames * sizeof(float));
	}

	/* Convert to RNNoise format and resample if necessary */
	if (filter->resampler_to_48k) {
		float *output[NKT_MAX_AUDIO_CHANNELS];
		uint32_t out_frames;
		uint64_t ts_offset;
		audio_resampler_resample(filter->resampler_to_48k,
					 (uint8_t **)output, &out_frames, &ts_offset,
					 (const uint8_t **)filter->copy_buffers,
					 (uint32_t)filter->frames);

		for (size_t i = 0; i < filter->channels; i++) {
			for (ssize_t j = 0, k = (ssize_t)out_frames - NKT_RNNOISE_FRAME_SIZE;
			     j < NKT_RNNOISE_FRAME_SIZE; ++j, ++k) {
				if (k >= 0) {
					filter->rnn_segment_buffers[i][j] = output[i][k] * 32768.0f;
				} else {
					filter->rnn_segment_buffers[i][j] = 0;
				}
			}
		}
	} else {
		for (size_t i = 0; i < filter->channels; i++) {
			for (size_t j = 0; j < NKT_RNNOISE_FRAME_SIZE; ++j) {
				filter->rnn_segment_buffers[i][j] = filter->copy_buffers[i][j] * 32768.0f;
			}
		}
	}

	/* Execute RNNoise processing */
	for (size_t i = 0; i < filter->channels; i++) {
		rnnoise_process_frame(filter->rnn_states[i],
				      filter->rnn_segment_buffers[i],
				      filter->rnn_segment_buffers[i]);
	}

	/* Revert signal level and resample back if necessary */
	if (filter->resampler_from_48k) {
		float *output[NKT_MAX_AUDIO_CHANNELS];
		uint32_t out_frames;
		uint64_t ts_offset;
		audio_resampler_resample(filter->resampler_from_48k,
					 (uint8_t **)output, &out_frames, &ts_offset,
					 (const uint8_t **)filter->rnn_segment_buffers,
					 NKT_RNNOISE_FRAME_SIZE);

		for (size_t i = 0; i < filter->channels; i++) {
			for (ssize_t j = 0, k = (ssize_t)out_frames - filter->frames;
			     j < (ssize_t)filter->frames; ++j, ++k) {
				if (k >= 0) {
					filter->copy_buffers[i][j] = output[i][k] / 32768.0f;
				} else {
					filter->copy_buffers[i][j] = 0;
				}
			}
		}
	} else {
		for (size_t i = 0; i < filter->channels; i++) {
			for (size_t j = 0; j < NKT_RNNOISE_FRAME_SIZE; ++j) {
				filter->copy_buffers[i][j] = filter->rnn_segment_buffers[i][j] / 32768.0f;
			}
		}
	}

	/* Apply wet/dry mix based on strength */
	const float strength = filter->strength;
	const float inverse = 1.0f - strength;

	for (size_t i = 0; i < filter->channels; i++) {
		for (size_t j = 0; j < filter->frames; j++) {
			float original = filter->original_buffers[i][j];
			float filtered = filter->copy_buffers[i][j];
			filter->copy_buffers[i][j] = (filtered * strength) + (original * inverse);
		}
	}

	/* Push to output deque */
	for (size_t i = 0; i < filter->channels; i++)
		deque_push_back(&filter->output_buffers[i], filter->copy_buffers[i],
				filter->frames * sizeof(float));
}

static void reset_data(struct nkt_rnnoise_data *filter)
{
	for (size_t i = 0; i < filter->channels; i++) {
		clear_deque(&filter->input_buffers[i]);
		clear_deque(&filter->output_buffers[i]);
	}
	clear_deque(&filter->info_buffer);
}

static struct obs_audio_data *nkt_rnnoise_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct nkt_rnnoise_data *filter = data;
	struct nkt_audio_info info;
	size_t segment_size = filter->frames * sizeof(float);
	size_t out_size;

	if (!filter->rnn_states[0])
		return audio;

	/* If timestamp has dramatically changed, consider it a new stream */
	if (filter->last_timestamp) {
		int64_t diff = llabs((int64_t)filter->last_timestamp - (int64_t)audio->timestamp);
		if (diff > 1000000000LL)
			reset_data(filter);
	}

	filter->last_timestamp = audio->timestamp;

	/* Push audio packet info to info deque */
	info.frames = audio->frames;
	info.timestamp = audio->timestamp;
	deque_push_back(&filter->info_buffer, &info, sizeof(info));

	/* Push current audio data to input deque */
	for (size_t i = 0; i < filter->channels; i++)
		deque_push_back(&filter->input_buffers[i], audio->data[i],
				audio->frames * sizeof(float));

	/* Process each 10ms segment */
	while (filter->input_buffers[0].size >= segment_size)
		process_rnnoise(filter);

	/* Check if we have enough data to output */
	memset(&info, 0, sizeof(info));
	deque_peek_front(&filter->info_buffer, &info, sizeof(info));
	out_size = info.frames * sizeof(float);

	if (filter->output_buffers[0].size < out_size)
		return NULL;

	/* Pop and return processed audio */
	deque_pop_front(&filter->info_buffer, NULL, sizeof(info));
	da_resize(filter->output_data, out_size * filter->channels);

	for (size_t i = 0; i < filter->channels; i++) {
		filter->output_audio.data[i] = (uint8_t *)&filter->output_data.array[i * out_size];
		deque_pop_front(&filter->output_buffers[i], filter->output_audio.data[i], out_size);
	}

	filter->output_audio.frames = info.frames;
	filter->output_audio.timestamp = info.timestamp - filter->latency;
	return &filter->output_audio;
}

static void nkt_rnnoise_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_MODEL_PATH, "");
	obs_data_set_default_double(settings, S_STRENGTH, 100.0);
}

static obs_properties_t *nkt_rnnoise_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	/* Model file path picker */
	obs_property_t *model_prop = obs_properties_add_path(
		props,
		S_MODEL_PATH,
		TEXT_MODEL_PATH,
		OBS_PATH_FILE,
		TEXT_MODEL_FILTER,
		NULL
	);
	obs_property_set_long_description(model_prop, TEXT_MODEL_PATH_DESC);

	/* Strength slider (0-100%) */
	obs_property_t *strength_prop = obs_properties_add_float_slider(
		props,
		S_STRENGTH,
		TEXT_STRENGTH,
		0.0,    /* min */
		100.0,  /* max */
		1.0     /* step */
	);
	obs_property_float_set_suffix(strength_prop, "%");
	obs_property_set_long_description(strength_prop, TEXT_STRENGTH_DESC);

	return props;
}

/* ========================================================================= */
/* Filter Info Structure                                                      */
/* ========================================================================= */

struct obs_source_info nkt_rnnoise_filter_info = {
	.id             = NKT_RNNOISE_FILTER_ID,
	.type           = OBS_SOURCE_TYPE_FILTER,
	.output_flags   = OBS_SOURCE_AUDIO,
	.get_name       = nkt_rnnoise_name,
	.create         = nkt_rnnoise_create,
	.destroy        = nkt_rnnoise_destroy,
	.update         = nkt_rnnoise_update,
	.filter_audio   = nkt_rnnoise_filter_audio,
	.get_defaults   = nkt_rnnoise_defaults,
	.get_properties = nkt_rnnoise_properties,
};

/* ========================================================================= */
/* Module Entry Points                                                        */
/* ========================================================================= */

bool obs_module_load(void)
{
	obs_register_source(&nkt_rnnoise_filter_info);

	blog(LOG_INFO, "[NKT RNNoise Filter] Plugin loaded successfully");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[NKT RNNoise Filter] Plugin unloaded");
}
