/*
 * Audio Monitor Plugin - Main Window
 * Window displaying audio meters for all non-hidden, active audio sources
 */

#include "audio-monitor-window.hpp"
#include "audio-source-widget.hpp"
#include "constants.h"
#include "volume-monitor.hpp"

#include <obs-module.h>

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

	containerWidget = new QWidget();
	containerWidget->setStyleSheet("background-color: #1D1F26;");
	containerLayout = new QHBoxLayout(containerWidget);
	containerLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	containerLayout->setSpacing(12);  // Gap between cards
	containerLayout->setContentsMargins(16, 16, 16, 16);

	scrollArea->setWidget(containerWidget);
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

	// Add widget to layout (spacing handled by containerLayout)
	containerLayout->addWidget(widget);

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

		// Refresh widgets with loaded colors, types, and filter levels
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

	// Width value button (shows width, click to reset to default)
	widthValueBtn = new QPushButton(QString::number(currentCardWidth));
	widthValueBtn->setFixedSize(36, 24);
	widthValueBtn->setStyleSheet(
		"QPushButton { "
		"  color: #FFFFFF; font-size: 11px; font-weight: 500; "
		"  background: transparent; border: 1px solid transparent; "
		"  border-radius: 3px; "
		"} "
		"QPushButton:hover { background-color: rgba(255,255,255,0.1); border-color: #3C404D; }");
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

	// Update value button text
	widthValueBtn->setText(QString::number(currentCardWidth));

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
