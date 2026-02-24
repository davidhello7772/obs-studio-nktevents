/*
 * Audio Monitor Plugin - Audio Source Widget
 *
 * Per-source widget displaying a vertical volume meter, type badge,
 * and control buttons for source type, filter level, and audio monitoring.
 *
 * Architecture:
 * - Wraps an OBS source with RAII (OBSSource, OBSVolMeter, OBSSignal)
 * - Tracks volume history for averaging (~10 seconds at ~10Hz)
 * - Manages RNNoise filters (0-2 per source) via filter level controls
 * - Displays warning states (HIGH/LOW volume) with visual animations
 *
 * Thread Safety:
 * - OBSVolumeLevel callback runs on audio thread, only updates volumeHistory
 * - All other callbacks marshal to Qt thread via QMetaObject::invokeMethod
 * - UI updates must happen on Qt thread only
 *
 * Ownership:
 * - Qt parent-child relationships manage widget lifetime
 * - OBS RAII wrappers manage source/volmeter/signal lifetime
 * - Destructor disconnects callbacks before RAII cleanup
 */

#pragma once

#include <obs.hpp>

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <deque>

class FilterListWidget;
class VolumeMeter;

class AudioSourceWidget : public QFrame {
	Q_OBJECT

public:
	/**
	 * @brief Construct a widget for the given OBS source.
	 *
	 * Creates all UI elements, attaches volume meter, and connects signals.
	 * Widget is 155px wide with vertical meter and control buttons.
	 *
	 * @param source OBS source to monitor (widget takes weak reference)
	 * @param color Initial background color
	 * @param parent Qt parent widget
	 */
	explicit AudioSourceWidget(obs_source_t *source, const QColor &color, QWidget *parent = nullptr);

	/**
	 * @brief Destructor - disconnects callbacks and cleans up resources.
	 *
	 * Order is critical: disconnect volume callback before RAII destroys volmeter,
	 * then clear signals, then Qt handles child widgets.
	 */
	~AudioSourceWidget();

	/**
	 * @brief Set the card background color.
	 *
	 * If no warning is active, updates display immediately.
	 * Color is saved for restoration when warning clears.
	 *
	 * @param color New background color
	 */
	void SetBackgroundColor(const QColor &color);

	/** @brief Get the OBS source UUID. */
	QString GetSourceUuid() const;

	/** @brief Get the OBS source display name. */
	QString GetSourceName() const;

	/**
	 * @brief Source type enumeration for multilingual broadcasting.
	 *
	 * - Normal: Standard audio source (no special treatment)
	 * - Mix: Reference mix for comparison (only one allowed)
	 * - Translated: Translation channel (monitored for HIGH/LOW volume)
	 * - Reference: Reference language for comparison (only one allowed)
	 */
	enum class SourceType { Normal, Mix, Translated, Reference };

	/** @brief Get the current source type. */
	SourceType GetSourceType() const { return sourceType; }

	/**
	 * @brief Set the source type and update visuals.
	 *
	 * Updates badge, button styles, meter thresholds, and clears warnings.
	 *
	 * @param type New source type
	 */
	void SetSourceType(SourceType type);

	/**
	 * @brief Warning state for translation level alerts.
	 *
	 * - None: No warning (normal display)
	 * - HighVolume: Translated volume too high vs reference
	 * - LowVolume: Translated volume too low vs reference
	 */
	enum class WarningState { None, HighVolume, LowVolume };

	/**
	 * @brief Set the warning state and update display.
	 *
	 * Changes background color, shows/hides warning badge, starts blink timer.
	 *
	 * @param state New warning state
	 */
	void SetWarningState(WarningState state);

	/** @brief Get the current warning state. */
	WarningState GetWarningState() const { return warningState; }

	/**
	 * @brief Get the average audio level over recent history.
	 *
	 * Averages the last ~10 seconds of peak values (100 samples at ~10Hz).
	 * Used by VolumeMonitor for comparing Translated vs Reference levels.
	 *
	 * @return Average level in dBFS, or -60.0 if no history
	 */
	double GetAverageDb() const;

	/** @brief Get the current filter level (0=OFF, 1=LOW, 2=HIGH). */
	int GetFilterLevel() const { return filterLevel; }

	/**
	 * @brief Set the filter level and apply RNNoise filters.
	 *
	 * Adds or removes RNNoise filters to match the target level:
	 * - 0 (OFF): No filters
	 * - 1 (LOW): 1 RNNoise filter
	 * - 2 (HIGH): 2 RNNoise filters
	 *
	 * @param level Target filter level (clamped to 0-2)
	 */
	void SetFilterLevel(int level);

	/**
	 * @brief Set the card width dynamically.
	 *
	 * Updates the minimum and fixed width of the widget.
	 * Used by AudioMonitorWindow to allow user-adjustable card sizes.
	 *
	 * @param width New width in pixels (should be within MIN/MAX bounds)
	 */
	void SetCardWidth(int width);

	/**
	 * @brief Apply meter thresholds based on source type.
	 *
	 * Reads threshold values from parent AudioMonitorWindow and applies them
	 * to the volume meter. Mix sources get an offset applied to the thresholds.
	 * Called when thresholds change or source type changes.
	 */
	void ApplyMeterThresholds();

signals:
	void colorChanged(const QColor &newColor);
	void sourceTypeChanged(AudioSourceWidget *widget, SourceType newType);
	void filterLevelChanged(AudioSourceWidget *widget, int newLevel);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;

private slots:
	void OnColorButtonClicked();
	void OnTypeMixClicked();
	void OnTypeTransClicked();
	void OnTypeRefClicked();
	void OnBlinkTimer();
	void OnMonitorButtonClicked();
	void OnFilterLevelUp();
	void OnFilterLevelDown();

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
	QPushButton *monitorButton;

	// FilterListWidget - Currently unused (nullptr). Kept for potential future use
	// to display individual filter details when filter level controls are expanded.
	// The FilterListWidget and FilterItemWidget classes remain functional for this.
	FilterListWidget *filterList;

	// Type selection buttons (MIX, TRANS, REF)
	QWidget *typeControlsContainer;
	QPushButton *typeMixBtn;
	QPushButton *typeTransBtn;
	QPushButton *typeRefBtn;

	// Filter level UI elements
	int filterLevel = 0;  // 0=OFF, 1=LOW, 2=HIGH
	QWidget *filterLevelContainer;
	QPushButton *filterMinusBtn;
	QPushButton *filterPlusBtn;
	QWidget *filterIndicator;
	QLabel *filterLevelBadge;  // Solid pill badge: OFF (muted) / LOW (green) / HIGH (orange)
	QWidget *meterWell;      // Dark overlay for meter
	QWidget *controlsWell;   // Dark overlay for controls

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

	// Drag-and-drop
	QPoint dragStartPosition;
	static constexpr int DRAG_THRESHOLD = 10;

	void UpdateBackgroundStyle();
	void UpdateTypeButtonsStyle();
	void UpdateTypeBadge();
	void CreateTypeControls(QVBoxLayout *parentLayout);
	void UpdateWarningDisplay();
	void UpdateMonitorButtonStyle();
	void UpdateMonitorButtonFromOBS(int type);
	static bool IsMonitoringAvailable();

	// RNNoise filter helpers
	static bool IsRnnoisePluginAvailable();
	int CountRnnoiseFilters() const;

	// RNNoise filter management
	void ApplyFilterLevel(int targetLevel);
	void AddRnnoiseFilter();
	void RemoveRnnoiseFilter();
	void RemoveAllRnnoiseFilters();
	void UpdateFilterLevelDisplay();
	QString GenerateRnnoiseFilterName(int index) const;
	void CreateFilterLevelControls(QVBoxLayout *parentLayout);

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
