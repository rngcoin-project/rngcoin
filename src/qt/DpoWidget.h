//DpoWidget.h by Rngcoin developers
#pragma once
#include <QTabWidget>
#include <QDialog>
class DpoCreateRootWidget;
class DpoCreateRecordWidget;
class DpoRegisterDocWidget;

class DpoWidget: public QDialog {
	public:
		DpoWidget(QWidget*parent = 0);
		~DpoWidget();
		QString name()const;
		QString value()const;
	protected:
		QTabWidget* _tab = 0;

		DpoCreateRootWidget* _createRoot = 0;
		DpoCreateRecordWidget* _createRecord = 0;
		DpoRegisterDocWidget* _registerDoc = 0;
};
