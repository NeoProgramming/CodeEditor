#include "MainWindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("CodeEditor");

	MainWindow w;
	w.show();

	return app.exec();
}
