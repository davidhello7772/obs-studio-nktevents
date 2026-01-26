/*
 * Audio Monitor Plugin - Audio Source Widget
 * Per-source widget with vertical meter and colored background
 */

#include "audio-source-widget.hpp"
#include "audio-monitor-window.hpp"
#include "filter-list-widget.hpp"
#include "volume-meter.hpp"

#include <obs-module.h>

#include <QColorDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QPalette>
#include <QVBoxLayout>

// Warning colors (like KFS)
static const QColor COLOR_WARNING_HIGH(255, 140, 0);   // Bright orange for high volume
static const QColor COLOR_WARNING_LOW(255, 0, 0);      // Red for low volume

AudioSourceWidget::AudioSourceWidget(obs_source_t *src, const QColor &color, QWidget *parent)
	: QFrame(parent),
	  source(src),
	  obs_volmeter(obs_volmeter_create(OBS_FADER_LOG)),
	  currentColor(color),
	  savedColor(color),
	  filterList(nullptr)  // Unused - kept for potential future filter details UI
{
	// Set up the main frame - use stylesheet for border (no native QFrame border)
	setFrameShape(QFrame::NoFrame);
	setAutoFillBackground(true);

	// Main vertical layout - 150px wide card design
	// No side margins so controlsWell can extend edge-to-edge
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 12, 0, 0);
	mainLayout->setSpacing(8);

	// Fixed-height badge container (keeps meter height consistent)
	badgeContainer = new QWidget();
	badgeContainer->setFixedHeight(20);
	badgeContainer->setAttribute(Qt::WA_TranslucentBackground);
	QHBoxLayout *badgeLayout = new QHBoxLayout(badgeContainer);
	badgeLayout->setContentsMargins(10, 0, 10, 0);  // Side margins for content area
	badgeLayout->setSpacing(6);

	// Type badge (MIX, TRANS, REF)
	typeBadge = new QLabel();
	typeBadge->setAlignment(Qt::AlignCenter);
	typeBadge->setFixedHeight(18);
	typeBadge->hide();

	// Warning label (HIGH!, LOW!)
	warningLabel = new QLabel();
	warningLabel->setAlignment(Qt::AlignCenter);
	warningLabel->setFixedHeight(18);
	warningLabel->hide();

	badgeLayout->addStretch();
	badgeLayout->addWidget(typeBadge);
	badgeLayout->addWidget(warningLabel);
	badgeLayout->addStretch();
	mainLayout->addWidget(badgeContainer);

	// Source name label (centered, with word wrap if needed)
	// Fixed height to keep meters aligned across cards (fits 2 lines)
	nameLabel = new QLabel(obs_source_get_name(source));
	nameLabel->setAlignment(Qt::AlignCenter | Qt::AlignTop);
	nameLabel->setWordWrap(true);
	nameLabel->setFixedHeight(32);  // Height for up to 2 lines of 12px text
	nameLabel->setContentsMargins(10, 0, 10, 0);  // Side margins for content area
	nameLabel->setStyleSheet(
		"font-weight: 600; color: white; background: transparent; "
		"font-size: 12px; text-shadow: 0 1px 2px rgba(0,0,0,0.3);");
	mainLayout->addWidget(nameLabel);

	// ============================================================================
	// Meter Well - dark overlay containing volume meter and level display
	// ============================================================================
	// Wrap meterWell in a container with side margins so it doesn't go edge-to-edge
	QWidget *meterWellContainer = new QWidget();
	meterWellContainer->setAttribute(Qt::WA_TranslucentBackground);
	QHBoxLayout *meterContainerLayout = new QHBoxLayout(meterWellContainer);
	meterContainerLayout->setContentsMargins(10, 0, 10, 0);  // Side margins for content area
	meterContainerLayout->setSpacing(0);

	meterWell = new QWidget();
	meterWell->setStyleSheet(
		"QWidget { background-color: rgba(0, 0, 0, 0.35); border-radius: 4px; }");
	QVBoxLayout *meterWellLayout = new QVBoxLayout(meterWell);
	meterWellLayout->setContentsMargins(6, 8, 6, 8);
	meterWellLayout->setSpacing(6);

	// Vertical volume meter
	volMeter = new VolumeMeter(this, obs_volmeter, true);  // true = vertical
	volMeter->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	volMeter->setMinimumWidth(50);
	volMeter->setMinimumHeight(160);

	// Center the meter horizontally
	QHBoxLayout *meterLayout = new QHBoxLayout();
	meterLayout->setContentsMargins(0, 0, 0, 0);
	meterLayout->addStretch();
	meterLayout->addWidget(volMeter);
	meterLayout->addStretch();
	meterWellLayout->addLayout(meterLayout, 1);

	meterContainerLayout->addWidget(meterWell);
	mainLayout->addWidget(meterWellContainer, 1);

	// ============================================================================
	// Controls Well - dark background for contrast (like meter well)
	// ============================================================================
	controlsWell = new QWidget();
	controlsWell->setObjectName("controlsWell");
	controlsWell->setStyleSheet("QWidget#controlsWell { background-color: rgba(0, 0, 0, 0.35); border-radius: 4px; }");
	QVBoxLayout *controlsWellLayout = new QVBoxLayout(controlsWell);
	controlsWellLayout->setContentsMargins(0, 8, 0, 8);
	controlsWellLayout->setSpacing(8);

	// Type selection controls (MIX, TRANS, REF)
	CreateTypeControls(controlsWellLayout);

	// Divider line
	QFrame *divider1 = new QFrame();
	divider1->setFrameShape(QFrame::HLine);
	divider1->setStyleSheet("background-color: rgba(255, 255, 255, 0.1);");
	divider1->setFixedHeight(1);
	QHBoxLayout *divider1Layout = new QHBoxLayout();
	divider1Layout->setContentsMargins(13, 0, 13, 0);
	divider1Layout->addWidget(divider1);
	controlsWellLayout->addLayout(divider1Layout);

	// Filter level controls (OFF/LOW/HIGH)
	CreateFilterLevelControls(controlsWellLayout);

	// Divider line
	QFrame *divider2 = new QFrame();
	divider2->setFrameShape(QFrame::HLine);
	divider2->setStyleSheet("background-color: rgba(255, 255, 255, 0.1);");
	divider2->setFixedHeight(1);
	QHBoxLayout *divider2Layout = new QHBoxLayout();
	divider2Layout->setContentsMargins(13, 0, 13, 0);
	divider2Layout->addWidget(divider2);
	controlsWellLayout->addLayout(divider2Layout);

	// Bottom buttons row: monitor and color picker
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->setContentsMargins(0, 0, 0, 2);
	buttonLayout->setSpacing(6);

	// Monitor output toggle button
	monitorButton = new QPushButton();
	monitorButton->setFixedSize(28, 28);
	monitorButton->setToolTip(obs_module_text("AudioMonitor.Monitor.Tooltip"));
	connect(monitorButton, &QPushButton::clicked, this, &AudioSourceWidget::OnMonitorButtonClicked);

	// Hide if monitoring not available on this system
	if (!IsMonitoringAvailable()) {
		monitorButton->hide();
	} else {
		currentMonitoringType = obs_source_get_monitoring_type(source);
		UpdateMonitorButtonStyle();
	}

	// Color picker button
	colorButton = new QPushButton();
	colorButton->setFixedSize(28, 28);
	colorButton->setToolTip(obs_module_text("AudioMonitor.ColorButton.Tooltip"));
	colorButton->setIcon(QIcon(":/audio-monitor/images/color-picker.svg"));
	colorButton->setIconSize(QSize(16, 16));
	colorButton->setStyleSheet(
		"QPushButton { "
		"  background-color: transparent; "
		"  border: 1px solid transparent; "
		"  border-radius: 4px; "
		"} "
		"QPushButton:hover { background-color: rgba(71, 107, 215, 0.3); }");
	connect(colorButton, &QPushButton::clicked, this, &AudioSourceWidget::OnColorButtonClicked);

	buttonLayout->addStretch();
	buttonLayout->addWidget(monitorButton);
	buttonLayout->addWidget(colorButton);
	buttonLayout->addStretch();
	controlsWellLayout->addLayout(buttonLayout);

	mainLayout->addWidget(controlsWell);

	setLayout(mainLayout);

	// Set size for the widget - 155px wide card design
	setMinimumWidth(155);
	setFixedWidth(155);
	setMinimumHeight(340);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

	// Apply the background color
	SetBackgroundColor(currentColor);

	// Apply default meter thresholds
	ApplyMeterThresholds();

	// Attach volmeter to source
	obs_volmeter_attach_source(obs_volmeter, source);

	// Add volume level callback
	obs_volmeter_add_callback(obs_volmeter, OBSVolumeLevel, this);

	// Listen for source rename and filter changes
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	sigs.emplace_back(sh, "rename", OBSSourceRenamed, this);
	sigs.emplace_back(sh, "filter_add", OBSFilterChanged, this);
	sigs.emplace_back(sh, "filter_remove", OBSFilterChanged, this);

	// Listen for audio monitoring changes (if available)
	if (IsMonitoringAvailable()) {
		sigs.emplace_back(sh, "audio_monitoring", OBSMonitoringTypeChanged, this);
	}
}

AudioSourceWidget::~AudioSourceWidget()
{
	// Stop blink timer (Qt will delete it as child object, but stop it explicitly)
	if (blinkTimer) {
		blinkTimer->stop();
		// Don't delete - Qt will handle it since parent is this
	}

	// Remove callback BEFORE RAII destroys volmeter
	obs_volmeter_remove_callback(obs_volmeter, OBSVolumeLevel, this);

	// Clear signal connections explicitly
	sigs.clear();

	// Clear volume history
	volumeHistory.clear();
}

void AudioSourceWidget::OBSVolumeLevel(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				       const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	AudioSourceWidget *widget = static_cast<AudioSourceWidget *>(data);
	widget->volMeter->setLevels(magnitude, peak, inputPeak);

	// Track PEAK for volume history (use max of all channels)
	// Peak values match what the meter displays visually
	float maxPeak = -60.0f;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (peak[i] > maxPeak)
			maxPeak = peak[i];
	}
	widget->lastMagnitudeDb = maxPeak;

	// Add to volume history (like KFS: average over recent samples)
	widget->volumeHistory.push_back(maxPeak);
	if (widget->volumeHistory.size() > VOLUME_HISTORY_SIZE) {
		widget->volumeHistory.pop_front();
	}

	Q_UNUSED(inputPeak);
}

double AudioSourceWidget::GetAverageDb() const
{
	if (volumeHistory.empty())
		return -60.0;

	double sum = 0.0;
	for (double val : volumeHistory) {
		sum += val;
	}
	return sum / volumeHistory.size();
}

void AudioSourceWidget::OBSSourceRenamed(void *data, calldata_t *cd)
{
	AudioSourceWidget *widget = static_cast<AudioSourceWidget *>(data);
	const char *newName = calldata_string(cd, "new_name");

	// Update on Qt thread
	QMetaObject::invokeMethod(
		widget->nameLabel, [widget, name = QString::fromUtf8(newName)]() { widget->nameLabel->setText(name); },
		Qt::QueuedConnection);
}

void AudioSourceWidget::OnColorButtonClicked()
{
	QColorDialog::ColorDialogOptions options;
#ifdef __linux__
	options |= QColorDialog::DontUseNativeDialog;
#endif

	QColor newColor =
		QColorDialog::getColor(savedColor, this, obs_module_text("AudioMonitor.ColorDialog.Title"), options);

	if (newColor.isValid() && newColor != savedColor) {
		savedColor = newColor;
		if (warningState == WarningState::None) {
			currentColor = newColor;
			UpdateBackgroundStyle();
		}
		emit colorChanged(newColor);
	}
}

void AudioSourceWidget::SetBackgroundColor(const QColor &color)
{
	savedColor = color;
	if (warningState == WarningState::None) {
		currentColor = color;
		UpdateBackgroundStyle();
	}
}

void AudioSourceWidget::UpdateBackgroundStyle()
{
	// Use QPalette for reliable background color changes
	QPalette pal = palette();
	pal.setColor(QPalette::Window, currentColor);
	setPalette(pal);

	// Use padding instead of border so background color fills to the edge
	QString frameStyle = QString(
		"AudioSourceWidget { "
		"  background-color: %1; "
		"  border: none; "
		"  border-radius: 8px; "
		"}"
	).arg(currentColor.name());
	setStyleSheet(frameStyle);

	// Update color button to show the saved (user-chosen) color (no border)
	QString buttonStyle = QString(
		"QPushButton { background-color: %1; border: none; border-radius: 4px; } "
		"QPushButton:hover { border: 1px solid rgba(255, 255, 255, 0.3); }"
	).arg(savedColor.name());
	colorButton->setStyleSheet(buttonStyle);

	// Keep controlsWell dark background for contrast (like meter well)
	controlsWell->setStyleSheet("QWidget#controlsWell { background-color: rgba(0, 0, 0, 0.35); border-radius: 4px; }");

	// Re-apply filter button styles (parent stylesheet can affect children)
	UpdateFilterLevelDisplay();

	// Adjust text color based on background brightness
	int brightness = (currentColor.red() * 299 + currentColor.green() * 587 + currentColor.blue() * 114) / 1000;
	QString textColor = (brightness > 128) ? "black" : "white";
	nameLabel->setStyleSheet(QString("font-weight: bold; color: %1; background: transparent;").arg(textColor));

	// Force repaint
	update();
}

QString AudioSourceWidget::GetSourceUuid() const
{
	return QString::fromUtf8(obs_source_get_uuid(source));
}

QString AudioSourceWidget::GetSourceName() const
{
	return QString::fromUtf8(obs_source_get_name(source));
}

void AudioSourceWidget::SetCardWidth(int width)
{
	setMinimumWidth(width);
	setFixedWidth(width);
}

void AudioSourceWidget::OnTypeMixClicked()
{
	// Toggle Mix type (if already Mix, go to Normal)
	SourceType newType = (sourceType == SourceType::Mix) ? SourceType::Normal : SourceType::Mix;
	emit sourceTypeChanged(this, newType);
}

void AudioSourceWidget::OnTypeTransClicked()
{
	// Toggle Translated type (if already Translated, go to Normal)
	SourceType newType = (sourceType == SourceType::Translated) ? SourceType::Normal : SourceType::Translated;
	emit sourceTypeChanged(this, newType);
}

void AudioSourceWidget::OnTypeRefClicked()
{
	// Toggle Reference type (if already Reference, go to Normal)
	SourceType newType = (sourceType == SourceType::Reference) ? SourceType::Normal : SourceType::Reference;
	emit sourceTypeChanged(this, newType);
}

void AudioSourceWidget::SetSourceType(SourceType type)
{
	if (sourceType == type)
		return;

	sourceType = type;
	UpdateTypeButtonsStyle();
	UpdateTypeBadge();
	ApplyMeterThresholds();

	// Clear any warning when changing type
	if (warningState != WarningState::None) {
		SetWarningState(WarningState::None);
	}
}

void AudioSourceWidget::UpdateTypeBadge()
{
	switch (sourceType) {
	case SourceType::Mix:
		typeBadge->setText(obs_module_text("AudioMonitor.Badge.Mix"));
		typeBadge->setStyleSheet(
			"background-color: #FF6B35; color: white; font-weight: bold; "
			"font-size: 9px; padding: 2px 4px; border-radius: 3px;");
		typeBadge->show();
		break;
	case SourceType::Translated:
		typeBadge->setText(obs_module_text("AudioMonitor.Badge.Translated"));
		typeBadge->setStyleSheet(
			"background-color: #4A90D9; color: white; font-weight: bold; "
			"font-size: 9px; padding: 2px 4px; border-radius: 3px;");
		typeBadge->show();
		break;
	case SourceType::Reference:
		typeBadge->setText(obs_module_text("AudioMonitor.Badge.Reference"));
		typeBadge->setStyleSheet(
			"background-color: #9B59B6; color: white; font-weight: bold; "
			"font-size: 9px; padding: 2px 4px; border-radius: 3px;");
		typeBadge->show();
		break;
	case SourceType::Normal:
	default:
		typeBadge->hide();
		break;
	}
}

void AudioSourceWidget::UpdateTypeButtonsStyle()
{
	// Style for inactive (unselected) buttons
	QString inactiveStyle =
		"QPushButton { "
		"  background-color: rgba(255, 255, 255, 0.08); "
		"  border: 1px solid rgba(255, 255, 255, 0.12); "
		"  border-radius: 3px; "
		"  color: rgba(255, 255, 255, 0.5); "
		"  font-size: 9px; "
		"  font-weight: 600; "
		"  padding: 2px 6px; "
		"} "
		"QPushButton:hover { "
		"  background-color: rgba(255, 255, 255, 0.15); "
		"  color: rgba(255, 255, 255, 0.8); "
		"}";

	// Active styles with specific colors
	QString activeMixStyle =
		"QPushButton { "
		"  background-color: rgba(255, 107, 53, 0.3); "
		"  border: 1px solid #FF6B35; "
		"  border-radius: 3px; "
		"  color: #FF6B35; "
		"  font-size: 9px; "
		"  font-weight: 600; "
		"  padding: 2px 6px; "
		"} "
		"QPushButton:hover { "
		"  background-color: rgba(255, 107, 53, 0.4); "
		"}";

	QString activeTransStyle =
		"QPushButton { "
		"  background-color: rgba(74, 144, 217, 0.3); "
		"  border: 1px solid #4A90D9; "
		"  border-radius: 3px; "
		"  color: #4A90D9; "
		"  font-size: 9px; "
		"  font-weight: 600; "
		"  padding: 2px 6px; "
		"} "
		"QPushButton:hover { "
		"  background-color: rgba(74, 144, 217, 0.4); "
		"}";

	QString activeRefStyle =
		"QPushButton { "
		"  background-color: rgba(155, 89, 182, 0.3); "
		"  border: 1px solid #9B59B6; "
		"  border-radius: 3px; "
		"  color: #9B59B6; "
		"  font-size: 9px; "
		"  font-weight: 600; "
		"  padding: 2px 6px; "
		"} "
		"QPushButton:hover { "
		"  background-color: rgba(155, 89, 182, 0.4); "
		"}";

	// Apply styles based on current type
	typeMixBtn->setStyleSheet(sourceType == SourceType::Mix ? activeMixStyle : inactiveStyle);
	typeTransBtn->setStyleSheet(sourceType == SourceType::Translated ? activeTransStyle : inactiveStyle);
	typeRefBtn->setStyleSheet(sourceType == SourceType::Reference ? activeRefStyle : inactiveStyle);
}

void AudioSourceWidget::SetWarningState(WarningState state)
{
	if (warningState == state)
		return;

	warningState = state;
	UpdateWarningDisplay();
}

void AudioSourceWidget::UpdateWarningDisplay()
{
	if (warningState == WarningState::None) {
		// Clear warning - restore original color
		warningLabel->hide();
		currentColor = savedColor;
		UpdateBackgroundStyle();

		// Stop blink timer
		if (blinkTimer) {
			blinkTimer->stop();
		}
		blinkVisible = true;
	} else {
		// Show warning
		QString message;
		QColor warningColor;

		if (warningState == WarningState::HighVolume) {
			message = obs_module_text("AudioMonitor.Warning.High");
			warningColor = COLOR_WARNING_HIGH;
		} else {
			message = obs_module_text("AudioMonitor.Warning.Low");
			warningColor = COLOR_WARNING_LOW;
		}

		warningLabel->setText(message);
		warningLabel->setStyleSheet(
			QString("background-color: %1; color: white; font-weight: bold; "
				"font-size: 9px; padding: 2px 4px; border-radius: 3px;")
				.arg(warningColor.name()));
		warningLabel->show();

		// Change background color
		currentColor = warningColor.darker(150);  // Darker version for background
		UpdateBackgroundStyle();

		// Start blink timer for pulsing effect
		if (!blinkTimer) {
			blinkTimer = new QTimer(this);
			connect(blinkTimer, &QTimer::timeout, this, &AudioSourceWidget::OnBlinkTimer);
		}
		blinkTimer->start(400);  // 0.8s cycle (400ms on, 400ms dim)
		blinkVisible = true;
	}
}

void AudioSourceWidget::OnBlinkTimer()
{
	blinkVisible = !blinkVisible;

	// Pulse the warning label opacity
	if (warningState != WarningState::None) {
		warningLabel->setVisible(blinkVisible);
	}
}

void AudioSourceWidget::ApplyMeterThresholds()
{
	// Get configurable thresholds from parent AudioMonitorWindow
	// Navigate the widget hierarchy: this -> containerWidget -> scrollArea viewport -> AudioMonitorWindow
	AudioMonitorWindow *window = nullptr;
	QWidget *p = parentWidget();
	while (p) {
		window = qobject_cast<AudioMonitorWindow *>(p);
		if (window)
			break;
		p = p->parentWidget();
	}

	// Get threshold values (use defaults if window not found)
	double nominal = window ? window->getNominalThreshold() : AudioMonitorWindow::DEFAULT_NOMINAL_THRESHOLD;
	double warning = window ? window->getWarningThreshold() : AudioMonitorWindow::DEFAULT_WARNING_THRESHOLD;
	double error = window ? window->getErrorThreshold() : AudioMonitorWindow::DEFAULT_ERROR_THRESHOLD;
	double mixOffset = window ? window->getMixOffset() : AudioMonitorWindow::DEFAULT_MIX_OFFSET;

	// Blue zone only shown for Mix and Translated sources
	switch (sourceType) {
	case SourceType::Mix:
		// Mix reference uses thresholds shifted by mixOffset
		// This creates a quieter "target zone" for mix channels
		volMeter->setNominalLevel(nominal + mixOffset);
		volMeter->setWarningLevel(warning + mixOffset);
		volMeter->setErrorLevel(error + mixOffset);
		break;
	case SourceType::Translated:
		// Translated channels - show blue zone with configurable thresholds
		volMeter->setNominalLevel(nominal);
		volMeter->setWarningLevel(warning);
		volMeter->setErrorLevel(error);
		break;
	case SourceType::Normal:
	case SourceType::Reference:
	default:
		// Normal and Reference sources - NO blue zone (nominalLevel = minimumLevel)
		// Green zone starts at minimum, uses configurable warning/error thresholds
		volMeter->setNominalLevel(-60.0);  // Same as minimumLevel = no blue zone
		volMeter->setWarningLevel(warning);
		volMeter->setErrorLevel(error);
		break;
	}
}

// ============================================================================
// RNNoise Filter Integration
// ============================================================================

bool AudioSourceWidget::IsRnnoisePluginAvailable()
{
	// Cache the result since it won't change during runtime
	static bool checked = false;
	static bool available = false;

	if (!checked) {
		checked = true;
		// Check if the filter type is registered by trying to get its info
		available = obs_source_get_display_name(RNNOISE_FILTER_ID) != nullptr;
	}

	return available;
}

int AudioSourceWidget::CountRnnoiseFilters() const
{
	struct CountData {
		const char *filterId;
		int count;
	} data = {RNNOISE_FILTER_ID, 0};

	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *param) {
		CountData *d = static_cast<CountData *>(param);
		if (strcmp(obs_source_get_id(filter), d->filterId) == 0) {
			d->count++;
		}
	}, &data);

	return data.count;
}

void AudioSourceWidget::OBSFilterChanged(void *data, calldata_t *)
{
	// Filter change callback - update filter level display
	AudioSourceWidget *widget = static_cast<AudioSourceWidget *>(data);

	// Update on Qt thread
	QMetaObject::invokeMethod(
		widget,
		[widget]() {
			// Sync filter level with actual RNNoise filter count
			int count = widget->CountRnnoiseFilters();
			if (count != widget->filterLevel) {
				widget->filterLevel = qBound(0, count, MAX_RNNOISE_FILTERS);
				widget->UpdateFilterLevelDisplay();
			}
		},
		Qt::QueuedConnection);
}

// ============================================================================
// Filter Level Management (RNNoise)
// ============================================================================

QString AudioSourceWidget::GenerateRnnoiseFilterName(int index) const
{
	return QString("RNNoise #%1").arg(index + 1);
}

void AudioSourceWidget::AddRnnoiseFilter()
{
	if (!IsRnnoisePluginAvailable())
		return;

	int currentCount = CountRnnoiseFilters();
	if (currentCount >= MAX_RNNOISE_FILTERS)
		return;

	QString filterName = GenerateRnnoiseFilterName(currentCount);

	// Create settings with 100% strength
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_double(settings, "strength", 100.0);

	// Create filter
	obs_source_t *filter = obs_source_create(RNNOISE_FILTER_ID, filterName.toUtf8().constData(), settings, nullptr);

	if (filter) {
		// Add to source (source takes ownership)
		obs_source_filter_add(source, filter);
		// Release our reference
		obs_source_release(filter);
	}
}

void AudioSourceWidget::RemoveRnnoiseFilter()
{
	if (!source)
		return;

	// Find the last RNNoise filter
	struct RemoveData {
		obs_source_t *parent;
		obs_source_t *lastFilter;
		const char *filterId;
	} data = {source, nullptr, RNNOISE_FILTER_ID};

	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			RemoveData *d = static_cast<RemoveData *>(param);
			if (strcmp(obs_source_get_id(filter), d->filterId) == 0) {
				d->lastFilter = filter;  // Keep track of last one found
			}
		},
		&data);

	if (data.lastFilter) {
		obs_source_filter_remove(source, data.lastFilter);
	}
}

void AudioSourceWidget::RemoveAllRnnoiseFilters()
{
	if (!source)
		return;

	// Collect all RNNoise filters first (can't modify during enumeration)
	struct CollectData {
		std::vector<obs_source_t *> filters;
		const char *filterId;
	} data;
	data.filterId = RNNOISE_FILTER_ID;

	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			CollectData *d = static_cast<CollectData *>(param);
			if (strcmp(obs_source_get_id(filter), d->filterId) == 0) {
				// Add reference for our collection
				obs_source_get_ref(filter);
				d->filters.push_back(filter);
			}
		},
		&data);

	// Now remove them
	for (obs_source_t *filter : data.filters) {
		obs_source_filter_remove(source, filter);
		obs_source_release(filter);
	}
}

void AudioSourceWidget::ApplyFilterLevel(int targetLevel)
{
	if (!source)
		return;

	if (!IsRnnoisePluginAvailable())
		return;

	int currentCount = CountRnnoiseFilters();
	int targetCount = qBound(0, targetLevel, MAX_RNNOISE_FILTERS);

	// Remove filters if we have too many
	while (currentCount > targetCount) {
		RemoveRnnoiseFilter();
		currentCount--;
	}

	// Add filters if we need more
	while (currentCount < targetCount) {
		AddRnnoiseFilter();
		currentCount++;
	}
}

void AudioSourceWidget::SetFilterLevel(int level)
{
	level = qBound(0, level, MAX_RNNOISE_FILTERS);
	if (filterLevel == level)
		return;

	filterLevel = level;
	ApplyFilterLevel(level);
	UpdateFilterLevelDisplay();
	emit filterLevelChanged(this, level);
}

void AudioSourceWidget::OnFilterLevelUp()
{
	SetFilterLevel(filterLevel + 1);
}

void AudioSourceWidget::OnFilterLevelDown()
{
	SetFilterLevel(filterLevel - 1);
}

void AudioSourceWidget::UpdateFilterLevelDisplay()
{
	if (!filterMinusBtn || !filterPlusBtn || !filterLevelLabel || !filterDot1 || !filterDot2)
		return;

	// Update button enabled states
	filterMinusBtn->setEnabled(filterLevel > 0);
	filterPlusBtn->setEnabled(filterLevel < MAX_RNNOISE_FILTERS);

	// Update label and colors
	QString labelText;
	QString labelColor;
	QString dot1Color;
	QString dot2Color;
	QString dot1Shadow;
	QString dot2Shadow;

	switch (filterLevel) {
	case 0:  // OFF
		labelText = obs_module_text("AudioMonitor.FilterLevel.Off");
		labelColor = "#B8B8B8";  // Muted
		dot1Color = "rgba(255, 255, 255, 0.2)";
		dot2Color = "rgba(255, 255, 255, 0.2)";
		dot1Shadow = "none";
		dot2Shadow = "none";
		break;
	case 1:  // LOW
		labelText = obs_module_text("AudioMonitor.FilterLevel.Low");
		labelColor = "#37D247";  // Green (success)
		dot1Color = "#37D247";
		dot2Color = "rgba(255, 255, 255, 0.2)";
		dot1Shadow = "0 0 4px rgba(55, 210, 71, 0.6)";
		dot2Shadow = "none";
		break;
	case 2:  // HIGH
		labelText = obs_module_text("AudioMonitor.FilterLevel.High");
		labelColor = "#E5AF24";  // Orange (warning)
		dot1Color = "#E5AF24";
		dot2Color = "#E5AF24";
		dot1Shadow = "0 0 4px rgba(229, 175, 36, 0.6)";
		dot2Shadow = "0 0 4px rgba(229, 175, 36, 0.6)";
		break;
	}

	filterLevelLabel->setText(labelText);
	filterLevelLabel->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: 600; letter-spacing: 0.5px; background: transparent;").arg(labelColor));

	filterDot1->setStyleSheet(QString("background-color: %1; border-radius: 3px;").arg(dot1Color));
	filterDot2->setStyleSheet(QString("background-color: %1; border-radius: 3px;").arg(dot2Color));

	// Calculate button text color based on blended background
	// The controlsWell has 35% black overlay on card color
	// Blended = card * 0.65 (approximately)
	int blendedR = static_cast<int>(currentColor.red() * 0.65);
	int blendedG = static_cast<int>(currentColor.green() * 0.65);
	int blendedB = static_cast<int>(currentColor.blue() * 0.65);

	// Calculate luminance of blended background
	int blendedLuminance = (blendedR * 299 + blendedG * 587 + blendedB * 114) / 1000;

	// Get card's hue for tinted contrast color
	int h, s, l;
	currentColor.getHsl(&h, &s, &l);

	// Enabled: light tint with card's hue for visual harmony
	// Use high lightness (210-220) to ensure contrast against darkened well
	QColor enabledTextColor;
	if (blendedLuminance < 128) {
		// Dark background: use light tinted color
		enabledTextColor = QColor::fromHsl(h, qMin(s, 60), 210);
	} else {
		// Light background (rare with 35% overlay): use darker tint
		enabledTextColor = QColor::fromHsl(h, qMin(s, 80), 40);
	}

	// Disabled: muted version - same hue but lower saturation and closer to gray
	QColor disabledTextColor = QColor::fromHsl(h, 20, blendedLuminance < 128 ? 100 : 80);

	bool minusEnabled = filterLevel > 0;
	bool plusEnabled = filterLevel < MAX_RNNOISE_FILTERS;

	// Build stylesheets with calculated colors
	// Use subtle borders that blend with the dark well background
	QString enabledBtnStyle = QString(
		"QPushButton { "
		"  background-color: rgba(255, 255, 255, 0.1); "
		"  border: 1px solid rgba(255, 255, 255, 0.15); "
		"  border-radius: 4px; "
		"  color: %1; "
		"  font-size: 16px; "
		"  font-weight: bold; "
		"  padding: 0px; "
		"  padding-bottom: 2px; "
		"  margin: 0px; "
		"} "
		"QPushButton:hover { "
		"  background-color: rgba(255, 255, 255, 0.2); "
		"}").arg(enabledTextColor.name());

	QString disabledBtnStyle = QString(
		"QPushButton { "
		"  background-color: rgba(128, 128, 128, 0.08); "
		"  border: 1px solid rgba(128, 128, 128, 0.12); "
		"  border-radius: 4px; "
		"  color: %1; "
		"  font-size: 16px; "
		"  font-weight: bold; "
		"  padding: 0px; "
		"  padding-bottom: 2px; "
		"  margin: 0px; "
		"}").arg(disabledTextColor.name());

	filterMinusBtn->setStyleSheet(minusEnabled ? enabledBtnStyle : disabledBtnStyle);
	filterMinusBtn->setEnabled(minusEnabled);

	filterPlusBtn->setStyleSheet(plusEnabled ? enabledBtnStyle : disabledBtnStyle);
	filterPlusBtn->setEnabled(plusEnabled);
}

void AudioSourceWidget::CreateTypeControls(QVBoxLayout *parentLayout)
{
	// Container for type selection controls
	typeControlsContainer = new QWidget();
	typeControlsContainer->setAttribute(Qt::WA_TranslucentBackground);
	QVBoxLayout *containerLayout = new QVBoxLayout(typeControlsContainer);
	containerLayout->setContentsMargins(10, 0, 10, 0);
	containerLayout->setSpacing(4);

	// Label row: "TYPE" text, centered
	QLabel *textLabel = new QLabel(obs_module_text("AudioMonitor.Type.Label"));
	textLabel->setAlignment(Qt::AlignCenter);
	textLabel->setStyleSheet("color: #B8B8B8; font-size: 9px; font-weight: 500; letter-spacing: 0.5px; background: transparent;");
	containerLayout->addWidget(textLabel);

	// Control row: [MIX] [TRANS] [REF]
	QWidget *controlRow = new QWidget();
	controlRow->setAttribute(Qt::WA_TranslucentBackground);
	QHBoxLayout *controlLayout = new QHBoxLayout(controlRow);
	controlLayout->setContentsMargins(0, 0, 0, 0);
	controlLayout->setSpacing(4);

	// MIX button
	typeMixBtn = new QPushButton(obs_module_text("AudioMonitor.Badge.Mix"));
	typeMixBtn->setObjectName("typeMixBtn");
	typeMixBtn->setToolTip(obs_module_text("AudioMonitor.Type.Mix.Tooltip"));
	connect(typeMixBtn, &QPushButton::clicked, this, &AudioSourceWidget::OnTypeMixClicked);

	// TRANS button
	typeTransBtn = new QPushButton(obs_module_text("AudioMonitor.Badge.Translated"));
	typeTransBtn->setObjectName("typeTransBtn");
	typeTransBtn->setToolTip(obs_module_text("AudioMonitor.Type.Translated.Tooltip"));
	connect(typeTransBtn, &QPushButton::clicked, this, &AudioSourceWidget::OnTypeTransClicked);

	// REF button
	typeRefBtn = new QPushButton(obs_module_text("AudioMonitor.Badge.Reference"));
	typeRefBtn->setObjectName("typeRefBtn");
	typeRefBtn->setToolTip(obs_module_text("AudioMonitor.Type.Reference.Tooltip"));
	connect(typeRefBtn, &QPushButton::clicked, this, &AudioSourceWidget::OnTypeRefClicked);

	controlLayout->addWidget(typeMixBtn);
	controlLayout->addWidget(typeTransBtn);
	controlLayout->addWidget(typeRefBtn);

	containerLayout->addWidget(controlRow);

	parentLayout->addWidget(typeControlsContainer);

	// Initialize button styles
	UpdateTypeButtonsStyle();
}

void AudioSourceWidget::CreateFilterLevelControls(QVBoxLayout *parentLayout)
{
	// Container for filter level controls (transparent)
	filterLevelContainer = new QWidget();
	filterLevelContainer->setAttribute(Qt::WA_TranslucentBackground);
	QVBoxLayout *containerLayout = new QVBoxLayout(filterLevelContainer);
	containerLayout->setContentsMargins(10, 0, 10, 0);
	containerLayout->setSpacing(4);

	// Label row: just "FILTER" text, centered
	QLabel *textLabel = new QLabel(obs_module_text("AudioMonitor.FilterLevel.Label"));
	textLabel->setAlignment(Qt::AlignCenter);
	textLabel->setStyleSheet("color: #B8B8B8; font-size: 9px; font-weight: 500; letter-spacing: 0.5px; background: transparent;");
	containerLayout->addWidget(textLabel);

	// Control row: [-] [indicator] [+]
	QWidget *controlRow = new QWidget();
	controlRow->setAttribute(Qt::WA_TranslucentBackground);
	QHBoxLayout *controlLayout = new QHBoxLayout(controlRow);
	controlLayout->setContentsMargins(0, 0, 0, 0);
	controlLayout->setSpacing(4);

	// Minus button - text with dynamic color
	filterMinusBtn = new QPushButton("-");
	filterMinusBtn->setObjectName("filterMinusBtn");
	filterMinusBtn->setFixedSize(28, 28);
	filterMinusBtn->setToolTip(obs_module_text("AudioMonitor.FilterLevel.Decrease.Tooltip"));
	connect(filterMinusBtn, &QPushButton::clicked, this, &AudioSourceWidget::OnFilterLevelDown);

	// Indicator widget (dots + label) - transparent background
	filterIndicator = new QWidget();
	filterIndicator->setAttribute(Qt::WA_TranslucentBackground);
	QHBoxLayout *indicatorLayout = new QHBoxLayout(filterIndicator);
	indicatorLayout->setContentsMargins(0, 0, 0, 0);
	indicatorLayout->setSpacing(3);

	filterDot1 = new QWidget();
	filterDot1->setFixedSize(6, 6);
	filterDot1->setStyleSheet("background-color: rgba(255, 255, 255, 0.2); border-radius: 3px;");

	filterDot2 = new QWidget();
	filterDot2->setFixedSize(6, 6);
	filterDot2->setStyleSheet("background-color: rgba(255, 255, 255, 0.2); border-radius: 3px;");

	filterLevelLabel = new QLabel(obs_module_text("AudioMonitor.FilterLevel.Off"));
	filterLevelLabel->setStyleSheet("color: #B8B8B8; font-size: 8px; font-weight: 600; letter-spacing: 0.5px; background: transparent;");

	indicatorLayout->addStretch();
	indicatorLayout->addWidget(filterDot1);
	indicatorLayout->addWidget(filterDot2);
	indicatorLayout->addWidget(filterLevelLabel);
	indicatorLayout->addStretch();

	// Plus button - text with dynamic color
	filterPlusBtn = new QPushButton("+");
	filterPlusBtn->setObjectName("filterPlusBtn");
	filterPlusBtn->setFixedSize(28, 28);
	filterPlusBtn->setToolTip(obs_module_text("AudioMonitor.FilterLevel.Increase.Tooltip"));
	connect(filterPlusBtn, &QPushButton::clicked, this, &AudioSourceWidget::OnFilterLevelUp);

	controlLayout->addWidget(filterMinusBtn);
	controlLayout->addWidget(filterIndicator, 1);
	controlLayout->addWidget(filterPlusBtn);

	containerLayout->addWidget(controlRow);

	parentLayout->addWidget(filterLevelContainer);

	// Initialize display
	UpdateFilterLevelDisplay();
}

// ============================================================================
// Audio Monitoring Toggle
// ============================================================================

bool AudioSourceWidget::IsMonitoringAvailable()
{
	// Cache the result since it won't change during runtime
	static bool checked = false;
	static bool available = false;

	if (!checked) {
		checked = true;
		available = obs_audio_monitoring_available();
	}

	return available;
}

void AudioSourceWidget::OnMonitorButtonClicked()
{
	// Toggle: None ↔ Monitor and Output (keeps audio going to stream/recording)
	obs_monitoring_type newType;
	if (currentMonitoringType == OBS_MONITORING_TYPE_NONE) {
		newType = OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT;
	} else {
		newType = OBS_MONITORING_TYPE_NONE;
	}

	// Set the new monitoring type (this triggers the signal callback)
	obs_source_set_monitoring_type(source, newType);
	// Note: Button style will update via OBSMonitoringTypeChanged callback
}

void AudioSourceWidget::OBSMonitoringTypeChanged(void *data, calldata_t *cd)
{
	AudioSourceWidget *widget = static_cast<AudioSourceWidget *>(data);
	int type = calldata_int(cd, "type");

	// Update on Qt thread
	QMetaObject::invokeMethod(widget, [widget, type]() {
		widget->UpdateMonitorButtonFromOBS(type);
	}, Qt::QueuedConnection);
}

void AudioSourceWidget::UpdateMonitorButtonFromOBS(int type)
{
	currentMonitoringType = static_cast<obs_monitoring_type>(type);
	UpdateMonitorButtonStyle();
}

void AudioSourceWidget::UpdateMonitorButtonStyle()
{
	monitorButton->setText("");
	monitorButton->setIconSize(QSize(16, 16));

	// Two states: Off (NONE) or On (any monitoring enabled)
	bool isMonitoring = (currentMonitoringType != OBS_MONITORING_TYPE_NONE);
	const QString colorSuccess = "#37D247";

	if (isMonitoring) {
		// Monitoring enabled - green active state
		monitorButton->setIcon(QIcon(":/audio-monitor/images/monitor-and-output.svg"));
		monitorButton->setStyleSheet(QString(
			"QPushButton { "
			"  background-color: rgba(55, 210, 71, 0.2); "
			"  border: 1px solid %1; "
			"  border-radius: 4px; "
			"} "
			"QPushButton:hover { background-color: rgba(55, 210, 71, 0.3); }"
		).arg(colorSuccess));
	} else {
		// Monitoring disabled - inactive state
		monitorButton->setIcon(QIcon(":/audio-monitor/images/monitor-off.svg"));
		monitorButton->setStyleSheet(
			"QPushButton { "
			"  background-color: transparent; "
			"  border: 1px solid transparent; "
			"  border-radius: 4px; "
			"} "
			"QPushButton:hover { background-color: rgba(184, 184, 184, 0.2); }");
	}
}
