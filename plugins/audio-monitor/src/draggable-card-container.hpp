/*
 * Audio Monitor Plugin - Draggable Card Container
 *
 * A container widget that manages a horizontal layout of draggable cards.
 * Supports drag-and-drop reordering with visual feedback (drop indicator).
 */

#pragma once

#include <QHBoxLayout>
#include <QMap>
#include <QStringList>
#include <QWidget>

class DraggableCardContainer : public QWidget {
	Q_OBJECT

public:
	explicit DraggableCardContainer(QWidget *parent = nullptr);
	~DraggableCardContainer();

	void addWidget(QWidget *widget, const QString &uuid);
	QWidget *removeWidget(const QString &uuid);
	void setOrder(const QStringList &order);
	QStringList currentOrder() const;
	int indexOf(const QString &uuid) const;
	void clear();
	int count() const { return widgets.count(); }
	QWidget *widget(const QString &uuid) const { return widgets.value(uuid); }
	void setSpacing(int spacing);
	void setContentsMargins(int left, int top, int right, int bottom);

signals:
	void orderChanged(const QStringList &newOrder);

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	QHBoxLayout *layout;
	QMap<QString, QWidget *> widgets;
	QStringList order;

	QWidget *dropIndicator = nullptr;
	int dropIndex = -1;

	void showDropIndicator(int index);
	void hideDropIndicator();
	int calculateDropIndex(const QPoint &pos);
	void rebuildLayout();
};
