/*
 * Audio Monitor Plugin - Filter List Widget
 * Container for filter items with dynamic updates
 */

#include "filter-list-widget.hpp"
#include "filter-item-widget.hpp"

#include <obs-frontend-api.h>

FilterListWidget::FilterListWidget(obs_source_t *source_, QWidget *parent)
	: QWidget(parent),
	  source(source_)
{
	// FIXED HEIGHT for consistent column alignment across all sources
	setFixedHeight(FIXED_HEIGHT);

	layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(2);

	// Overflow label (reused, not deleted during refresh)
	overflowLabel = new QLabel(this);
	overflowLabel->setStyleSheet("font-size: 10px; color: palette(mid);");
	overflowLabel->setAlignment(Qt::AlignCenter);
	overflowLabel->hide();

	RefreshFilters();

	// Connect to filter add/remove signals
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_connect(sh, "filter_add", OBSFilterAdded, this);
		signal_handler_connect(sh, "filter_remove", OBSFilterRemoved, this);
	}
}

FilterListWidget::~FilterListWidget()
{
	// CRITICAL: Disconnect signals FIRST to prevent callbacks during destruction
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_disconnect(sh, "filter_add", OBSFilterAdded, this);
		signal_handler_disconnect(sh, "filter_remove", OBSFilterRemoved, this);
	}

	// Delete all filter item widgets (their destructors handle their own cleanup)
	for (auto *w : filterWidgets) {
		delete w;
	}
	filterWidgets.clear();

	// OBSSource RAII wrapper handles obs_source_release automatically
}

void FilterListWidget::RefreshFilters()
{
	// STEP 1: Delete ALL existing filter widgets first
	// This triggers their destructors which disconnect signals
	for (auto *w : filterWidgets) {
		layout->removeWidget(w);
		w->deleteLater();  // Use deleteLater for Qt safety
	}
	filterWidgets.clear();

	// STEP 2: Remove overflow label from layout (but don't delete, we reuse it)
	layout->removeWidget(overflowLabel);
	overflowLabel->hide();

	// STEP 3: Enumerate and create new widgets
	struct FilterData {
		FilterListWidget *self;
		int count;
	} data = {this, 0};

	obs_source_enum_filters(source, [](obs_source_t *, obs_source_t *filter, void *param) {
		FilterData *d = static_cast<FilterData *>(param);

		if (d->count < MAX_VISIBLE_FILTERS) {
			FilterItemWidget *item = new FilterItemWidget(filter, d->self);
			d->self->filterWidgets.append(item);
			d->self->layout->insertWidget(d->count, item);
		}
		d->count++;
	}, &data);

	// Show overflow indicator if needed
	if (data.count > MAX_VISIBLE_FILTERS) {
		int extra = data.count - MAX_VISIBLE_FILTERS;
		overflowLabel->setText(QString("+%1 more").arg(extra));
		overflowLabel->show();
		layout->addWidget(overflowLabel);
	}

	// Add stretch at end to push filters to top when < 4 filters
	layout->addStretch(1);
}

void FilterListWidget::OBSFilterAdded(void *data, calldata_t *)
{
	FilterListWidget *widget = static_cast<FilterListWidget *>(data);

	// Post to Qt event loop - safe even if called from any thread
	QMetaObject::invokeMethod(widget, "RefreshFilters", Qt::QueuedConnection);
}

void FilterListWidget::OBSFilterRemoved(void *data, calldata_t *)
{
	FilterListWidget *widget = static_cast<FilterListWidget *>(data);

	// Post to Qt event loop - safe even if called from any thread
	QMetaObject::invokeMethod(widget, "RefreshFilters", Qt::QueuedConnection);
}
