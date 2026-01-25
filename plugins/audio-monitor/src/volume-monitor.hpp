/*
 * Audio Monitor Plugin - Volume Monitor
 * Monitors translation levels against reference and triggers alerts
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
	explicit VolumeMonitor(AudioMonitorWindow *window);
	~VolumeMonitor();

	void start();
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
