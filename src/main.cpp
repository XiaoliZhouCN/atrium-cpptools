// Loomery 入口。
//
// 这一版只做两件事：起一个 QApplication，把 AppWindow 显示出来。
// 具体的窗口内容、渲染面、文档加载都留到架构定下来之后再进来。

#include "app/AppWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("Loomery");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("Atrium");

    loomery::AppWindow window;
    window.show();

    return application.exec();
}
