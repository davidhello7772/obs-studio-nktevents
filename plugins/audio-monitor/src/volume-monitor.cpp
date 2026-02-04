/*
 * Audio Monitor Plugin - Volume Monitor
 * Monitors translation levels against reference and triggers alerts
 */

#include "volume-monitor.hpp"
#include "audio-monitor-window.hpp"

#include <QDateTime>

VolumeMonitor::VolumeMonitor(AudioMonitorWindow *window)
	: QObject(window),
	  monitorWindow(window)
{
	checkTimer = new QTimer(this);
	connect(checkTimer, &QTimer::timeout, this, &VolumeMonitor::checkVolumeLevels);
}

VolumeMonitor::~VolumeMonitor()
{
	stop();
}

void VolumeMonitor::start()
{
	if (!checkTimer->isActive()) {
		lowVolumeStartTime.clear();
		highVolumeStartTime.clear();
		checkTimer->start(1000);  // Check every 1 second
	}
}

void VolumeMonitor::stop()
{
	if (checkTimer->isActive()) {
		checkTimer->stop();
	}

	// Clear all warnings when stopping
	// Note: null check added as defensive measure against race conditions during shutdown
	for (auto *widget : monitorWindow->GetSourceWidgets().values()) {
		if (widget && widget->GetWarningState() != AudioSourceWidget::WarningState::None) {
			widget->SetWarningState(AudioSourceWidget::WarningState::None);
		}
	}

	lowVolumeStartTime.clear();
	highVolumeStartTime.clear();
}

void VolumeMonitor::checkVolumeLevels()
{
	const QString &referenceUuid = monitorWindow->GetReferenceUuid();
	const auto &widgets = monitorWindow->GetSourceWidgets();

	// Need a reference source to compare against
	if (referenceUuid.isEmpty() || !widgets.contains(referenceUuid)) {
		// No reference - clear all warnings
		for (auto *widget : widgets.values()) {
			if (widget->GetWarningState() != AudioSourceWidget::WarningState::None) {
				widget->SetWarningState(AudioSourceWidget::WarningState::None);
			}
		}
		lowVolumeStartTime.clear();
		highVolumeStartTime.clear();
		return;
	}

	AudioSourceWidget *referenceWidget = widgets[referenceUuid];
	double referenceDb = referenceWidget->GetAverageDb();
	qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

	// Check all translated sources
	for (auto it = widgets.begin(); it != widgets.end(); ++it) {
		const QString &uuid = it.key();
		AudioSourceWidget *widget = it.value();

		// Only monitor Translated sources
		if (widget->GetSourceType() != AudioSourceWidget::SourceType::Translated) {
			// Clear any existing warnings for non-translated sources
			if (widget->GetWarningState() != AudioSourceWidget::WarningState::None) {
				widget->SetWarningState(AudioSourceWidget::WarningState::None);
			}
			lowVolumeStartTime.remove(uuid);
			highVolumeStartTime.remove(uuid);
			continue;
		}

		double translatedDb = widget->GetAverageDb();
		double diff = translatedDb - referenceDb;

		// Check HIGH VOLUME: translated > reference + threshold
		if (diff > WARNING_THRESHOLD_DB) {
			// High volume condition met
			if (!highVolumeStartTime.contains(uuid)) {
				highVolumeStartTime[uuid] = currentTime;
			}

			// Check if condition persisted for 6 seconds
			if (currentTime - highVolumeStartTime[uuid] >= ALERT_DELAY_MS) {
				widget->SetWarningState(AudioSourceWidget::WarningState::HighVolume);
			}

			// Clear low volume tracking
			lowVolumeStartTime.remove(uuid);
		}
		// Check LOW VOLUME: reference > translated + threshold AND reference is audible
		else if (-diff > WARNING_THRESHOLD_DB && referenceDb >= MIN_REFERENCE_DB) {
			// Low volume condition met
			if (!lowVolumeStartTime.contains(uuid)) {
				lowVolumeStartTime[uuid] = currentTime;
			}

			// Check if condition persisted for 6 seconds
			if (currentTime - lowVolumeStartTime[uuid] >= ALERT_DELAY_MS) {
				widget->SetWarningState(AudioSourceWidget::WarningState::LowVolume);
			}

			// Clear high volume tracking
			highVolumeStartTime.remove(uuid);
		}
		// Volume is OK
		else {
			// Clear any active warning immediately
			if (widget->GetWarningState() != AudioSourceWidget::WarningState::None) {
				widget->SetWarningState(AudioSourceWidget::WarningState::None);
			}
			lowVolumeStartTime.remove(uuid);
			highVolumeStartTime.remove(uuid);
		}
	}
}
