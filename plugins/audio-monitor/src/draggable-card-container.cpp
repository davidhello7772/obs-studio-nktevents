/*
 * Audio Monitor Plugin - Draggable Card Container
 */

#include "draggable-card-container.hpp"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

static const char *MIME_TYPE = "application/x-audio-monitor-widget";

DraggableCardContainer::DraggableCardContainer(QWidget *parent)
	: QWidget(parent)
{
	layout = new QHBoxLayout(this);
	layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	layout->setSpacing(12);
	layout->setContentsMargins(16, 16, 16, 16);

	setAcceptDrops(true);
}

DraggableCardContainer::~DraggableCardContainer() {}

void DraggableCardContainer::addWidget(QWidget *widget, const QString &uuid)
{
	if (!widget || uuid.isEmpty())
		return;

	if (widgets.contains(uuid)) {
		QWidget *existing = widgets.take(uuid);
		layout->removeWidget(existing);
	}

	widgets[uuid] = widget;

	int savedIndex = order.indexOf(uuid);
	if (savedIndex == -1) {
		order.append(uuid);
	}

	rebuildLayout();
}

QWidget *DraggableCardContainer::removeWidget(const QString &uuid)
{
	if (!widgets.contains(uuid))
		return nullptr;

	QWidget *widget = widgets.take(uuid);
	layout->removeWidget(widget);

	return widget;
}

void DraggableCardContainer::setOrder(const QStringList &newOrder)
{
	QStringList finalOrder;

	for (const QString &uuid : newOrder) {
		if (widgets.contains(uuid) && !finalOrder.contains(uuid)) {
			finalOrder.append(uuid);
		}
	}

	for (const QString &uuid : widgets.keys()) {
		if (!finalOrder.contains(uuid)) {
			finalOrder.append(uuid);
		}
	}

	order = finalOrder;
	rebuildLayout();
}

QStringList DraggableCardContainer::currentOrder() const
{
	QStringList result;
	for (const QString &uuid : order) {
		if (widgets.contains(uuid)) {
			result.append(uuid);
		}
	}
	return result;
}

int DraggableCardContainer::indexOf(const QString &uuid) const
{
	QStringList current = currentOrder();
	return current.indexOf(uuid);
}

void DraggableCardContainer::clear()
{
	for (QWidget *w : widgets.values()) {
		layout->removeWidget(w);
		delete w;
	}
	widgets.clear();
	order.clear();
}

void DraggableCardContainer::setSpacing(int spacing)
{
	layout->setSpacing(spacing);
}

void DraggableCardContainer::setContentsMargins(int left, int top, int right, int bottom)
{
	layout->setContentsMargins(left, top, right, bottom);
}

void DraggableCardContainer::rebuildLayout()
{
	while (layout->count() > 0) {
		QLayoutItem *item = layout->takeAt(0);
		delete item;
	}

	for (const QString &uuid : order) {
		if (widgets.contains(uuid)) {
			layout->addWidget(widgets[uuid]);
		}
	}
}

void DraggableCardContainer::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat(MIME_TYPE)) {
		event->acceptProposedAction();
		showDropIndicator(calculateDropIndex(event->position().toPoint()));
	}
}

void DraggableCardContainer::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat(MIME_TYPE)) {
		int newIndex = calculateDropIndex(event->position().toPoint());
		showDropIndicator(newIndex);
		event->acceptProposedAction();
	}
}

void DraggableCardContainer::dragLeaveEvent(QDragLeaveEvent *event)
{
	Q_UNUSED(event);
	hideDropIndicator();
}

void DraggableCardContainer::dropEvent(QDropEvent *event)
{
	if (!event->mimeData()->hasFormat(MIME_TYPE)) {
		hideDropIndicator();
		return;
	}

	QString uuid = QString::fromUtf8(event->mimeData()->data(MIME_TYPE));

	QStringList visibleOrder = currentOrder();
	int sourceIndex = visibleOrder.indexOf(uuid);
	int targetIndex = dropIndex;

	if (sourceIndex != -1 && targetIndex != -1 && targetIndex != sourceIndex) {
		if (targetIndex > sourceIndex) {
			targetIndex--;
		}

		visibleOrder.removeAt(sourceIndex);
		visibleOrder.insert(targetIndex, uuid);

		order = visibleOrder;
		rebuildLayout();
		emit orderChanged(currentOrder());
	}

	hideDropIndicator();
	event->acceptProposedAction();
}

int DraggableCardContainer::calculateDropIndex(const QPoint &pos)
{
	QStringList visibleOrder = currentOrder();
	if (visibleOrder.isEmpty())
		return 0;

	int x = pos.x();
	int currentX = layout->contentsMargins().left();
	int spacing = layout->spacing();

	for (int i = 0; i < visibleOrder.size(); i++) {
		QWidget *w = widgets.value(visibleOrder[i]);
		if (!w)
			continue;

		int widgetCenter = currentX + w->width() / 2;
		if (x < widgetCenter)
			return i;

		currentX += w->width() + spacing;
	}

	return visibleOrder.size();
}

void DraggableCardContainer::showDropIndicator(int index)
{
	if (!dropIndicator) {
		dropIndicator = new QWidget(this);
		dropIndicator->setFixedWidth(4);
		dropIndicator->setStyleSheet(
			"background-color: #4A90D9; border-radius: 2px;");
	}

	dropIndex = index;

	QStringList visibleOrder = currentOrder();
	int x = layout->contentsMargins().left();
	int spacing = layout->spacing();

	for (int i = 0; i < index && i < visibleOrder.size(); i++) {
		if (QWidget *w = widgets.value(visibleOrder[i])) {
			x += w->width() + spacing;
		}
	}

	if (index > 0 && index <= visibleOrder.size()) {
		x -= spacing / 2 + 2;
	}

	int topMargin = layout->contentsMargins().top();
	int bottomMargin = layout->contentsMargins().bottom();
	int indicatorHeight = height() - topMargin - bottomMargin;

	dropIndicator->setGeometry(x, topMargin, 4, indicatorHeight);
	dropIndicator->show();
	dropIndicator->raise();
}

void DraggableCardContainer::hideDropIndicator()
{
	if (dropIndicator) {
		dropIndicator->hide();
	}
	dropIndex = -1;
}
