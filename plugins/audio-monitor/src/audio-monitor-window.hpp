/*
 * Audio Monitor Plugin - Main Window
 * Window displaying audio meters for all non-hidden, active audio sources
 */

#pragma once

#include "audio-source-widget.hpp"

#include <obs.hpp>

#include <QDialog>
#include <QHBoxLayout>
#include <QMap>
#include <QScrollArea>

class VolumeMonitor;

class AudioMonitorWindow : public QDialog {
	Q_OBJECT

public:
	explicit AudioMonitorWindow(QWidget *parent = nullptr);
	~AudioMonitorWindow();

	void ToggleShowHide();
	void SaveLoadColorSettings(obs_data_t *save_data, bool saving);
	void RefreshSources();

	// Accessors for VolumeMonitor
	const QMap<QString, AudioSourceWidget *> &GetSourceWidgets() const { return sourceWidgets; }
	const QString &GetReferenceUuid() const { return referenceUuid; }

private:
	// UI elements
	QScrollArea *scrollArea;
	QWidget *containerWidget;
	QHBoxLayout *containerLayout;

	// Source tracking: source UUID -> widget
	QMap<QString, AudioSourceWidget *> sourceWidgets;

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
	void ConnectSignals();
	void DisconnectSignals();
	void AddSource(obs_source_t *source);
	void RemoveSource(obs_source_t *source);
	void RemoveSourceByUuid(const QString &uuid);
	bool ShouldShowSource(obs_source_t *source);
	void OnSourceTypeChanged(AudioSourceWidget *widget, AudioSourceWidget::SourceType newType);

	// Static signal handlers
	static void OnSourceActivate(void *data, calldata_t *cd);
	static void OnSourceDeactivate(void *data, calldata_t *cd);
	static void OnSourceRemove(void *data, calldata_t *cd);
};
