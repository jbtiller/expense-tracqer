#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <QtWidgets>
#pragma GCC diagnostic pop

auto init_gui() -> void {
    auto* win = new QMainWindow();
    win->setWindowTitle("Hello World");
    win->resize(320, 240);
    win->show();

    auto* box = new QPushButton("Do you dare?", win);
    box->move(100, 100);
    box->show();

    QObject::connect(box, &QPushButton::clicked, win, &QMainWindow::close);
}
