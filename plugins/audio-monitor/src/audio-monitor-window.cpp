/*
 * Audio Monitor Plugin - Main Window
 * Window displaying audio meters for all non-hidden, active audio sources
 */

#include "audio-monitor-window.hpp"
#include "audio-source-widget.hpp"
#include "volume-monitor.hpp"

#include <obs-module.h>

#include <QLabel>

AudioMonitorWindow::AudioMonitorWindow(QWidget *parent)
	: QDialog(parent, Qt::Window)
{
	setWindowTitle(obs_module_text("AudioMonitor.Window.Title"));
	setMinimumSize(500, 350);
	resize(900, 400);

	// Create scrollable container for meters (horizontal scrolling)
	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	containerWidget = new QWidget();
	containerLayout = new QHBoxLayout(containerWidget);
	containerLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	containerLayout->setSpacing(0);  // We add spacers manually between widgets
	containerLayout->setContentsMargins(16, 16, 16, 16);

	scrollArea->setWidget(containerWidget);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addWidget(scrollArea);
	setLayout(mainLayout);

	// Create volume monitor for translation level alerts
	volumeMonitor = new VolumeMonitor(this);

	// Connect to global source signals
	ConnectSignals();

	// Initial population of sources
	RefreshSources();
}

AudioMonitorWindow::~AudioMonitorWindow()
{
	// 1. Stop volume monitor (must stop before widgets are deleted)
	if (volumeMonitor) {
		volumeMonitor->stop();
		// Qt will delete volumeMonitor since it's a child of this window
	}

	// 2. Disconnect global signal handlers (before widgets are deleted)
	DisconnectSignals();

	// 3. Delete all source widgets explicitly (triggers their destructors)
	// This is explicit because widgets are tracked in sourceWidgets map
	for (auto *widget : sourceWidgets.values()) {
		delete widget;
	}
	sourceWidgets.clear();

	// Qt will then delete remaining child objects (containerWidget, scrollArea, etc.)
}

void AudioMonitorWindow::ConnectSignals()
{
	if (signalsConnected)
		return;

	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_activate", OnSourceActivate, this);
	signal_handler_connect(sh, "source_deactivate", OnSourceDeactivate, this);
	signal_handler_connect(sh, "source_remove", OnSourceRemove, this);
	signalsConnected = true;
}

void AudioMonitorWindow::DisconnectSignals()
{
	if (!signalsConnected)
		return;

	signal_handler_t *sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "source_activate", OnSourceActivate, this);
	signal_handler_disconnect(sh, "source_deactivate", OnSourceDeactivate, this);
	signal_handler_disconnect(sh, "source_remove", OnSourceRemove, this);
	signalsConnected = false;
}

void AudioMonitorWindow::ToggleShowHide()
{
	if (isVisible()) {
		hide();
		// Stop monitoring when hidden
		if (volumeMonitor) {
			volumeMonitor->stop();
		}
	} else {
		show();
		raise();
		activateWindow();
		// Start monitoring when visible
		if (volumeMonitor) {
			volumeMonitor->start();
		}
	}
}

void AudioMonitorWindow::RefreshSources()
{
	// Clear existing widgets
	for (auto *widget : sourceWidgets.values()) {
		containerLayout->removeWidget(widget);
		delete widget;
	}
	sourceWidgets.clear();

	// Enumerate all sources
	auto enumProc = [](void *data, obs_source_t *source) -> bool {
		auto *window = static_cast<AudioMonitorWindow *>(data);
		if (window->ShouldShowSource(source)) {
			window->AddSource(source);
		}
		return true;
	};
	obs_enum_sources(enumProc, this);
}

bool AudioMonitorWindow::ShouldShowSource(obs_source_t *source)
{
	// Must have audio output capability
	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return false;

	// Must be active
	if (!obs_source_active(source))
		return false;

	// Must not be hidden in mixer
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	bool hidden = obs_data_get_bool(priv, "mixer_hidden");
	if (hidden)
		return false;

	return true;
}

void AudioMonitorWindow::AddSource(obs_source_t *source)
{
	QString uuid = QString::fromUtf8(obs_source_get_uuid(source));

	// Skip if already added
	if (sourceWidgets.contains(uuid))
		return;

	// Get stored color or default dark brown (like KFS)
	QColor color = sourceColors.value(uuid, QColor(0x4E, 0x34, 0x2E));

	// Create the per-source widget
	AudioSourceWidget *widget = new AudioSourceWidget(source, color, this);

	// When user changes color, update our map
	connect(widget, &AudioSourceWidget::colorChanged, this, [this, uuid](const QColor &newColor) {
		sourceColors[uuid] = newColor;
	});

	// Handle source type changes
	connect(widget, &AudioSourceWidget::sourceTypeChanged, this, &AudioMonitorWindow::OnSourceTypeChanged);

	// Restore saved source type
	if (sourceTypes.contains(uuid)) {
		widget->SetSourceType(sourceTypes[uuid]);
	}

	sourceWidgets[uuid] = widget;

	// Add spacer before widget if not the first one
	if (containerLayout->count() > 0) {
		containerLayout->addSpacing(16);
	}
	containerLayout->addWidget(widget);
}

void AudioMonitorWindow::RemoveSource(obs_source_t *source)
{
	QString uuid = QString::fromUtf8(obs_source_get_uuid(source));
	RemoveSourceByUuid(uuid);
}

void AudioMonitorWindow::RemoveSourceByUuid(const QString &uuid)
{
	if (!sourceWidgets.contains(uuid))
		return;

	// Clear mix reference if this was the mix reference source
	if (mixReferenceUuid == uuid) {
		mixReferenceUuid.clear();
	}

	// Clear reference if this was the reference source
	if (referenceUuid == uuid) {
		referenceUuid.clear();
	}

	AudioSourceWidget *widget = sourceWidgets.take(uuid);
	containerLayout->removeWidget(widget);
	delete widget;
}

void AudioMonitorWindow::OnSourceActivate(void *data, calldata_t *cd)
{
	auto *window = static_cast<AudioMonitorWindow *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	// Check if it's an audio source and not hidden
	if (window->ShouldShowSource(source)) {
		// Must invoke on Qt thread
		QMetaObject::invokeMethod(
			window, [window, source]() { window->AddSource(source); }, Qt::QueuedConnection);
	}
}

void AudioMonitorWindow::OnSourceDeactivate(void *data, calldata_t *cd)
{
	auto *window = static_cast<AudioMonitorWindow *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	QString uuid = QString::fromUtf8(obs_source_get_uuid(source));

	QMetaObject::invokeMethod(
		window, [window, uuid]() { window->RemoveSourceByUuid(uuid); }, Qt::QueuedConnection);
}

void AudioMonitorWindow::OnSourceRemove(void *data, calldata_t *cd)
{
	auto *window = static_cast<AudioMonitorWindow *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	QString uuid = QString::fromUtf8(obs_source_get_uuid(source));

	QMetaObject::invokeMethod(
		window, [window, uuid]() { window->RemoveSourceByUuid(uuid); }, Qt::QueuedConnection);
}

void AudioMonitorWindow::OnSourceTypeChanged(AudioSourceWidget *widget, AudioSourceWidget::SourceType newType)
{
	QString uuid = widget->GetSourceUuid();

	// If setting to Mix, clear any previous Mix source (only one allowed)
	if (newType == AudioSourceWidget::SourceType::Mix) {
		if (!mixReferenceUuid.isEmpty() && mixReferenceUuid != uuid) {
			if (sourceWidgets.contains(mixReferenceUuid)) {
				sourceWidgets[mixReferenceUuid]->SetSourceType(AudioSourceWidget::SourceType::Normal);
				sourceTypes[mixReferenceUuid] = AudioSourceWidget::SourceType::Normal;
			}
		}
		mixReferenceUuid = uuid;
	} else if (mixReferenceUuid == uuid) {
		// Clear mix reference if this source is no longer Mix
		mixReferenceUuid.clear();
	}

	// If setting to Reference, clear any previous Reference source (only one allowed)
	if (newType == AudioSourceWidget::SourceType::Reference) {
		if (!referenceUuid.isEmpty() && referenceUuid != uuid) {
			if (sourceWidgets.contains(referenceUuid)) {
				sourceWidgets[referenceUuid]->SetSourceType(AudioSourceWidget::SourceType::Normal);
				sourceTypes[referenceUuid] = AudioSourceWidget::SourceType::Normal;
			}
		}
		referenceUuid = uuid;
	} else if (referenceUuid == uuid) {
		// Clear reference if this source is no longer Reference
		referenceUuid.clear();
	}

	// Update the source type
	sourceTypes[uuid] = newType;
	widget->SetSourceType(newType);
}

void AudioMonitorWindow::SaveLoadColorSettings(obs_data_t *save_data, bool saving)
{
	if (saving) {
		// Create object to hold all colors
		OBSDataAutoRelease colorData = obs_data_create();

		for (auto it = sourceColors.begin(); it != sourceColors.end(); ++it) {
			const QString &uuid = it.key();
			const QColor &color = it.value();
			// Store as ARGB integer
			obs_data_set_int(colorData, uuid.toUtf8().constData(), (int)color.rgba());
		}

		obs_data_set_obj(save_data, "audio_monitor_colors", colorData);

		// Save source types (0=Normal, 1=Mix, 2=Translated, 3=Reference)
		OBSDataAutoRelease typeData = obs_data_create();
		for (auto it = sourceTypes.begin(); it != sourceTypes.end(); ++it) {
			const QString &uuid = it.key();
			int typeValue = static_cast<int>(it.value());
			obs_data_set_int(typeData, uuid.toUtf8().constData(), typeValue);
		}
		obs_data_set_obj(save_data, "audio_monitor_source_types", typeData);
	} else {
		// Load colors
		OBSDataAutoRelease colorData = obs_data_get_obj(save_data, "audio_monitor_colors");
		if (colorData) {
			sourceColors.clear();

			// Iterate through all stored colors
			obs_data_item_t *item = obs_data_first(colorData);
			while (item) {
				const char *uuid = obs_data_item_get_name(item);
				int rgba = (int)obs_data_item_get_int(item);
				sourceColors[QString::fromUtf8(uuid)] = QColor::fromRgba((QRgb)rgba);
				obs_data_item_next(&item);
			}
		}

		// Load source types
		sourceTypes.clear();
		mixReferenceUuid.clear();
		referenceUuid.clear();
		OBSDataAutoRelease typeData = obs_data_get_obj(save_data, "audio_monitor_source_types");
		if (typeData) {
			obs_data_item_t *item = obs_data_first(typeData);
			while (item) {
				const char *uuid = obs_data_item_get_name(item);
				int typeValue = (int)obs_data_item_get_int(item);
				auto type = static_cast<AudioSourceWidget::SourceType>(typeValue);
				sourceTypes[QString::fromUtf8(uuid)] = type;

				// Track the mix reference UUID
				if (type == AudioSourceWidget::SourceType::Mix) {
					mixReferenceUuid = QString::fromUtf8(uuid);
				}
				// Track the reference UUID
				if (type == AudioSourceWidget::SourceType::Reference) {
					referenceUuid = QString::fromUtf8(uuid);
				}
				obs_data_item_next(&item);
			}
		}

		// Refresh widgets with loaded colors and types
		RefreshSources();
	}
}
