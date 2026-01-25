/*
 * NKT Custom RNNoise Filter Plugin for OBS Studio
 *
 * Provides noise filtering with custom RNNoise model support
 * and adjustable filter strength.
 *
 * Copyright (C) 2024 NKT Events
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NKT_RNNOISE_FILTER_H
#define NKT_RNNOISE_FILTER_H

#include <obs-module.h>
#include <util/deque.h>
#include <util/darray.h>
#include <util/threading.h>
#include <media-io/audio-resampler.h>
#include <rnnoise.h>

/* Filter ID */
#define NKT_RNNOISE_FILTER_ID "nkt_rnnoise_filter"

/* RNNoise constants (fixed by RNNoise library) */
#define NKT_RNNOISE_SAMPLE_RATE 48000
#define NKT_RNNOISE_FRAME_SIZE  480

/* Maximum channels supported */
#define NKT_MAX_AUDIO_CHANNELS 8

/* Buffer size in milliseconds (must be 10ms for RNNoise) */
#define NKT_BUFFER_SIZE_MSEC 10

/* Settings keys */
#define S_MODEL_PATH    "model_path"
#define S_STRENGTH      "strength"

/* Audio frame info for deque */
struct nkt_audio_info {
	uint32_t frames;
	uint64_t timestamp;
};

/* Main filter data structure */
struct nkt_rnnoise_data {
	obs_source_t *context;

	/* Channel configuration */
	size_t channels;
	size_t frames;

	/* Timing */
	uint64_t last_timestamp;
	uint64_t latency;

	/* Model management (thread-safe swap) */
	pthread_mutex_t model_mutex;
	RNNModel *model;           /* Currently active model (NULL = built-in) */
	char *model_path;          /* Path to current model file */

	/* Filter strength (0.0 - 1.0) */
	float strength;

	/* RNNoise state per channel */
	DenoiseState *rnn_states[NKT_MAX_AUDIO_CHANNELS];

	/* Resamplers (only if sample rate != 48kHz) */
	audio_resampler_t *resampler_to_48k;
	audio_resampler_t *resampler_from_48k;

	/* Circular buffers for input/output */
	struct deque input_buffers[NKT_MAX_AUDIO_CHANNELS];
	struct deque output_buffers[NKT_MAX_AUDIO_CHANNELS];
	struct deque info_buffer;

	/* Processing buffers */
	float *copy_buffers[NKT_MAX_AUDIO_CHANNELS];
	float *rnn_segment_buffers[NKT_MAX_AUDIO_CHANNELS];
	float *original_buffers[NKT_MAX_AUDIO_CHANNELS]; /* For wet/dry mixing */

	/* Output data */
	struct obs_audio_data output_audio;
	DARRAY(float) output_data;
};

/* External filter info declaration */
extern struct obs_source_info nkt_rnnoise_filter_info;

#endif /* NKT_RNNOISE_FILTER_H */
