/*
 * RNNoise Filter with SH Model for OBS Studio
 *
 * Provides noise filtering with custom RNNoise model support
 * and adjustable filter strength.
 *
 * Copyright (C) 2024 12 Tracks Multilingual
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RNNOISE_FILTER_SH_MODEL_H
#define RNNOISE_FILTER_SH_MODEL_H

#include <obs-module.h>
#include <util/deque.h>
#include <util/darray.h>
#include <util/threading.h>
#include <media-io/audio-resampler.h>
#include <rnnoise.h>

/* Filter ID */
#define RNNOISE_SH_FILTER_ID "rnnoise_filter_sh_model"

/* RNNoise constants (fixed by RNNoise library) */
#define RNNOISE_SH_SAMPLE_RATE 48000
#define RNNOISE_SH_FRAME_SIZE  480

/* Maximum channels supported */
#define RNNOISE_SH_MAX_AUDIO_CHANNELS 8

/* Buffer size in milliseconds (must be 10ms for RNNoise) */
#define RNNOISE_SH_BUFFER_SIZE_MSEC 10

/* Settings keys */
#define S_STRENGTH      "strength"

/* Embedded model filename (in plugin data directory) */
#define RNNOISE_SH_MODEL_FILENAME "sh.rnnn"

/* Audio frame info for deque */
struct rnnoise_sh_audio_info {
	uint32_t frames;
	uint64_t timestamp;
};

/* Main filter data structure */
struct rnnoise_sh_data {
	obs_source_t *context;

	/* Channel configuration */
	size_t channels;
	size_t frames;

	/* Timing */
	uint64_t last_timestamp;
	uint64_t latency;

	/* Model management */
	pthread_mutex_t model_mutex;
	RNNModel *model;           /* Custom model (NULL = built-in) */
	bool model_loaded;         /* Whether we attempted to load the model */

	/* Filter strength (0.0 - 1.0) */
	float strength;

	/* RNNoise state per channel */
	DenoiseState *rnn_states[RNNOISE_SH_MAX_AUDIO_CHANNELS];

	/* Resamplers (only if sample rate != 48kHz) */
	audio_resampler_t *resampler_to_48k;
	audio_resampler_t *resampler_from_48k;

	/* Circular buffers for input/output */
	struct deque input_buffers[RNNOISE_SH_MAX_AUDIO_CHANNELS];
	struct deque output_buffers[RNNOISE_SH_MAX_AUDIO_CHANNELS];
	struct deque info_buffer;

	/* Processing buffers */
	float *copy_buffers[RNNOISE_SH_MAX_AUDIO_CHANNELS];
	float *rnn_segment_buffers[RNNOISE_SH_MAX_AUDIO_CHANNELS];
	float *original_buffers[RNNOISE_SH_MAX_AUDIO_CHANNELS]; /* For wet/dry mixing */

	/* Output data */
	struct obs_audio_data output_audio;
	DARRAY(float) output_data;
};

/* External filter info declaration */
extern struct obs_source_info rnnoise_sh_filter_info;

#endif /* RNNOISE_FILTER_SH_MODEL_H */
