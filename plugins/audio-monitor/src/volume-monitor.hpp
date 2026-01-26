/*
 * Audio Monitor Plugin - Volume Monitor
 *
 * Monitors translation levels against reference and triggers alerts.
 * Compares average dB levels of Translated sources against the Reference
 * source, triggering HIGH or LOW volume warnings after a hysteresis delay.
 *
 * Alert Logic:
 * - HIGH: Translated > Reference + 10dB for 6 seconds
 * - LOW: Reference > Translated + 10dB for 6 seconds (and reference > -40dB)
 *
 * Thread Safety:
 * - Runs entirely on Qt thread via QTimer
 * - Reads AudioSourceWidget state (safe, same thread)
 * - Updates warning states via Qt methods (safe)
 */

#pragma once

#include "audio-source-widget.hpp"

#include <QMap>
#include <QObject>
#include <QTimer>

class AudioMonitorWindow;

class VolumeMonitor : public QObject {
	Q_OBJECT

public:
	/**
	 * @brief Construct a volume monitor for the given window.
	 *
	 * Creates the check timer but does not start it.
	 *
	 * @param window Parent window providing source widgets
	 */
	explicit VolumeMonitor(AudioMonitorWindow *window);

	~VolumeMonitor();

	/**
	 * @brief Start monitoring volume levels.
	 *
	 * Starts the 1-second check timer. Safe to call multiple times.
	 * Call when window becomes visible.
	 */
	void start();

	/**
	 * @brief Stop monitoring and clear all warnings.
	 *
	 * Stops the check timer and clears warning states on all widgets.
	 * Call when window becomes hidden.
	 */
	void stop();

private slots:
	void checkVolumeLevels();

private:
	AudioMonitorWindow *monitorWindow;
	QTimer *checkTimer = nullptr;

	// Configuration (like KFS)
	static constexpr double WARNING_THRESHOLD_DB = 10.0;  // dB difference to trigger warning
	static constexpr double MIN_REFERENCE_DB = -40.0;     // Reference must be audible for low volume alert
	static constexpr int ALERT_DELAY_MS = 6000;           // 6 seconds before triggering alert

	// Track how long each translated source has been in warning condition
	QMap<QString, qint64> lowVolumeStartTime;
	QMap<QString, qint64> highVolumeStartTime;
};
