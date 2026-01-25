/*
 * Audio Monitor Plugin - Audio Source Widget
 * Per-source widget with vertical meter and colored background
 */

#include "audio-source-widget.hpp"
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
	  savedColor(color)
{
	// Set up the main frame with colored background and border
	setFrameShape(QFrame::Box);
	setFrameShadow(QFrame::Plain);
	setLineWidth(3);
	setAutoFillBackground(true);

	// Main vertical layout with generous padding for color to wrap around
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(12, 12, 12, 12);
	mainLayout->setSpacing(6);

	// Fixed-height badge container (keeps meter height consistent)
	badgeContainer = new QWidget();
	badgeContainer->setFixedHeight(22);
	QHBoxLayout *badgeLayout = new QHBoxLayout(badgeContainer);
	badgeLayout->setContentsMargins(0, 0, 0, 0);
	badgeLayout->setSpacing(4);

	// Type badge (MIX, TRANS, REF)
	typeBadge = new QLabel();
	typeBadge->setAlignment(Qt::AlignCenter);
	typeBadge->setFixedHeight(18);
	typeBadge->hide();

	// Warning label (HIGH VOLUME!, LOW VOLUME!)
	warningLabel = new QLabel();
	warningLabel->setAlignment(Qt::AlignCenter);
	warningLabel->setFixedHeight(18);
	warningLabel->hide();

	badgeLayout->addStretch();
	badgeLayout->addWidget(typeBadge);
	badgeLayout->addWidget(warningLabel);
	badgeLayout->addStretch();
	mainLayout->addWidget(badgeContainer);

	// Source name label (centered, wrapped)
	nameLabel = new QLabel(obs_source_get_name(source));
	nameLabel->setAlignment(Qt::AlignCenter);
	nameLabel->setWordWrap(true);
	nameLabel->setStyleSheet("font-weight: bold; color: white; background: transparent;");
	mainLayout->addWidget(nameLabel);

	// Vertical volume meter (takes most space) - increased width
	volMeter = new VolumeMeter(this, obs_volmeter, true);  // true = vertical
	volMeter->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	volMeter->setMinimumWidth(60);  // Increased from 50
	volMeter->setMinimumHeight(180);

	// Center the meter horizontally
	QHBoxLayout *meterLayout = new QHBoxLayout();
	meterLayout->addStretch();
	meterLayout->addWidget(volMeter);
	meterLayout->addStretch();
	mainLayout->addLayout(meterLayout, 1);

	// Bottom buttons row: type toggle and color picker
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->setSpacing(8);

	// Type toggle button (cycles: Normal → Mix → Translated → Reference → Normal)
	typeButton = new QPushButton();
	typeButton->setFixedSize(24, 24);
	typeButton->setToolTip(obs_module_text("AudioMonitor.TypeButton.Tooltip"));
	connect(typeButton, &QPushButton::clicked, this, &AudioSourceWidget::OnTypeButtonClicked);
	UpdateTypeButtonStyle();

	// Monitor output toggle button (toggles monitoring on/off, keeps output to stream)
	monitorButton = new QPushButton();
	monitorButton->setFixedSize(28, 28);
	monitorButton->setToolTip("Toggle audio monitoring (output to stream unchanged)");
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
	colorButton->setIconSize(QSize(20, 20));
	colorButton->setStyleSheet(
		"QPushButton { "
		"  background-color: rgba(255, 255, 255, 0.9); "
		"  border: 2px solid white; "
		"  border-radius: 4px; "
		"} "
		"QPushButton:hover { background-color: white; }");
	connect(colorButton, &QPushButton::clicked, this, &AudioSourceWidget::OnColorButtonClicked);

	buttonLayout->addStretch();
	buttonLayout->addWidget(typeButton);
	buttonLayout->addWidget(monitorButton);
	buttonLayout->addWidget(colorButton);
	buttonLayout->addStretch();
	mainLayout->addLayout(buttonLayout);

	// Filter list widget (shows filters applied to this source)
	filterList = new FilterListWidget(source, this);
	mainLayout->addWidget(filterList);

	setLayout(mainLayout);

	// Set minimum/preferred size for the widget (190px width for filter names, 370px height for filter list)
	setMinimumWidth(190);
	setFixedWidth(190);
	setMinimumHeight(370);  // Increased from 280 to accommodate filter list
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

	// Also set stylesheet for border and border-radius (these need CSS)
	QString frameStyle = QString(
		"QFrame { "
		"  background-color: %1; "
		"  border: 3px solid %1; "
		"  border-radius: 8px; "
		"}"
	).arg(currentColor.name());
	setStyleSheet(frameStyle);

	// Update color button to show the saved (user-chosen) color
	QString buttonStyle = QString("background-color: %1; border: 2px solid white; border-radius: 4px;")
				      .arg(savedColor.name());
	colorButton->setStyleSheet(buttonStyle);

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

void AudioSourceWidget::OnTypeButtonClicked()
{
	// Cycle through: Normal → Mix → Translated → Reference → Normal
	SourceType newType;
	switch (sourceType) {
	case SourceType::Normal:
		newType = SourceType::Mix;
		break;
	case SourceType::Mix:
		newType = SourceType::Translated;
		break;
	case SourceType::Translated:
		newType = SourceType::Reference;
		break;
	case SourceType::Reference:
	default:
		newType = SourceType::Normal;
		break;
	}
	emit sourceTypeChanged(this, newType);
}

void AudioSourceWidget::SetSourceType(SourceType type)
{
	if (sourceType == type)
		return;

	sourceType = type;
	UpdateTypeButtonStyle();
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
		typeBadge->setText("MIX");
		typeBadge->setStyleSheet(
			"background-color: #FF6B35; color: white; font-weight: bold; "
			"font-size: 9px; padding: 2px 4px; border-radius: 3px;");
		typeBadge->show();
		break;
	case SourceType::Translated:
		typeBadge->setText("TRANS");
		typeBadge->setStyleSheet(
			"background-color: #4A90D9; color: white; font-weight: bold; "
			"font-size: 9px; padding: 2px 4px; border-radius: 3px;");
		typeBadge->show();
		break;
	case SourceType::Reference:
		typeBadge->setText("REF");
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

void AudioSourceWidget::UpdateTypeButtonStyle()
{
	typeButton->setText("");  // Clear any text
	typeButton->setIconSize(QSize(20, 20));

	switch (sourceType) {
	case SourceType::Mix:
		// Mix: star icon with orange background
		typeButton->setIcon(QIcon(":/audio-monitor/images/type-mix.svg"));
		typeButton->setStyleSheet(
			"QPushButton { "
			"  background-color: #FF6B35; "
			"  border: none; "
			"  border-radius: 12px; "
			"} "
			"QPushButton:hover { background-color: #FF8C5A; }");
		break;
	case SourceType::Translated:
		// Translated: globe icon with blue background
		typeButton->setIcon(QIcon(":/audio-monitor/images/type-translated.svg"));
		typeButton->setStyleSheet(
			"QPushButton { "
			"  background-color: #4A90D9; "
			"  border: none; "
			"  border-radius: 12px; "
			"} "
			"QPushButton:hover { background-color: #5DA0E9; }");
		break;
	case SourceType::Reference:
		// Reference: pin icon with purple background
		typeButton->setIcon(QIcon(":/audio-monitor/images/type-reference.svg"));
		typeButton->setStyleSheet(
			"QPushButton { "
			"  background-color: #9B59B6; "
			"  border: none; "
			"  border-radius: 12px; "
			"} "
			"QPushButton:hover { background-color: #A569C6; }");
		break;
	case SourceType::Normal:
	default:
		// Normal: subtle circle icon
		typeButton->setIcon(QIcon(":/audio-monitor/images/type-normal.svg"));
		typeButton->setStyleSheet(
			"QPushButton { "
			"  background-color: transparent; "
			"  border: none; "
			"  border-radius: 12px; "
			"} "
			"QPushButton:hover { "
			"  background-color: rgba(255, 107, 53, 0.3); "
			"}");
		break;
	}
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
			message = "HIGH!";
			warningColor = COLOR_WARNING_HIGH;
		} else {
			message = "LOW!";
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
	// KFS uses a scale with ceiling at +12dB, OBS uses dBFS with ceiling at 0dB
	// Apply -12dB offset to convert KFS values to OBS dBFS
	// Blue zone only shown for Mix and Translated sources

	switch (sourceType) {
	case SourceType::Mix:
		// Mix reference has lower levels (like KFS "English for mix")
		// Blue zone: below -28dB (too quiet)
		// Green zone: -28dB to -20dB (nominal)
		// Yellow zone: -20dB to -3dB (warning)
		// Red zone: above -3dB (error/clipping)
		volMeter->setNominalLevel(-28.0);
		volMeter->setWarningLevel(-20.0);
		volMeter->setErrorLevel(-3.0);
		break;
	case SourceType::Translated:
		// Translated channels - show blue zone with standard thresholds
		// Blue zone: below -9dB (too quiet)
		// Green zone: -9dB to -6dB (nominal)
		// Yellow zone: -6dB to -3dB (warning)
		// Red zone: above -3dB (error/clipping)
		volMeter->setNominalLevel(-9.0);
		volMeter->setWarningLevel(-6.0);
		volMeter->setErrorLevel(-3.0);
		break;
	case SourceType::Normal:
	case SourceType::Reference:
	default:
		// Normal and Reference sources - NO blue zone (nominalLevel = minimumLevel)
		// Green zone: from minimum to -6dB
		// Yellow zone: -6dB to -3dB (warning)
		// Red zone: above -3dB (error/clipping)
		volMeter->setNominalLevel(-60.0);  // Same as minimumLevel = no blue zone
		volMeter->setWarningLevel(-6.0);
		volMeter->setErrorLevel(-3.0);
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
	// Filter change callback - kept for future use
	Q_UNUSED(data);
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
	monitorButton->setIconSize(QSize(22, 22));

	// Two states: Off (NONE) or On (any monitoring enabled)
	bool isMonitoring = (currentMonitoringType != OBS_MONITORING_TYPE_NONE);

	if (isMonitoring) {
		// Monitoring enabled - green headphones with sound waves
		monitorButton->setIcon(QIcon(":/audio-monitor/images/monitor-and-output.svg"));
		monitorButton->setStyleSheet(
			"QPushButton { "
			"  background-color: transparent; "
			"  border: none; "
			"  border-radius: 4px; "
			"} "
			"QPushButton:hover { background-color: rgba(76, 175, 80, 0.3); }");
	} else {
		// Monitoring disabled - gray headphones with strike-through
		monitorButton->setIcon(QIcon(":/audio-monitor/images/monitor-off.svg"));
		monitorButton->setStyleSheet(
			"QPushButton { "
			"  background-color: transparent; "
			"  border: none; "
			"  border-radius: 4px; "
			"} "
			"QPushButton:hover { background-color: rgba(136, 136, 136, 0.3); }");
	}
}
