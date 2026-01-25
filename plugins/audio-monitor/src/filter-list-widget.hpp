/*
 * Audio Monitor Plugin - Filter List Widget
 * Container for filter items with dynamic updates
 */

#pragma once

#include <obs.hpp>

#include <QLabel>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class FilterItemWidget;

class FilterListWidget : public QWidget {
	Q_OBJECT

public:
	explicit FilterListWidget(obs_source_t *source, QWidget *parent = nullptr);
	~FilterListWidget();

public slots:
	void RefreshFilters();

private:
	OBSSource source;  // RAII wrapper - auto-releases reference
	QVBoxLayout *layout;
	QVector<FilterItemWidget *> filterWidgets;
	QLabel *overflowLabel;  // Shows "+N more" if > 4 filters

	static constexpr int MAX_VISIBLE_FILTERS = 4;
	static constexpr int FIXED_HEIGHT = 90;  // 4 rows × 20px + padding

	// Static signal handlers
	static void OBSFilterAdded(void *data, calldata_t *cd);
	static void OBSFilterRemoved(void *data, calldata_t *cd);
};
