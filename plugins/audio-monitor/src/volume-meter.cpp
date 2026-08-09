/*
 * Audio Monitor Plugin - Volume Meter Widget
 * Adapted from OBS Studio frontend/widgets/VolumeMeter.cpp
 * Standalone version without OBSApp dependency
 */

#include "volume-meter.hpp"

#include <util/platform.h>

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

// Size of the audio indicator in pixels
#define INDICATOR_THICKNESS 3

// Padding on top and bottom of vertical meters
#define METER_PADDING 1

std::weak_ptr<VolumeMeterTimer> VolumeMeter::updateTimer;

// VolumeMeterTimer implementation
void VolumeMeterTimer::AddVolControl(VolumeMeter *meter)
{
	volumeMeters.push_back(meter);
}

void VolumeMeterTimer::RemoveVolControl(VolumeMeter *meter)
{
	volumeMeters.removeOne(meter);
}

void VolumeMeterTimer::timerEvent(QTimerEvent *)
{
	for (VolumeMeter *meter : volumeMeters) {
		if (meter->needLayoutChange()) {
			// Tell paintEvent to update layout and paint everything
			meter->update();
		} else {
			// Tell paintEvent to paint only the bars
			meter->update(meter->getBarRect());
		}
	}
}

// VolumeMeter property getters/setters

QColor VolumeMeter::getBackgroundNominalColor() const
{
	return backgroundNominalColor;
}

QColor VolumeMeter::getBackgroundNominalColorDisabled() const
{
	return backgroundNominalColorDisabled;
}

void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	backgroundNominalColor = std::move(c);
}

void VolumeMeter::setBackgroundNominalColorDisabled(QColor c)
{
	backgroundNominalColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundWarningColor() const
{
	return backgroundWarningColor;
}

QColor VolumeMeter::getBackgroundWarningColorDisabled() const
{
	return backgroundWarningColorDisabled;
}

void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	backgroundWarningColor = std::move(c);
}

void VolumeMeter::setBackgroundWarningColorDisabled(QColor c)
{
	backgroundWarningColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundErrorColor() const
{
	return backgroundErrorColor;
}

QColor VolumeMeter::getBackgroundErrorColorDisabled() const
{
	return backgroundErrorColorDisabled;
}

void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	backgroundErrorColor = std::move(c);
}

void VolumeMeter::setBackgroundErrorColorDisabled(QColor c)
{
	backgroundErrorColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundNominalColor() const
{
	return foregroundNominalColor;
}

QColor VolumeMeter::getForegroundNominalColorDisabled() const
{
	return foregroundNominalColorDisabled;
}

void VolumeMeter::setForegroundNominalColor(QColor c)
{
	foregroundNominalColor = std::move(c);
}

void VolumeMeter::setForegroundNominalColorDisabled(QColor c)
{
	foregroundNominalColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundWarningColor() const
{
	return foregroundWarningColor;
}

QColor VolumeMeter::getForegroundWarningColorDisabled() const
{
	return foregroundWarningColorDisabled;
}

void VolumeMeter::setForegroundWarningColor(QColor c)
{
	foregroundWarningColor = std::move(c);
}

void VolumeMeter::setForegroundWarningColorDisabled(QColor c)
{
	foregroundWarningColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundErrorColor() const
{
	return foregroundErrorColor;
}

QColor VolumeMeter::getForegroundErrorColorDisabled() const
{
	return foregroundErrorColorDisabled;
}

void VolumeMeter::setForegroundErrorColor(QColor c)
{
	foregroundErrorColor = std::move(c);
}

void VolumeMeter::setForegroundErrorColorDisabled(QColor c)
{
	foregroundErrorColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundQuietColor() const
{
	return backgroundQuietColor;
}

void VolumeMeter::setBackgroundQuietColor(QColor c)
{
	backgroundQuietColor = std::move(c);
}

QColor VolumeMeter::getForegroundQuietColor() const
{
	return foregroundQuietColor;
}

void VolumeMeter::setForegroundQuietColor(QColor c)
{
	foregroundQuietColor = std::move(c);
}

QColor VolumeMeter::getClipColor() const
{
	return clipColor;
}

void VolumeMeter::setClipColor(QColor c)
{
	clipColor = std::move(c);
}

QColor VolumeMeter::getMagnitudeColor() const
{
	return magnitudeColor;
}

void VolumeMeter::setMagnitudeColor(QColor c)
{
	magnitudeColor = std::move(c);
}

QColor VolumeMeter::getMajorTickColor() const
{
	return majorTickColor;
}

void VolumeMeter::setMajorTickColor(QColor c)
{
	majorTickColor = std::move(c);
}

QColor VolumeMeter::getMinorTickColor() const
{
	return minorTickColor;
}

void VolumeMeter::setMinorTickColor(QColor c)
{
	minorTickColor = std::move(c);
}

int VolumeMeter::getMeterThickness() const
{
	return meterThickness;
}

void VolumeMeter::setMeterThickness(int v)
{
	meterThickness = v;
	recalculateLayout = true;
}

qreal VolumeMeter::getMeterFontScaling() const
{
	return meterFontScaling;
}

void VolumeMeter::setMeterFontScaling(qreal v)
{
	meterFontScaling = v;
	recalculateLayout = true;
}

qreal VolumeMeter::getMinimumLevel() const
{
	return minimumLevel;
}

void VolumeMeter::setMinimumLevel(qreal v)
{
	minimumLevel = v;
}

qreal VolumeMeter::getNominalLevel() const
{
	return nominalLevel;
}

void VolumeMeter::setNominalLevel(qreal v)
{
	nominalLevel = v;
}

qreal VolumeMeter::getWarningLevel() const
{
	return warningLevel;
}

void VolumeMeter::setWarningLevel(qreal v)
{
	warningLevel = v;
}

qreal VolumeMeter::getErrorLevel() const
{
	return errorLevel;
}

void VolumeMeter::setErrorLevel(qreal v)
{
	errorLevel = v;
}

qreal VolumeMeter::getClipLevel() const
{
	return clipLevel;
}

void VolumeMeter::setClipLevel(qreal v)
{
	clipLevel = v;
}

qreal VolumeMeter::getMinimumInputLevel() const
{
	return minimumInputLevel;
}

void VolumeMeter::setMinimumInputLevel(qreal v)
{
	minimumInputLevel = v;
}

qreal VolumeMeter::getPeakDecayRate() const
{
	return peakDecayRate;
}

void VolumeMeter::setPeakDecayRate(qreal v)
{
	peakDecayRate = v;
}

qreal VolumeMeter::getMagnitudeIntegrationTime() const
{
	return magnitudeIntegrationTime;
}

void VolumeMeter::setMagnitudeIntegrationTime(qreal v)
{
	magnitudeIntegrationTime = v;
}

qreal VolumeMeter::getPeakHoldDuration() const
{
	return peakHoldDuration;
}

void VolumeMeter::setPeakHoldDuration(qreal v)
{
	peakHoldDuration = v;
}

qreal VolumeMeter::getInputPeakHoldDuration() const
{
	return inputPeakHoldDuration;
}

void VolumeMeter::setInputPeakHoldDuration(qreal v)
{
	inputPeakHoldDuration = v;
}

void VolumeMeter::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	obs_volmeter_set_peak_meter_type(obs_volmeter, peakMeterType);
	switch (peakMeterType) {
	case TRUE_PEAK_METER:
		setErrorLevel(-2.0);
		setWarningLevel(-13.0);
		break;

	case SAMPLE_PEAK_METER:
	default:
		setErrorLevel(-9.0);
		setWarningLevel(-20.0);
		break;
	}
}

void VolumeMeter::mousePressEvent(QMouseEvent *event)
{
	setFocus(Qt::MouseFocusReason);
	event->accept();
}

void VolumeMeter::wheelEvent(QWheelEvent *event)
{
	QApplication::sendEvent(focusProxy(), event);
}

VolumeMeter::VolumeMeter(QWidget *parent, obs_volmeter_t *obs_volmeter, bool vertical)
	: QWidget(parent),
	  obs_volmeter(obs_volmeter),
	  vertical(vertical)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	// Default meter colors
	backgroundNominalColor.setRgb(0x26, 0x7f, 0x26);   // Dark green
	backgroundWarningColor.setRgb(0x7f, 0x7f, 0x26);   // Dark yellow
	backgroundErrorColor.setRgb(0x7f, 0x26, 0x26);     // Dark red
	foregroundNominalColor.setRgb(0x4c, 0xff, 0x4c);   // Bright green
	foregroundWarningColor.setRgb(0xff, 0xff, 0x4c);   // Bright yellow
	foregroundErrorColor.setRgb(0xff, 0x4c, 0x4c);     // Bright red

	// Quiet zone colors (blue/gray for "too quiet")
	backgroundQuietColor.setRgb(0x30, 0x30, 0x50);     // Dark blue-gray
	foregroundQuietColor.setRgb(0x80, 0x80, 0xC0);     // Light blue-gray

	backgroundNominalColorDisabled.setRgb(90, 90, 90);
	backgroundWarningColorDisabled.setRgb(117, 117, 117);
	backgroundErrorColorDisabled.setRgb(65, 65, 65);
	foregroundNominalColorDisabled.setRgb(163, 163, 163);
	foregroundWarningColorDisabled.setRgb(217, 217, 217);
	foregroundErrorColorDisabled.setRgb(113, 113, 113);

	clipColor.setRgb(0xff, 0xff, 0xff);      // Bright white
	magnitudeColor.setRgb(0x00, 0x00, 0x00); // Black
	majorTickColor.setRgb(0x00, 0x00, 0x00); // Black
	minorTickColor.setRgb(0x32, 0x32, 0x32); // Dark gray
	minimumLevel = -60.0;                    // -60 dB
	nominalLevel = -40.0;                    // -40 dB (below this = "too quiet")
	warningLevel = -20.0;                    // -20 dB
	errorLevel = -9.0;                       //  -9 dB
	clipLevel = -0.5;                        //  -0.5 dB
	minimumInputLevel = -50.0;               // -50 dB
	peakDecayRate = 11.76;                   //  20 dB / 1.7 sec
	magnitudeIntegrationTime = 0.3;          //  99% in 300 ms
	peakHoldDuration = 20.0;                 //  20 seconds
	inputPeakHoldDuration = 1.0;             //  1 second
	meterThickness = 9;                      // Bar thickness in pixels
	meterFontScaling = 0.7;                  // Font size for numbers is 70% of Widget's font size
	channels = (int)audio_output_get_channels(obs_get_audio());

	doLayout();
	updateTimerRef = updateTimer.lock();
	if (!updateTimerRef) {
		updateTimerRef = std::make_shared<VolumeMeterTimer>();
		updateTimerRef->setTimerType(Qt::PreciseTimer);
		updateTimerRef->start(16);
		updateTimer = updateTimerRef;
	}

	updateTimerRef->AddVolControl(this);
}

VolumeMeter::~VolumeMeter()
{
	updateTimerRef->RemoveVolControl(this);
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
			    const float inputPeak[MAX_AUDIO_CHANNELS])
{
	uint64_t ts = os_gettime_ns();
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = ts;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = magnitude[channelNr];
		currentPeak[channelNr] = peak[channelNr];
		currentInputPeak[channelNr] = inputPeak[channelNr];
	}

	locker.unlock();
	calculateBallistics(ts);
}

inline void VolumeMeter::resetLevels()
{
	currentLastUpdateTime = 0;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = -M_INFINITE;
		currentPeak[channelNr] = -M_INFINITE;
		currentInputPeak[channelNr] = -M_INFINITE;

		displayMagnitude[channelNr] = -M_INFINITE;
		displayPeak[channelNr] = -M_INFINITE;
		displayPeakHold[channelNr] = -M_INFINITE;
		displayPeakHoldLastUpdateTime[channelNr] = 0;
		displayInputPeakHold[channelNr] = -M_INFINITE;
		displayInputPeakHoldLastUpdateTime[channelNr] = 0;
	}
}

bool VolumeMeter::needLayoutChange()
{
	int currentNrAudioChannels = obs_volmeter_get_nr_channels(obs_volmeter);

	if (!currentNrAudioChannels) {
		struct obs_audio_info oai;
		obs_get_audio_info(&oai);
		currentNrAudioChannels = (oai.speakers == SPEAKERS_MONO) ? 1 : 2;
	}

	if (displayNrAudioChannels != currentNrAudioChannels) {
		displayNrAudioChannels = currentNrAudioChannels;
		recalculateLayout = true;
	}

	return recalculateLayout;
}

inline void VolumeMeter::doLayout()
{
	QMutexLocker locker(&dataMutex);

	if (displayNrAudioChannels) {
		int meterSize = std::floor(66 / displayNrAudioChannels);
		setMeterThickness(std::clamp(meterSize, 9, 21));
	}
	recalculateLayout = false;

	tickFont = font();
	QFontInfo info(tickFont);
	tickFont.setPointSizeF(info.pointSizeF() * meterFontScaling);
	QFontMetrics metrics(tickFont);
	if (vertical) {
		QRect scaleBounds = metrics.boundingRect("-88");
		setMinimumSize(displayNrAudioChannels * (meterThickness + 1) - 1 + 10 + scaleBounds.width() + 2, 100);
	} else {
		setMinimumSize(100, displayNrAudioChannels * (meterThickness + 1) - 1 + 4 + metrics.capHeight());
	}

	resetLevels();
}

inline bool VolumeMeter::detectIdle(uint64_t ts)
{
	double timeSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
	if (timeSinceLastUpdate > 0.5) {
		resetLevels();
		return true;
	} else {
		return false;
	}
}

inline void VolumeMeter::calculateBallisticsForChannel(int channelNr, uint64_t ts, qreal timeSinceLastRedraw)
{
	if (currentPeak[channelNr] >= displayPeak[channelNr] || isnan(displayPeak[channelNr])) {
		displayPeak[channelNr] = currentPeak[channelNr];
	} else {
		float decay = float(peakDecayRate * timeSinceLastRedraw);
		displayPeak[channelNr] =
			std::clamp(displayPeak[channelNr] - decay, std::min(currentPeak[channelNr], 0.f), 0.f);
	}

	if (currentPeak[channelNr] >= displayPeakHold[channelNr] || !isfinite(displayPeakHold[channelNr])) {
		displayPeakHold[channelNr] = currentPeak[channelNr];
		displayPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak = (uint64_t)(ts - displayPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > peakHoldDuration) {
			displayPeakHold[channelNr] = currentPeak[channelNr];
			displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
	    !isfinite(displayInputPeakHold[channelNr])) {
		displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
		displayInputPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak = (uint64_t)(ts - displayInputPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > inputPeakHoldDuration) {
			displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
			displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (!isfinite(displayMagnitude[channelNr])) {
		displayMagnitude[channelNr] = currentMagnitude[channelNr];
	} else {
		float attack = float((currentMagnitude[channelNr] - displayMagnitude[channelNr]) *
				     (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
		displayMagnitude[channelNr] =
			std::clamp(displayMagnitude[channelNr] + attack, (float)minimumLevel, 0.f);
	}
}

inline void VolumeMeter::calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);

	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++)
		calculateBallisticsForChannel(channelNr, ts, timeSinceLastRedraw);
}

void VolumeMeter::paintInputMeter(QPainter &painter, int x, int y, int width, int height, float peakHold)
{
	QMutexLocker locker(&dataMutex);
	QColor color;

	if (peakHold < minimumInputLevel)
		color = backgroundNominalColor;
	else if (peakHold < warningLevel)
		color = foregroundNominalColor;
	else if (peakHold < errorLevel)
		color = foregroundWarningColor;
	else if (peakHold <= clipLevel)
		color = foregroundErrorColor;
	else
		color = clipColor;

	painter.fillRect(x, y, width, height, color);
}

void VolumeMeter::paintHTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);

		QRect textBounds = metrics.boundingRect(str);
		int pos;
		if (i == 0) {
			pos = position - textBounds.width();
		} else {
			pos = position - (textBounds.width() / 2);
			if (pos < 0)
				pos = 0;
		}
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);
		painter.drawLine(position, y, position, y + 2);
	}
}

void VolumeMeter::paintVTicks(QPainter &painter, int x, int y, int height)
{
	qreal scale = height / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = y + int(i * scale) + METER_PADDING;
		QString str = QString::number(i);

		if (i == 0) {
			painter.drawText(x + 10, position + metrics.capHeight(), str);
		} else {
			painter.drawText(x + 8, position + (metrics.capHeight() / 2), str);
		}
		painter.drawLine(x, position, x + 2, position);
	}
}

#define CLIP_FLASH_DURATION_MS 1000

inline int VolumeMeter::convertToInt(float number)
{
	constexpr int min = std::numeric_limits<int>::min();
	constexpr int max = std::numeric_limits<int>::max();

	if (number >= (float)max)
		return max;
	else if (number < min)
		return min;
	else
		return int(number);
}

void VolumeMeter::paintHMeter(QPainter &painter, int x, int y, int width, int height, float magnitude, float peak,
			      float peakHold)
{
	qreal scale = width / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = x + 0;
	int maximumPosition = x + width;
	int magnitudePosition = x + width - convertToInt(magnitude * scale);
	int peakPosition = x + width - convertToInt(peak * scale);
	int peakHoldPosition = x + width - convertToInt(peakHold * scale);
	int nominalPosition = x + width - convertToInt(nominalLevel * scale);
	int warningPosition = x + width - convertToInt(warningLevel * scale);
	int errorPosition = x + width - convertToInt(errorLevel * scale);

	int quietLength = nominalPosition - minimumPosition;
	int nominalLength = warningPosition - nominalPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		// Peak below minimum - all background
		painter.fillRect(minimumPosition, y, quietLength, height, backgroundQuietColor);
		painter.fillRect(nominalPosition, y, nominalLength, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < nominalPosition) {
		// Peak in quiet zone (blue)
		painter.fillRect(minimumPosition, y, peakPosition - minimumPosition, height, foregroundQuietColor);
		painter.fillRect(peakPosition, y, nominalPosition - peakPosition, height, backgroundQuietColor);
		painter.fillRect(nominalPosition, y, nominalLength, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		// Peak in nominal zone (green)
		painter.fillRect(minimumPosition, y, quietLength, height, foregroundQuietColor);
		painter.fillRect(nominalPosition, y, peakPosition - nominalPosition, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(peakPosition, y, warningPosition - peakPosition, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		// Peak in warning zone (yellow)
		painter.fillRect(minimumPosition, y, quietLength, height, foregroundQuietColor);
		painter.fillRect(nominalPosition, y, nominalLength, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(warningPosition, y, peakPosition - warningPosition, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(peakPosition, y, errorPosition - peakPosition, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		// Peak in error zone (red)
		painter.fillRect(minimumPosition, y, quietLength, height, foregroundQuietColor);
		painter.fillRect(nominalPosition, y, nominalLength, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(errorPosition, y, peakPosition - errorPosition, height,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
		painter.fillRect(peakPosition, y, maximumPosition - peakPosition, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else {
		// Clipping
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [&]() { clipping = false; });
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength + quietLength;
		painter.fillRect(minimumPosition, y, end, height,
				 QBrush(muted ? foregroundErrorColorDisabled : foregroundErrorColor));
	}

	// Peak hold indicator
	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < nominalPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height, foregroundQuietColor);
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
	else
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(magnitudePosition - 3, y, 3, height, magnitudeColor);
}

void VolumeMeter::paintVMeter(QPainter &painter, int x, int y, int width, int height, float magnitude, float peak,
			      float peakHold)
{
	qreal scale = height / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = y + 0;
	int maximumPosition = y + height;
	int magnitudePosition = y + height - convertToInt(magnitude * scale);
	int peakPosition = y + height - convertToInt(peak * scale);
	int peakHoldPosition = y + height - convertToInt(peakHold * scale);
	int nominalPosition = y + height - convertToInt(nominalLevel * scale);
	int warningPosition = y + height - convertToInt(warningLevel * scale);
	int errorPosition = y + height - convertToInt(errorLevel * scale);

	int quietLength = nominalPosition - minimumPosition;
	int nominalLength = warningPosition - nominalPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	// Helper lambda to draw the background zones
	auto drawBackgroundZones = [&]() {
		painter.fillRect(x, minimumPosition, width, quietLength, backgroundQuietColor);
		painter.fillRect(x, nominalPosition, width, nominalLength,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	};

	if (peakPosition < minimumPosition) {
		// Peak below minimum - all background
		drawBackgroundZones();
	} else if (peakPosition < nominalPosition) {
		// Peak in quiet zone (blue)
		painter.fillRect(x, minimumPosition, width, peakPosition - minimumPosition, foregroundQuietColor);
		painter.fillRect(x, peakPosition, width, nominalPosition - peakPosition, backgroundQuietColor);
		painter.fillRect(x, nominalPosition, width, nominalLength,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		// Peak in nominal zone (green)
		painter.fillRect(x, minimumPosition, width, quietLength, foregroundQuietColor);
		painter.fillRect(x, nominalPosition, width, peakPosition - nominalPosition,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, peakPosition, width, warningPosition - peakPosition,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		// Peak in warning zone (yellow)
		painter.fillRect(x, minimumPosition, width, quietLength, foregroundQuietColor);
		painter.fillRect(x, nominalPosition, width, nominalLength,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, peakPosition - warningPosition,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(x, peakPosition, width, errorPosition - peakPosition,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		// Peak in error zone (red)
		painter.fillRect(x, minimumPosition, width, quietLength, foregroundQuietColor);
		painter.fillRect(x, nominalPosition, width, nominalLength,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(x, errorPosition, width, peakPosition - errorPosition,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
		painter.fillRect(x, peakPosition, width, maximumPosition - peakPosition,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else {
		// Clipping
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [&]() { clipping = false; });
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength + quietLength;
		painter.fillRect(x, minimumPosition, width, end,
				 QBrush(muted ? foregroundErrorColorDisabled : foregroundErrorColor));
	}

	// Peak hold indicator
	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < nominalPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3, foregroundQuietColor);
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
	else
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(x, magnitudePosition - 3, width, 3, magnitudeColor);
}

void VolumeMeter::paintEvent(QPaintEvent *event)
{
	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	QRect widgetRect = rect();
	int width = widgetRect.width();
	int height = widgetRect.height();

	QPainter painter(this);

	// Paint window background color (as widget is opaque)
	QColor background = palette().color(QPalette::ColorRole::Window);
	painter.fillRect(event->region().boundingRect(), background);

	if (vertical)
		height -= METER_PADDING * 2;

	if (event->region().boundingRect() != getBarRect()) {
		if (needLayoutChange())
			doLayout();

		if (vertical) {
			paintVTicks(painter, displayNrAudioChannels * (meterThickness + 1) - 1, 0,
				    height - (INDICATOR_THICKNESS + 3));
		} else {
			paintHTicks(painter, INDICATOR_THICKNESS + 3, displayNrAudioChannels * (meterThickness + 1) - 1,
				    width - (INDICATOR_THICKNESS + 3));
		}
	}

	if (vertical) {
		// Invert the Y axis to ease the math
		painter.translate(0, height + METER_PADDING);
		painter.scale(1, -1);
	}

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {

		int channelNrFixed = (displayNrAudioChannels == 1 && channels > 2) ? 2 : channelNr;

		if (vertical)
			paintVMeter(painter, channelNr * (meterThickness + 1), INDICATOR_THICKNESS + 2, meterThickness,
				    height - (INDICATOR_THICKNESS + 2), displayMagnitude[channelNrFixed],
				    displayPeak[channelNrFixed], displayPeakHold[channelNrFixed]);
		else
			paintHMeter(painter, INDICATOR_THICKNESS + 2, channelNr * (meterThickness + 1),
				    width - (INDICATOR_THICKNESS + 2), meterThickness, displayMagnitude[channelNrFixed],
				    displayPeak[channelNrFixed], displayPeakHold[channelNrFixed]);

		if (idle)
			continue;

		if (vertical)
			paintInputMeter(painter, channelNr * (meterThickness + 1), 0, meterThickness,
					INDICATOR_THICKNESS, displayInputPeakHold[channelNrFixed]);
		else
			paintInputMeter(painter, 0, channelNr * (meterThickness + 1), INDICATOR_THICKNESS,
					meterThickness, displayInputPeakHold[channelNrFixed]);
	}

	lastRedrawTime = ts;
}

QRect VolumeMeter::getBarRect() const
{
	QRect rec = rect();
	if (vertical)
		rec.setWidth(displayNrAudioChannels * (meterThickness + 1) - 1);
	else
		rec.setHeight(displayNrAudioChannels * (meterThickness + 1) - 1);

	return rec;
}

void VolumeMeter::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::StyleChange)
		recalculateLayout = true;

	QWidget::changeEvent(e);
}
