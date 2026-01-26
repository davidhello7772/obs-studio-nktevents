/*
 * Audio Monitor Plugin - Main Window
 *
 * Window displaying audio meters for all non-hidden, active audio sources.
 * Provides a horizontal scrolling layout of per-source audio cards with
 * volume meters, type controls (MIX/TRANS/REF), and filter level controls.
 *
 * Architecture:
 * - Listens to OBS source_activate/deactivate/remove signals
 * - Creates AudioSourceWidget for each visible audio source
 * - Persists colors, types, and filter levels with scene collection
 * - Integrates VolumeMonitor for translation level alerts
 *
 * Thread Safety:
 * - All OBS signal callbacks marshal to Qt thread via QMetaObject::invokeMethod
 * - Safe to access sourceWidgets map only from Qt thread
 */

#pragma once

#include "audio-source-widget.hpp"

#include <obs.hpp>

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

class VolumeMonitor;

class AudioMonitorWindow : public QDialog {
	Q_OBJECT

public:
	explicit AudioMonitorWindow(QWidget *parent = nullptr);
	~AudioMonitorWindow();

	/**
	 * @brief Toggle window visibility and start/stop volume monitoring.
	 *
	 * When shown, starts the VolumeMonitor timer for translation alerts.
	 * When hidden, stops monitoring to save resources.
	 */
	void ToggleShowHide();

	/**
	 * @brief Save or load color, type, and filter level settings.
	 *
	 * Called by OBS save/load callbacks. Persists per-source settings
	 * (colors, types, filter levels) with the scene collection.
	 *
	 * @param save_data OBS data object for save/load
	 * @param saving true = save settings, false = load settings
	 */
	void SaveLoadColorSettings(obs_data_t *save_data, bool saving);

	/**
	 * @brief Clear and repopulate all source widgets.
	 *
	 * Enumerates all OBS sources and creates widgets for visible audio sources.
	 * Called on initialization and scene collection change.
	 */
	void RefreshSources();

	/**
	 * @brief Get the map of source UUID to widget.
	 * @return Const reference to source widgets map (Qt thread only)
	 */
	const QMap<QString, AudioSourceWidget *> &GetSourceWidgets() const { return sourceWidgets; }

	/**
	 * @brief Get the UUID of the Reference source for level comparison.
	 * @return UUID string, or empty if no reference set
	 */
	const QString &GetReferenceUuid() const { return referenceUuid; }

	/** @brief Filter level presets for translated sources */
	enum class PresetType { Teaching, Meditation, NoFiltering };

	/**
	 * @brief Apply a filter level preset to all Translated sources.
	 *
	 * Teaching = HIGH (2 RNNoise filters), Meditation = LOW (1 filter),
	 * NoFiltering = OFF (0 filters).
	 *
	 * @param preset The preset to apply
	 */
	void ApplyPreset(PresetType preset);

	/**
	 * @brief Get the current threshold values for meter display.
	 * Mix sources should add getMixOffset() to these values.
	 */
	double getNominalThreshold() const { return nominalThreshold; }
	double getWarningThreshold() const { return warningThreshold; }
	double getErrorThreshold() const { return errorThreshold; }
	double getMixOffset() const { return mixOffset; }

	/** @brief Default values */
	static constexpr double DEFAULT_NOMINAL_THRESHOLD = -9.0;
	static constexpr double DEFAULT_WARNING_THRESHOLD = -6.0;
	static constexpr double DEFAULT_ERROR_THRESHOLD = -3.0;
	static constexpr double DEFAULT_MIX_OFFSET = -25.0;
	static constexpr double MIX_OFFSET_MIN = -40.0;
	static constexpr double MIX_OFFSET_MAX = -10.0;

private slots:
	void OnCardWidthDecrease();
	void OnCardWidthIncrease();
	void OnCardWidthReset();

	// Threshold control slots
	void OnNominalDecrease();
	void OnNominalIncrease();
	void OnNominalReset();
	void OnWarningDecrease();
	void OnWarningIncrease();
	void OnWarningReset();
	void OnErrorDecrease();
	void OnErrorIncrease();
	void OnErrorReset();
	void OnMixOffsetDecrease();
	void OnMixOffsetIncrease();
	void OnMixOffsetReset();

private:
	// UI elements
	QWidget *headerWidget;
	QPushButton *presetTeaching;
	QPushButton *presetMeditation;
	QPushButton *presetNoFilter;
	QScrollArea *scrollArea;
	QWidget *containerWidget;
	QHBoxLayout *containerLayout;

	// Card width controls
	QPushButton *widthDecreaseBtn;
	QPushButton *widthIncreaseBtn;
	QPushButton *widthValueBtn;  // Clickable label showing width (click to reset)
	int currentCardWidth;  // Current card width in pixels

	// Threshold controls (Levels: green, orange, red)
	QPushButton *nominalMinusBtn;
	QPushButton *nominalValueBtn;
	QPushButton *nominalPlusBtn;
	QPushButton *warningMinusBtn;
	QPushButton *warningValueBtn;
	QPushButton *warningPlusBtn;
	QPushButton *errorMinusBtn;
	QPushButton *errorValueBtn;
	QPushButton *errorPlusBtn;

	// Mix offset controls
	QPushButton *mixOffsetMinusBtn;
	QPushButton *mixOffsetValueBtn;
	QPushButton *mixOffsetPlusBtn;

	// Current threshold values (dBFS) - applies to Translated sources
	// Mix sources use these values + mixOffset
	double nominalThreshold = DEFAULT_NOMINAL_THRESHOLD;
	double warningThreshold = DEFAULT_WARNING_THRESHOLD;
	double errorThreshold = DEFAULT_ERROR_THRESHOLD;
	double mixOffset = DEFAULT_MIX_OFFSET;

	// Source tracking: source UUID -> widget
	QMap<QString, AudioSourceWidget *> sourceWidgets;

	// Filter level persistence
	QMap<QString, int> filterLevels;  // uuid -> filter level (0/1/2)

	// Stored color settings
	QMap<QString, QColor> sourceColors;

	// Source type tracking
	QString mixReferenceUuid;    // Only one Mix source allowed
	QString referenceUuid;       // Only one Reference source allowed (for level comparison)
	QMap<QString, AudioSourceWidget::SourceType> sourceTypes;

	// Volume monitor for translation level alerts
	VolumeMonitor *volumeMonitor = nullptr;

	// Signal connections
	bool signalsConnected = false;

	// Internal methods
	void CreateHeader();
	void CreateThresholdControls(QHBoxLayout *layout);
	void UpdatePresetButtonStates();
	void UpdateWidthControls();
	void UpdateThresholdControls();
	void UpdateMixOffsetControls();
	void ApplyCardWidthToAll();
	void ApplyThresholdsToAll();
	void ConnectSignals();
	void DisconnectSignals();
	void AddSource(obs_source_t *source);
	void RemoveSource(obs_source_t *source);
	void RemoveSourceByUuid(const QString &uuid);
	bool ShouldShowSource(obs_source_t *source);
	void OnSourceTypeChanged(AudioSourceWidget *widget, AudioSourceWidget::SourceType newType);
	void OnFilterLevelChanged(AudioSourceWidget *widget, int newLevel);

	// Static signal handlers
	static void OnSourceActivate(void *data, calldata_t *cd);
	static void OnSourceDeactivate(void *data, calldata_t *cd);
	static void OnSourceRemove(void *data, calldata_t *cd);
};
