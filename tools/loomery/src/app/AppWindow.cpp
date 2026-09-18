#include "app/AppWindow.h"

#include <QLabel>

namespace loomery
{

AppWindow::AppWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("Loomery"));
    resize(1100, 800);

    auto *placeholder = new QLabel(tr("空窗口。渲染面尚未接入。"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}

} // namespace loomery
