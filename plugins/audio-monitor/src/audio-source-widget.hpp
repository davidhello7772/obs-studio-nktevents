/*
 * Audio Monitor Plugin - Audio Source Widget
 * Per-source widget with vertical meter and colored background
 */

#pragma once

#include <obs.hpp>

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <deque>

class FilterListWidget;
class VolumeMeter;

class AudioSourceWidget : public QFrame {
	Q_OBJECT

public:
	explicit AudioSourceWidget(obs_source_t *source, const QColor &color, QWidget *parent = nullptr);
	~AudioSourceWidget();

	void SetBackgroundColor(const QColor &color);
	QString GetSourceUuid() const;
	QString GetSourceName() const;

	// Source type: Normal, Mix reference, Translated channel, or Reference language
	enum class SourceType { Normal, Mix, Translated, Reference };
	SourceType GetSourceType() const { return sourceType; }
	void SetSourceType(SourceType type);

	// Warning state for level alerts (only for Translated sources)
	enum class WarningState { None, HighVolume, LowVolume };
	void SetWarningState(WarningState state);
	WarningState GetWarningState() const { return warningState; }

	// Get average audio level in dB (for VolumeMonitor comparison)
	double GetAverageDb() const;

signals:
	void colorChanged(const QColor &newColor);
	void sourceTypeChanged(AudioSourceWidget *widget, SourceType newType);

private slots:
	void OnColorButtonClicked();
	void OnTypeButtonClicked();
	void OnBlinkTimer();
	void OnMonitorButtonClicked();

private:
	// OBS handles (RAII wrappers)
	OBSSource source;
	OBSVolMeter obs_volmeter;
	std::vector<OBSSignal> sigs;

	// UI elements
	QLabel *nameLabel;
	QLabel *typeBadge;
	QLabel *warningLabel;
	QWidget *badgeContainer;  // Fixed-height container for consistent layout
	VolumeMeter *volMeter;
	QPushButton *colorButton;
	QPushButton *typeButton;
	QPushButton *monitorButton;
	FilterListWidget *filterList;

	// State
	QColor currentColor;
	QColor savedColor;  // Original color before warning
	SourceType sourceType = SourceType::Normal;
	WarningState warningState = WarningState::None;
	obs_monitoring_type currentMonitoringType = OBS_MONITORING_TYPE_NONE;

	// Volume history for averaging (like KFS: ~10 seconds of samples at ~10Hz)
	std::deque<double> volumeHistory;
	static constexpr size_t VOLUME_HISTORY_SIZE = 100;
	double lastMagnitudeDb = -60.0;

	// Warning animation
	QTimer *blinkTimer = nullptr;
	bool blinkVisible = true;

	void UpdateBackgroundStyle();
	void UpdateTypeButtonStyle();
	void UpdateTypeBadge();
	void ApplyMeterThresholds();
	void UpdateWarningDisplay();
	void UpdateMonitorButtonStyle();
	void UpdateMonitorButtonFromOBS(int type);
	static bool IsMonitoringAvailable();

	// RNNoise filter helpers
	static bool IsRnnoisePluginAvailable();
	int CountRnnoiseFilters() const;

	// RNNoise filter ID (use ID for stability)
	static constexpr const char *RNNOISE_FILTER_ID = "rnnoise_filter_sh_model";

	// Maximum number of RNNoise filters allowed per source
	static constexpr int MAX_RNNOISE_FILTERS = 2;

	// Static callbacks
	static void OBSVolumeLevel(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS]);

	static void OBSSourceRenamed(void *data, calldata_t *cd);
	static void OBSFilterChanged(void *data, calldata_t *cd);
	static void OBSMonitoringTypeChanged(void *data, calldata_t *cd);
};
