/*
 * Audio Monitor Plugin - Main Window
 * Window displaying audio meters for all non-hidden, active audio sources
 */

#include "audio-monitor-window.hpp"
#include "audio-source-widget.hpp"
#include "constants.h"
#include "draggable-card-container.hpp"
#include "volume-monitor.hpp"

#include <obs-module.h>

#include <QCoreApplication>
#include <QIcon>
#include <QLabel>

using namespace AudioMonitorConstants;

AudioMonitorWindow::AudioMonitorWindow(QWidget *parent)
	: QDialog(parent, Qt::Window),
	  currentCardWidth(CARD_WIDTH_DEFAULT)
{
	setWindowTitle(obs_module_text("AudioMonitor.Window.Title"));
	setMinimumSize(500, 350);
	resize(900, 400);

	// Main layout
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	// Create header with title and preset buttons
	CreateHeader();
	mainLayout->addWidget(headerWidget);

	// Create scrollable container for meters (horizontal scrolling)
	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setStyleSheet("QScrollArea { background-color: #1D1F26; border: none; }");

	// Use draggable container for reorderable cards
	draggableContainer = new DraggableCardContainer();
	draggableContainer->setStyleSheet("background-color: #1D1F26;");
	draggableContainer->setSpacing(12);
	draggableContainer->setContentsMargins(16, 16, 16, 16);

	connect(draggableContainer, &DraggableCardContainer::orderChanged,
		this, &AudioMonitorWindow::OnSourceOrderChanged);

	scrollArea->setWidget(draggableContainer);
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
	// 1. FIRST: Disconnect global signal handlers to stop new events from being queued
	// This must happen before any cleanup to prevent race conditions
	DisconnectSignals();

	// 2. Process any already-queued Qt events to clear the queue
	// This ensures no pending RemoveSourceByUuid callbacks execute during cleanup
	QCoreApplication::processEvents();

	// 3. Now safe to stop volume monitor (no more signal callbacks can fire)
	if (volumeMonitor) {
		volumeMonitor->stop();
		// Qt will delete volumeMonitor since it's a child of this window
	}

	// 4. Clear all source widgets via the container (handles deletion)
	if (draggableContainer) {
		draggableContainer->clear();
	}
	sourceWidgets.clear();

	// Qt will then delete remaining child objects (draggableContainer, scrollArea, etc.)
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
	// Clear existing widgets via container
	if (draggableContainer) {
		draggableContainer->clear();
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

	// Apply saved order after adding all sources
	if (!sourceOrder.isEmpty() && draggableContainer) {
		draggableContainer->setOrder(sourceOrder);
	}
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

	// Handle filter level changes
	connect(widget, &AudioSourceWidget::filterLevelChanged, this, &AudioMonitorWindow::OnFilterLevelChanged);

	// Restore saved source type
	if (sourceTypes.contains(uuid)) {
		widget->SetSourceType(sourceTypes[uuid]);
	}

	// Restore saved filter level
	if (filterLevels.contains(uuid)) {
		widget->SetFilterLevel(filterLevels[uuid]);
	}

	// Apply current card width
	widget->SetCardWidth(currentCardWidth);

	sourceWidgets[uuid] = widget;

	// Add widget to draggable container (handles ordering)
	if (draggableContainer) {
		draggableContainer->addWidget(widget, uuid);
	}

	// Update preset button states
	UpdatePresetButtonStates();
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

	sourceWidgets.remove(uuid);

	// Remove from container (returns widget, doesn't delete)
	QWidget *widget = nullptr;
	if (draggableContainer) {
		widget = draggableContainer->removeWidget(uuid);
	}
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
		// Save card width
		obs_data_set_int(save_data, "audio_monitor_card_width", currentCardWidth);

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

		// Save filter levels (0=OFF, 1=LOW, 2=HIGH)
		OBSDataAutoRelease levelData = obs_data_create();
		for (auto it = filterLevels.begin(); it != filterLevels.end(); ++it) {
			const QString &uuid = it.key();
			int level = it.value();
			obs_data_set_int(levelData, uuid.toUtf8().constData(), level);
		}
		obs_data_set_obj(save_data, "audio_monitor_filter_levels", levelData);

		// Save threshold values
		obs_data_set_double(save_data, "audio_monitor_nominal_threshold", nominalThreshold);
		obs_data_set_double(save_data, "audio_monitor_warning_threshold", warningThreshold);
		obs_data_set_double(save_data, "audio_monitor_error_threshold", errorThreshold);

		// Save Mix offset
		obs_data_set_double(save_data, "audio_monitor_mix_offset", mixOffset);

		// Save source order as array
		OBSDataArrayAutoRelease orderArray = obs_data_array_create();
		for (const QString &uuid : sourceOrder) {
			OBSDataAutoRelease item = obs_data_create();
			obs_data_set_string(item, "uuid", uuid.toUtf8().constData());
			obs_data_array_push_back(orderArray, item);
		}
		obs_data_set_array(save_data, "audio_monitor_source_order", orderArray);
	} else {
		// Load card width
		int loadedWidth = (int)obs_data_get_int(save_data, "audio_monitor_card_width");
		if (loadedWidth >= CARD_WIDTH_MIN && loadedWidth <= CARD_WIDTH_MAX) {
			currentCardWidth = loadedWidth;
		} else {
			currentCardWidth = CARD_WIDTH_DEFAULT;
		}
		UpdateWidthControls();

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

		// Load filter levels
		filterLevels.clear();
		OBSDataAutoRelease levelData = obs_data_get_obj(save_data, "audio_monitor_filter_levels");
		if (levelData) {
			obs_data_item_t *item = obs_data_first(levelData);
			while (item) {
				const char *uuid = obs_data_item_get_name(item);
				int level = (int)obs_data_item_get_int(item);
				filterLevels[QString::fromUtf8(uuid)] = level;
				obs_data_item_next(&item);
			}
		}

		// Load threshold values
		double loadedNominal = obs_data_get_double(save_data, "audio_monitor_nominal_threshold");
		double loadedWarning = obs_data_get_double(save_data, "audio_monitor_warning_threshold");
		double loadedError = obs_data_get_double(save_data, "audio_monitor_error_threshold");

		// Validate and apply loaded thresholds (use defaults if not set or invalid)
		if (loadedNominal >= NOMINAL_THRESHOLD_MIN && loadedNominal <= NOMINAL_THRESHOLD_MAX) {
			nominalThreshold = loadedNominal;
		} else {
			nominalThreshold = DEFAULT_NOMINAL_THRESHOLD;
		}
		if (loadedWarning >= WARNING_THRESHOLD_MIN && loadedWarning <= WARNING_THRESHOLD_MAX) {
			warningThreshold = loadedWarning;
		} else {
			warningThreshold = DEFAULT_WARNING_THRESHOLD;
		}
		if (loadedError >= ERROR_THRESHOLD_MIN && loadedError <= ERROR_THRESHOLD_MAX) {
			errorThreshold = loadedError;
		} else {
			errorThreshold = DEFAULT_ERROR_THRESHOLD;
		}

		// Ensure proper ordering with gaps
		if (nominalThreshold > warningThreshold - NOMINAL_WARNING_GAP) {
			nominalThreshold = warningThreshold - NOMINAL_WARNING_GAP;
		}
		if (warningThreshold > errorThreshold - WARNING_ERROR_GAP) {
			warningThreshold = errorThreshold - WARNING_ERROR_GAP;
		}

		// Load Mix offset
		double loadedMixOffset = obs_data_get_double(save_data, "audio_monitor_mix_offset");
		if (loadedMixOffset >= MIX_OFFSET_MIN && loadedMixOffset <= MIX_OFFSET_MAX) {
			mixOffset = loadedMixOffset;
		} else {
			mixOffset = DEFAULT_MIX_OFFSET;
		}

		UpdateThresholdControls();
		UpdateMixOffsetControls();

		// Load source order
		sourceOrder.clear();
		OBSDataArrayAutoRelease orderArray = obs_data_get_array(save_data, "audio_monitor_source_order");
		if (orderArray) {
			size_t count = obs_data_array_count(orderArray);
			for (size_t i = 0; i < count; i++) {
				OBSDataAutoRelease item = obs_data_array_item(orderArray, i);
				const char *uuid = obs_data_get_string(item, "uuid");
				if (uuid && *uuid) {
					sourceOrder.append(QString::fromUtf8(uuid));
				}
			}
		}

		// Refresh widgets with loaded colors, types, filter levels, thresholds, and order
		RefreshSources();
	}
}

void AudioMonitorWindow::CreateHeader()
{
	headerWidget = new QWidget();
	headerWidget->setObjectName("headerWidget");
	// Use ID selector (#headerWidget) to prevent cascade to child widgets
	headerWidget->setStyleSheet(
		"#headerWidget { background-color: #272A33; border-bottom: 1px solid #3C404D; }");
	headerWidget->setFixedHeight(48);

	QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
	headerLayout->setContentsMargins(16, 8, 16, 8);
	headerLayout->setSpacing(10);

	// Left side: Speaker icon + "Audio Monitor" title
	QLabel *speakerIcon = new QLabel();
	speakerIcon->setPixmap(QIcon(":/audio-monitor/images/header-speaker.svg").pixmap(18, 18));
	speakerIcon->setFixedSize(18, 18);

	QLabel *titleLabel = new QLabel(obs_module_text("AudioMonitor.Window.Title"));
	titleLabel->setStyleSheet(
		"color: #FFFFFF; font-size: 14px; font-weight: 600; background: transparent; border: none;");

	headerLayout->addWidget(speakerIcon);
	headerLayout->addWidget(titleLabel);

	// Separator
	headerLayout->addSpacing(20);

	// Card width controls
	QLabel *widthLabel = new QLabel(obs_module_text("AudioMonitor.CardWidth.Label"));
	widthLabel->setStyleSheet("color: #999999; font-size: 11px; background: transparent; border: none;");
	headerLayout->addWidget(widthLabel);

	// Width control button style
	const QString widthBtnStyle =
		"QPushButton { "
		"  background-color: #353942; "
		"  color: #FFFFFF; "
		"  border: 1px solid #3C404D; "
		"  border-radius: 3px; "
		"  font-size: 12px; "
		"  font-weight: bold; "
		"  padding: 2px 8px; "
		"} "
		"QPushButton:hover { background-color: #454952; } "
		"QPushButton:pressed { background-color: #252932; } "
		"QPushButton:disabled { color: #666666; background-color: #2A2D35; }";

	// Decrease width button
	widthDecreaseBtn = new QPushButton("-");
	widthDecreaseBtn->setFixedSize(24, 24);
	widthDecreaseBtn->setStyleSheet(widthBtnStyle);
	widthDecreaseBtn->setToolTip(obs_module_text("AudioMonitor.CardWidth.Decrease.Tooltip"));
	connect(widthDecreaseBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnCardWidthDecrease);
	headerLayout->addWidget(widthDecreaseBtn);

	// Width value button (shows width in px, click to reset to default)
	widthValueBtn = new QPushButton(QString("%1px").arg(currentCardWidth));
	widthValueBtn->setFixedSize(44, 24);
	widthValueBtn->setStyleSheet(
		"QPushButton { "
		"  color: #FFFFFF; font-size: 11px; font-weight: 600; "
		"  background-color: #2A2D35; border: 1px solid #3C404D; "
		"  border-radius: 3px; "
		"  padding: 2px 4px; "
		"} "
		"QPushButton:hover { background-color: #3A3D45; border-color: #4C505D; }");
	widthValueBtn->setToolTip(obs_module_text("AudioMonitor.CardWidth.Reset.Tooltip"));
	connect(widthValueBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnCardWidthReset);
	headerLayout->addWidget(widthValueBtn);

	// Increase width button
	widthIncreaseBtn = new QPushButton("+");
	widthIncreaseBtn->setFixedSize(24, 24);
	widthIncreaseBtn->setStyleSheet(widthBtnStyle);
	widthIncreaseBtn->setToolTip(obs_module_text("AudioMonitor.CardWidth.Increase.Tooltip"));
	connect(widthIncreaseBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnCardWidthIncrease);
	headerLayout->addWidget(widthIncreaseBtn);

	// Initial width control states
	UpdateWidthControls();

	// Separator before threshold controls
	headerLayout->addSpacing(15);

	// Threshold controls (Levels: green, orange, red)
	CreateThresholdControls(headerLayout);

	headerLayout->addStretch();

	// Right side: Preset buttons
	QLabel *presetLabel = new QLabel(obs_module_text("AudioMonitor.Preset.Label"));
	presetLabel->setStyleSheet("color: #999999; font-size: 11px; background: transparent; border: none;");
	headerLayout->addWidget(presetLabel);

	// Teaching preset button
	presetTeaching = new QPushButton(obs_module_text("AudioMonitor.Preset.Teaching"));
	presetTeaching->setIcon(QIcon(":/audio-monitor/images/preset-teaching.svg"));
	presetTeaching->setIconSize(QSize(14, 14));
	presetTeaching->setToolTip(obs_module_text("AudioMonitor.Preset.Teaching.Tooltip"));
	connect(presetTeaching, &QPushButton::clicked, [this]() { ApplyPreset(PresetType::Teaching); });
	headerLayout->addWidget(presetTeaching);

	// Meditation preset button
	presetMeditation = new QPushButton(obs_module_text("AudioMonitor.Preset.Meditation"));
	presetMeditation->setIcon(QIcon(":/audio-monitor/images/preset-meditation.svg"));
	presetMeditation->setIconSize(QSize(14, 14));
	presetMeditation->setToolTip(obs_module_text("AudioMonitor.Preset.Meditation.Tooltip"));
	connect(presetMeditation, &QPushButton::clicked, [this]() { ApplyPreset(PresetType::Meditation); });
	headerLayout->addWidget(presetMeditation);

	// No filtering preset button
	presetNoFilter = new QPushButton(obs_module_text("AudioMonitor.Preset.NoFilter"));
	presetNoFilter->setIcon(QIcon(":/audio-monitor/images/preset-no-filter.svg"));
	presetNoFilter->setIconSize(QSize(14, 14));
	presetNoFilter->setToolTip(obs_module_text("AudioMonitor.Preset.NoFilter.Tooltip"));
	connect(presetNoFilter, &QPushButton::clicked, [this]() { ApplyPreset(PresetType::NoFiltering); });
	headerLayout->addWidget(presetNoFilter);

	// Apply initial button styles
	UpdatePresetButtonStates();
}

void AudioMonitorWindow::ApplyPreset(PresetType preset)
{
	int targetLevel;
	switch (preset) {
	case PresetType::Teaching:
		targetLevel = 2;  // HIGH
		break;
	case PresetType::Meditation:
		targetLevel = 1;  // LOW
		break;
	case PresetType::NoFiltering:
	default:
		targetLevel = 0;  // OFF
		break;
	}

	// Apply to all Translated sources only
	for (auto *widget : sourceWidgets.values()) {
		if (widget->GetSourceType() == AudioSourceWidget::SourceType::Translated) {
			widget->SetFilterLevel(targetLevel);
		}
	}

	UpdatePresetButtonStates();
}

// Static cached stylesheet strings to avoid rebuilding on every call
static const QString &GetPresetBaseStyle()
{
	static const QString style =
		"QPushButton { "
		"  padding: 6px 12px; "
		"  border-radius: 4px; "
		"  font-size: 12px; "
		"  font-weight: 500; "
		"  border: 1px solid; "
		"  background: transparent; "
		"} ";
	return style;
}

static const QString &GetPresetInactiveStyle()
{
	static const QString style = GetPresetBaseStyle() +
		"QPushButton { "
		"  background-color: #272A33; "
		"  color: #B8B8B8; "
		"  border-color: #3C404D; "
		"} "
		"QPushButton:hover { background-color: #353942; }";
	return style;
}

static const QString &GetPresetTeachingActiveStyle()
{
	static const QString style = GetPresetBaseStyle() +
		"QPushButton { "
		"  background-color: #E5AF24; "
		"  color: #000000; "
		"  border-color: #E5AF24; "
		"} "
		"QPushButton:hover { background-color: #D9A520; }";
	return style;
}

static const QString &GetPresetMeditationActiveStyle()
{
	static const QString style = GetPresetBaseStyle() +
		"QPushButton { "
		"  background-color: #37D247; "
		"  color: #000000; "
		"  border-color: #37D247; "
		"} "
		"QPushButton:hover { background-color: #2FC03E; }";
	return style;
}

static const QString &GetPresetNoFilterActiveStyle()
{
	static const QString style = GetPresetBaseStyle() +
		"QPushButton { "
		"  background-color: #B8B8B8; "
		"  color: #000000; "
		"  border-color: #B8B8B8; "
		"} "
		"QPushButton:hover { background-color: #A8A8A8; }";
	return style;
}

void AudioMonitorWindow::UpdatePresetButtonStates()
{
	// Check if all translated sources have the same filter level
	int translatedCount = 0;
	int highCount = 0;
	int lowCount = 0;
	int offCount = 0;

	for (auto *widget : sourceWidgets.values()) {
		if (widget->GetSourceType() == AudioSourceWidget::SourceType::Translated) {
			translatedCount++;
			switch (widget->GetFilterLevel()) {
			case 2:
				highCount++;
				break;
			case 1:
				lowCount++;
				break;
			case 0:
			default:
				offCount++;
				break;
			}
		}
	}

	// Determine active preset
	bool teachingActive = (translatedCount > 0 && highCount == translatedCount);
	bool meditationActive = (translatedCount > 0 && lowCount == translatedCount);
	bool noFilterActive = (translatedCount > 0 && offCount == translatedCount);

	// Use cached stylesheet strings
	presetTeaching->setStyleSheet(teachingActive ? GetPresetTeachingActiveStyle() : GetPresetInactiveStyle());
	presetMeditation->setStyleSheet(meditationActive ? GetPresetMeditationActiveStyle() : GetPresetInactiveStyle());
	presetNoFilter->setStyleSheet(noFilterActive ? GetPresetNoFilterActiveStyle() : GetPresetInactiveStyle());
}

void AudioMonitorWindow::OnFilterLevelChanged(AudioSourceWidget *widget, int newLevel)
{
	QString uuid = widget->GetSourceUuid();
	filterLevels[uuid] = newLevel;
	UpdatePresetButtonStates();
}

void AudioMonitorWindow::OnSourceOrderChanged(const QStringList &newOrder)
{
	sourceOrder = newOrder;
}

// ============================================================================
// Card Width Controls
// ============================================================================

void AudioMonitorWindow::OnCardWidthDecrease()
{
	if (currentCardWidth > CARD_WIDTH_MIN) {
		currentCardWidth -= CARD_WIDTH_STEP;
		if (currentCardWidth < CARD_WIDTH_MIN)
			currentCardWidth = CARD_WIDTH_MIN;
		ApplyCardWidthToAll();
		UpdateWidthControls();
	}
}

void AudioMonitorWindow::OnCardWidthIncrease()
{
	if (currentCardWidth < CARD_WIDTH_MAX) {
		currentCardWidth += CARD_WIDTH_STEP;
		if (currentCardWidth > CARD_WIDTH_MAX)
			currentCardWidth = CARD_WIDTH_MAX;
		ApplyCardWidthToAll();
		UpdateWidthControls();
	}
}

void AudioMonitorWindow::OnCardWidthReset()
{
	if (currentCardWidth != CARD_WIDTH_DEFAULT) {
		currentCardWidth = CARD_WIDTH_DEFAULT;
		ApplyCardWidthToAll();
		UpdateWidthControls();
	}
}

void AudioMonitorWindow::UpdateWidthControls()
{
	if (!widthDecreaseBtn || !widthIncreaseBtn || !widthValueBtn)
		return;

	// Update value button text (with px suffix)
	widthValueBtn->setText(QString("%1px").arg(currentCardWidth));

	// Update button enabled states
	widthDecreaseBtn->setEnabled(currentCardWidth > CARD_WIDTH_MIN);
	widthIncreaseBtn->setEnabled(currentCardWidth < CARD_WIDTH_MAX);
}

void AudioMonitorWindow::ApplyCardWidthToAll()
{
	for (auto *widget : sourceWidgets.values()) {
		widget->SetCardWidth(currentCardWidth);
	}
}

// ============================================================================
// Threshold Controls
// ============================================================================

void AudioMonitorWindow::CreateThresholdControls(QHBoxLayout *headerLayout)
{
	// "Levels" label
	QLabel *levelsLabel = new QLabel(obs_module_text("AudioMonitor.Levels.Label"));
	levelsLabel->setStyleSheet("color: #999999; font-size: 11px; background: transparent; border: none;");
	headerLayout->addWidget(levelsLabel);

	// Shared button styles
	const QString stepperBtnStyle =
		"QPushButton { "
		"  background-color: #353942; "
		"  color: #FFFFFF; "
		"  border: 1px solid #3C404D; "
		"  border-radius: 3px; "
		"  font-size: 11px; "
		"  font-weight: bold; "
		"  padding: 2px 4px; "
		"} "
		"QPushButton:hover { background-color: #454952; } "
		"QPushButton:pressed { background-color: #252932; } "
		"QPushButton:disabled { color: #666666; background-color: #2A2D35; }";

	const QString valueBtnStyle =
		"QPushButton { "
		"  color: #FFFFFF; font-size: 11px; font-weight: 600; "
		"  background-color: #2A2D35; border: 1px solid #3C404D; "
		"  border-radius: 3px; "
		"  padding: 2px 4px; "
		"} "
		"QPushButton:hover { background-color: #3A3D45; border-color: #4C505D; }";

	// Helper lambda to create a color dot label
	auto createColorDot = [](const QColor &color) -> QLabel * {
		QLabel *dot = new QLabel();
		dot->setFixedSize(8, 8);
		dot->setStyleSheet(QString(
			"background-color: %1; "
			"border-radius: 4px; "
			"border: none;"
		).arg(color.name()));
		return dot;
	};

	// Helper lambda to create a subtle vertical separator
	auto createSeparator = []() -> QFrame * {
		QFrame *sep = new QFrame();
		sep->setFrameShape(QFrame::VLine);
		sep->setFixedSize(1, 16);
		sep->setStyleSheet("background-color: #3C404D; border: none;");
		return sep;
	};

	// === Nominal (Green) threshold ===
	QLabel *nominalDot = createColorDot(COLOR_THRESHOLD_NOMINAL);
	headerLayout->addWidget(nominalDot);

	nominalMinusBtn = new QPushButton("-");
	nominalMinusBtn->setFixedSize(20, 20);
	nominalMinusBtn->setStyleSheet(stepperBtnStyle);
	nominalMinusBtn->setToolTip(obs_module_text("AudioMonitor.Levels.Nominal.Decrease.Tooltip"));
	connect(nominalMinusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnNominalDecrease);
	headerLayout->addWidget(nominalMinusBtn);

	nominalValueBtn = new QPushButton(QString::number((int)nominalThreshold));
	nominalValueBtn->setFixedSize(32, 20);
	nominalValueBtn->setStyleSheet(valueBtnStyle);
	nominalValueBtn->setToolTip(QString(obs_module_text("AudioMonitor.Levels.Nominal.Value.Tooltip")).arg((int)DEFAULT_NOMINAL_THRESHOLD));
	connect(nominalValueBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnNominalReset);
	headerLayout->addWidget(nominalValueBtn);

	nominalPlusBtn = new QPushButton("+");
	nominalPlusBtn->setFixedSize(20, 20);
	nominalPlusBtn->setStyleSheet(stepperBtnStyle);
	nominalPlusBtn->setToolTip(obs_module_text("AudioMonitor.Levels.Nominal.Increase.Tooltip"));
	connect(nominalPlusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnNominalIncrease);
	headerLayout->addWidget(nominalPlusBtn);

	headerLayout->addSpacing(4);
	headerLayout->addWidget(createSeparator());
	headerLayout->addSpacing(4);

	// === Warning (Orange) threshold ===
	QLabel *warningDot = createColorDot(COLOR_THRESHOLD_WARNING);
	headerLayout->addWidget(warningDot);

	warningMinusBtn = new QPushButton("-");
	warningMinusBtn->setFixedSize(20, 20);
	warningMinusBtn->setStyleSheet(stepperBtnStyle);
	warningMinusBtn->setToolTip(obs_module_text("AudioMonitor.Levels.Warning.Decrease.Tooltip"));
	connect(warningMinusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnWarningDecrease);
	headerLayout->addWidget(warningMinusBtn);

	warningValueBtn = new QPushButton(QString::number((int)warningThreshold));
	warningValueBtn->setFixedSize(32, 20);
	warningValueBtn->setStyleSheet(valueBtnStyle);
	warningValueBtn->setToolTip(QString(obs_module_text("AudioMonitor.Levels.Warning.Value.Tooltip")).arg((int)DEFAULT_WARNING_THRESHOLD));
	connect(warningValueBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnWarningReset);
	headerLayout->addWidget(warningValueBtn);

	warningPlusBtn = new QPushButton("+");
	warningPlusBtn->setFixedSize(20, 20);
	warningPlusBtn->setStyleSheet(stepperBtnStyle);
	warningPlusBtn->setToolTip(obs_module_text("AudioMonitor.Levels.Warning.Increase.Tooltip"));
	connect(warningPlusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnWarningIncrease);
	headerLayout->addWidget(warningPlusBtn);

	headerLayout->addSpacing(4);
	headerLayout->addWidget(createSeparator());
	headerLayout->addSpacing(4);

	// === Error (Red) threshold ===
	QLabel *errorDot = createColorDot(COLOR_THRESHOLD_ERROR);
	headerLayout->addWidget(errorDot);

	errorMinusBtn = new QPushButton("-");
	errorMinusBtn->setFixedSize(20, 20);
	errorMinusBtn->setStyleSheet(stepperBtnStyle);
	errorMinusBtn->setToolTip(obs_module_text("AudioMonitor.Levels.Error.Decrease.Tooltip"));
	connect(errorMinusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnErrorDecrease);
	headerLayout->addWidget(errorMinusBtn);

	errorValueBtn = new QPushButton(QString::number((int)errorThreshold));
	errorValueBtn->setFixedSize(32, 20);
	errorValueBtn->setStyleSheet(valueBtnStyle);
	errorValueBtn->setToolTip(QString(obs_module_text("AudioMonitor.Levels.Error.Value.Tooltip")).arg((int)DEFAULT_ERROR_THRESHOLD));
	connect(errorValueBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnErrorReset);
	headerLayout->addWidget(errorValueBtn);

	errorPlusBtn = new QPushButton("+");
	errorPlusBtn->setFixedSize(20, 20);
	errorPlusBtn->setStyleSheet(stepperBtnStyle);
	errorPlusBtn->setToolTip(obs_module_text("AudioMonitor.Levels.Error.Increase.Tooltip"));
	connect(errorPlusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnErrorIncrease);
	headerLayout->addWidget(errorPlusBtn);

	// "dB" unit label for thresholds
	QLabel *dbLabel = new QLabel("dB");
	dbLabel->setStyleSheet("color: #999999; font-size: 10px; background: transparent; border: none;");
	headerLayout->addWidget(dbLabel);

	// Separator before Mix offset
	headerLayout->addSpacing(8);
	headerLayout->addWidget(createSeparator());
	headerLayout->addSpacing(8);

	// === Mix Offset controls ===
	QLabel *mixLabel = new QLabel(obs_module_text("AudioMonitor.MixOffset.Label"));
	mixLabel->setStyleSheet("color: #999999; font-size: 11px; background: transparent; border: none;");
	headerLayout->addWidget(mixLabel);

	mixOffsetMinusBtn = new QPushButton("-");
	mixOffsetMinusBtn->setFixedSize(20, 20);
	mixOffsetMinusBtn->setStyleSheet(stepperBtnStyle);
	mixOffsetMinusBtn->setToolTip(obs_module_text("AudioMonitor.MixOffset.Decrease.Tooltip"));
	connect(mixOffsetMinusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnMixOffsetDecrease);
	headerLayout->addWidget(mixOffsetMinusBtn);

	mixOffsetValueBtn = new QPushButton(QString::number((int)mixOffset));
	mixOffsetValueBtn->setFixedSize(36, 20);
	mixOffsetValueBtn->setStyleSheet(valueBtnStyle);
	mixOffsetValueBtn->setToolTip(QString(obs_module_text("AudioMonitor.MixOffset.Value.Tooltip")).arg((int)DEFAULT_MIX_OFFSET));
	connect(mixOffsetValueBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnMixOffsetReset);
	headerLayout->addWidget(mixOffsetValueBtn);

	mixOffsetPlusBtn = new QPushButton("+");
	mixOffsetPlusBtn->setFixedSize(20, 20);
	mixOffsetPlusBtn->setStyleSheet(stepperBtnStyle);
	mixOffsetPlusBtn->setToolTip(obs_module_text("AudioMonitor.MixOffset.Increase.Tooltip"));
	connect(mixOffsetPlusBtn, &QPushButton::clicked, this, &AudioMonitorWindow::OnMixOffsetIncrease);
	headerLayout->addWidget(mixOffsetPlusBtn);

	// "dB" unit label for mix offset
	QLabel *mixDbLabel = new QLabel("dB");
	mixDbLabel->setStyleSheet("color: #999999; font-size: 10px; background: transparent; border: none;");
	headerLayout->addWidget(mixDbLabel);

	// Initial threshold and mix offset control states
	UpdateThresholdControls();
	UpdateMixOffsetControls();
}

void AudioMonitorWindow::UpdateThresholdControls()
{
	if (!nominalValueBtn || !warningValueBtn || !errorValueBtn)
		return;

	// Update value button texts
	nominalValueBtn->setText(QString::number((int)nominalThreshold));
	warningValueBtn->setText(QString::number((int)warningThreshold));
	errorValueBtn->setText(QString::number((int)errorThreshold));

	// Update button enabled states based on constraints
	// Nominal: can decrease if > min, can increase if < warning - gap
	double nominalMax = warningThreshold - NOMINAL_WARNING_GAP;
	nominalMinusBtn->setEnabled(nominalThreshold > NOMINAL_THRESHOLD_MIN);
	nominalPlusBtn->setEnabled(nominalThreshold < nominalMax);

	// Warning: can decrease if > nominal + gap, can increase if < error - gap
	double warningMin = nominalThreshold + NOMINAL_WARNING_GAP;
	double warningMax = errorThreshold - WARNING_ERROR_GAP;
	warningMinusBtn->setEnabled(warningThreshold > warningMin);
	warningPlusBtn->setEnabled(warningThreshold < warningMax);

	// Error: can decrease if > warning + gap, can increase if < max
	double errorMin = warningThreshold + WARNING_ERROR_GAP;
	errorMinusBtn->setEnabled(errorThreshold > errorMin);
	errorPlusBtn->setEnabled(errorThreshold < ERROR_THRESHOLD_MAX);
}

void AudioMonitorWindow::ApplyThresholdsToAll()
{
	for (auto *widget : sourceWidgets.values()) {
		widget->ApplyMeterThresholds();
	}
}

// Nominal threshold handlers
void AudioMonitorWindow::OnNominalDecrease()
{
	if (nominalThreshold > NOMINAL_THRESHOLD_MIN) {
		nominalThreshold -= THRESHOLD_STEP;
		if (nominalThreshold < NOMINAL_THRESHOLD_MIN)
			nominalThreshold = NOMINAL_THRESHOLD_MIN;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnNominalIncrease()
{
	double maxAllowed = warningThreshold - NOMINAL_WARNING_GAP;
	if (nominalThreshold < maxAllowed) {
		nominalThreshold += THRESHOLD_STEP;
		if (nominalThreshold > maxAllowed)
			nominalThreshold = maxAllowed;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnNominalReset()
{
	if (nominalThreshold != DEFAULT_NOMINAL_THRESHOLD) {
		nominalThreshold = DEFAULT_NOMINAL_THRESHOLD;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

// Warning threshold handlers
void AudioMonitorWindow::OnWarningDecrease()
{
	double minAllowed = nominalThreshold + NOMINAL_WARNING_GAP;
	if (warningThreshold > minAllowed) {
		warningThreshold -= THRESHOLD_STEP;
		if (warningThreshold < minAllowed)
			warningThreshold = minAllowed;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnWarningIncrease()
{
	double maxAllowed = errorThreshold - WARNING_ERROR_GAP;
	if (warningThreshold < maxAllowed) {
		warningThreshold += THRESHOLD_STEP;
		if (warningThreshold > maxAllowed)
			warningThreshold = maxAllowed;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnWarningReset()
{
	if (warningThreshold != DEFAULT_WARNING_THRESHOLD) {
		warningThreshold = DEFAULT_WARNING_THRESHOLD;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

// Error threshold handlers
void AudioMonitorWindow::OnErrorDecrease()
{
	double minAllowed = warningThreshold + WARNING_ERROR_GAP;
	if (errorThreshold > minAllowed) {
		errorThreshold -= THRESHOLD_STEP;
		if (errorThreshold < minAllowed)
			errorThreshold = minAllowed;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnErrorIncrease()
{
	if (errorThreshold < ERROR_THRESHOLD_MAX) {
		errorThreshold += THRESHOLD_STEP;
		if (errorThreshold > ERROR_THRESHOLD_MAX)
			errorThreshold = ERROR_THRESHOLD_MAX;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnErrorReset()
{
	if (errorThreshold != DEFAULT_ERROR_THRESHOLD) {
		errorThreshold = DEFAULT_ERROR_THRESHOLD;
		UpdateThresholdControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnMixOffsetDecrease()
{
	if (mixOffset > MIX_OFFSET_MIN) {
		mixOffset -= THRESHOLD_STEP;
		if (mixOffset < MIX_OFFSET_MIN)
			mixOffset = MIX_OFFSET_MIN;
		UpdateMixOffsetControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnMixOffsetIncrease()
{
	if (mixOffset < MIX_OFFSET_MAX) {
		mixOffset += THRESHOLD_STEP;
		if (mixOffset > MIX_OFFSET_MAX)
			mixOffset = MIX_OFFSET_MAX;
		UpdateMixOffsetControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::OnMixOffsetReset()
{
	if (mixOffset != DEFAULT_MIX_OFFSET) {
		mixOffset = DEFAULT_MIX_OFFSET;
		UpdateMixOffsetControls();
		ApplyThresholdsToAll();
	}
}

void AudioMonitorWindow::UpdateMixOffsetControls()
{
	if (!mixOffsetMinusBtn || !mixOffsetPlusBtn || !mixOffsetValueBtn)
		return;

	// Update value button text
	mixOffsetValueBtn->setText(QString::number((int)mixOffset));

	// Update button enabled states
	mixOffsetMinusBtn->setEnabled(mixOffset > MIX_OFFSET_MIN);
	mixOffsetPlusBtn->setEnabled(mixOffset < MIX_OFFSET_MAX);
}
