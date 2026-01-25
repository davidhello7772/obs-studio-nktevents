/*
 * Audio Monitor Plugin - Filter Item Widget
 * Single filter row with visibility toggle
 */

#include "filter-item-widget.hpp"

#include <QHBoxLayout>
#include <QFontMetrics>
#include <obs-frontend-api.h>

FilterItemWidget::FilterItemWidget(obs_source_t *filter_, QWidget *parent)
	: QWidget(parent),
	  filter(filter_)
{
	setFixedHeight(20);

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(2, 0, 2, 0);
	layout->setSpacing(4);

	// Eye toggle checkbox
	visCheckbox = new QCheckBox(this);
	visCheckbox->setFixedSize(16, 16);
	visCheckbox->setChecked(obs_source_enabled(filter));
	visCheckbox->setToolTip(tr("Toggle filter"));
	visCheckbox->setStyleSheet(R"(
		QCheckBox::indicator {
			width: 14px;
			height: 14px;
		}
		QCheckBox::indicator:checked {
			image: url(:/audio-monitor/images/visible.svg);
		}
		QCheckBox::indicator:unchecked {
			image: url(:/audio-monitor/images/invisible.svg);
		}
	)");

	// Filter name label
	fullName = QString::fromUtf8(obs_source_get_name(filter));
	nameLabel = new QLabel(this);
	nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	nameLabel->setToolTip(fullName);  // Full name on hover

	layout->addWidget(visCheckbox);
	layout->addWidget(nameLabel, 1);

	// Connect toggle
	connect(visCheckbox, &QCheckBox::clicked, [this](bool checked) {
		obs_source_set_enabled(filter, checked);
	});

	// Listen for external enable changes
	signal_handler_t *sh = obs_source_get_signal_handler(filter);
	if (sh) {
		signal_handler_connect(sh, "enable", OBSFilterEnabled, this);
	}

	UpdateVisualState();
}

FilterItemWidget::~FilterItemWidget()
{
	// CRITICAL: Disconnect signal handler FIRST
	// The filter source may still exist and could fire "enable" signal
	// after this widget is deleted, causing a crash
	signal_handler_t *sh = obs_source_get_signal_handler(filter);
	if (sh) {
		signal_handler_disconnect(sh, "enable", OBSFilterEnabled, this);
	}

	// OBSSource RAII wrapper handles obs_source_release automatically
	// Qt handles child widget cleanup (visCheckbox, nameLabel)
}

void FilterItemWidget::UpdateVisualState()
{
	bool enabled = obs_source_enabled(filter);
	visCheckbox->setChecked(enabled);

	// Calculate elided text based on current width
	QFontMetrics fm(nameLabel->font());
	int availableWidth = 150;  // Approximate available width for text (column is 190px)
	QString elidedText = fm.elidedText(fullName, Qt::ElideRight, availableWidth);
	nameLabel->setText(elidedText);

	// Dim disabled filters
	nameLabel->setStyleSheet(enabled
		? "font-size: 11px; color: palette(text);"
		: "font-size: 11px; color: palette(mid);");
}

void FilterItemWidget::OBSFilterEnabled(void *data, calldata_t *)
{
	FilterItemWidget *widget = static_cast<FilterItemWidget *>(data);

	// Post to Qt event loop - safe even if called from audio thread
	QMetaObject::invokeMethod(widget, "UpdateVisualState", Qt::QueuedConnection);
}
