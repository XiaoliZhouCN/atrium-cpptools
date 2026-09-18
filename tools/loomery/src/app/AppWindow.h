// 主窗口。
//
// 目前是个空壳：只负责自己的存在（标题、尺寸、中央部件占位）。
// 它刻意不持有任何文档、索引或渲染状态 —— 那些东西的归属由架构决定。

#pragma once

#include <QMainWindow>

namespace loomery
{

class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget *parent = nullptr);
};

} // namespace loomery
