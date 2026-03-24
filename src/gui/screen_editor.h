#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QString>
#include <vector>

namespace smouse {

class ScreenEditor : public QWidget {
    Q_OBJECT

public:
    struct ScreenItem {
        QString name;
        QString client_id;
        QRect rect;  // In logical coordinates
        bool is_server;
        bool is_dragging = false;
    };

    explicit ScreenEditor(QWidget* parent = nullptr);

    void set_server_screen(int w, int h);
    void add_client_screen(const QString& name, const QString& client_id, int w, int h);
    void remove_client(const QString& client_id);
    void clear();

signals:
    void layout_changed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect logical_to_widget(const QRect& rect) const;
    QRect widget_to_logical(const QRect& rect) const;
    float scale() const;
    QPoint offset() const;

    std::vector<ScreenItem> screens_;
    int dragging_index_ = -1;
    QPoint drag_offset_;
};

} // namespace smouse
