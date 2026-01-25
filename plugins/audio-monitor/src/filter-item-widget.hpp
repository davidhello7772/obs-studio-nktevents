/*
 * Audio Monitor Plugin - Filter Item Widget
 * Single filter row with visibility toggle
 */

#pragma once

#include <obs.hpp>

#include <QCheckBox>
#include <QLabel>
#include <QWidget>

class FilterItemWidget : public QWidget {
	Q_OBJECT

public:
	explicit FilterItemWidget(obs_source_t *filter, QWidget *parent = nullptr);
	~FilterItemWidget();

public slots:
	void UpdateVisualState();

private:
	OBSSource filter;  // RAII wrapper - auto-releases reference
	QCheckBox *visCheckbox;
	QLabel *nameLabel;
	QString fullName;  // Store full name for tooltip

	// Static callback for filter enable signal
	static void OBSFilterEnabled(void *data, calldata_t *cd);
};
