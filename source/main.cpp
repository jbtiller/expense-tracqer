#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <QApplication>
#pragma GCC diagnostic pop

#include "expense_lib.hpp"
#include "expense_tracquer.hpp"

auto main(int argc, char* argv[]) -> int {
    QApplication app(argc, argv);
    Expenses exp;
    init_gui();
    return QApplication::exec();
}
