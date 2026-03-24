#include "screen_editor.h"

#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

namespace smouse {

ScreenEditor::ScreenEditor(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 250);
    setMouseTracking(true);

    // Default server screen
    set_server_screen(1920, 1080);
}

void ScreenEditor::set_server_screen(int w, int h) {
    // Remove existing server
    screens_.erase(
        std::remove_if(screens_.begin(), screens_.end(),
            [](const ScreenItem& s) { return s.is_server; }),
        screens_.end());

    ScreenItem server;
    server.name = "Server";
    server.client_id = "";
    server.rect = QRect(0, 0, w, h);
    server.is_server = true;
    screens_.insert(screens_.begin(), server);
    update();
}

void ScreenEditor::add_client_screen(const QString& name, const QString& client_id, int w, int h) {
    // Place to the right of the rightmost screen
    int max_right = 0;
    for (const auto& s : screens_) {
        max_right = std::max(max_right, s.rect.right());
    }

    ScreenItem item;
    item.name = name;
    item.client_id = client_id;
    item.rect = QRect(max_right + 50, 0, w, h);
    item.is_server = false;
    screens_.push_back(item);
    update();
}

void ScreenEditor::remove_client(const QString& client_id) {
    screens_.erase(
        std::remove_if(screens_.begin(), screens_.end(),
            [&](const ScreenItem& s) { return s.client_id == client_id; }),
        screens_.end());
    update();
}

void ScreenEditor::clear() {
    screens_.erase(
        std::remove_if(screens_.begin(), screens_.end(),
            [](const ScreenItem& s) { return !s.is_server; }),
        screens_.end());
    update();
}

float ScreenEditor::scale() const {
    if (screens_.empty()) return 0.1f;

    // Find bounding box of all screens
    int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
    for (const auto& s : screens_) {
        min_x = std::min(min_x, s.rect.left());
        min_y = std::min(min_y, s.rect.top());
        max_x = std::max(max_x, s.rect.right());
        max_y = std::max(max_y, s.rect.bottom());
    }

    int total_w = max_x - min_x + 200;
    int total_h = max_y - min_y + 200;

    float sx = static_cast<float>(width()) / total_w;
    float sy = static_cast<float>(height()) / total_h;
    return std::min(sx, sy);
}

QPoint ScreenEditor::offset() const {
    if (screens_.empty()) return {0, 0};

    int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
    for (const auto& s : screens_) {
        min_x = std::min(min_x, s.rect.left());
        min_y = std::min(min_y, s.rect.top());
        max_x = std::max(max_x, s.rect.right());
        max_y = std::max(max_y, s.rect.bottom());
    }

    float s = scale();
    int total_w = static_cast<int>((max_x - min_x) * s);
    int total_h = static_cast<int>((max_y - min_y) * s);

    return QPoint(
        (width() - total_w) / 2 - static_cast<int>(min_x * s),
        (height() - total_h) / 2 - static_cast<int>(min_y * s)
    );
}

QRect ScreenEditor::logical_to_widget(const QRect& rect) const {
    float s = scale();
    QPoint o = offset();
    return QRect(
        static_cast<int>(rect.x() * s) + o.x(),
        static_cast<int>(rect.y() * s) + o.y(),
        static_cast<int>(rect.width() * s),
        static_cast<int>(rect.height() * s)
    );
}

void ScreenEditor::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(40, 40, 40));

    for (const auto& screen : screens_) {
        QRect wr = logical_to_widget(screen.rect);

        // Screen fill
        if (screen.is_server) {
            painter.setBrush(QColor(50, 120, 200, 180));
        } else {
            painter.setBrush(QColor(80, 180, 80, 180));
        }

        painter.setPen(QPen(Qt::white, 2));
        painter.drawRoundedRect(wr, 4, 4);

        // Label
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(11);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(wr, Qt::AlignCenter, screen.name);

        // Resolution label
        font.setPointSize(8);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(QColor(200, 200, 200));
        QString res = QString("%1x%2").arg(screen.rect.width()).arg(screen.rect.height());
        painter.drawText(wr.adjusted(0, 0, 0, -10), Qt::AlignBottom | Qt::AlignHCenter, res);
    }
}

void ScreenEditor::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    QPoint pos = event->pos();
    for (int i = static_cast<int>(screens_.size()) - 1; i >= 0; i--) {
        QRect wr = logical_to_widget(screens_[i].rect);
        if (wr.contains(pos) && !screens_[i].is_server) {
            dragging_index_ = i;
            drag_offset_ = pos - wr.topLeft();
            screens_[i].is_dragging = true;
            setCursor(Qt::ClosedHandCursor);
            return;
        }
    }
}

void ScreenEditor::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_index_ < 0) {
        // Check hover
        QPoint pos = event->pos();
        bool over_screen = false;
        for (const auto& s : screens_) {
            if (!s.is_server && logical_to_widget(s.rect).contains(pos)) {
                over_screen = true;
                break;
            }
        }
        setCursor(over_screen ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }

    QPoint pos = event->pos();
    float s = scale();
    QPoint o = offset();

    int lx = static_cast<int>((pos.x() - drag_offset_.x() - o.x()) / s);
    int ly = static_cast<int>((pos.y() - drag_offset_.y() - o.y()) / s);

    screens_[dragging_index_].rect.moveTopLeft(QPoint(lx, ly));
    update();
}

void ScreenEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || dragging_index_ < 0) return;

    screens_[dragging_index_].is_dragging = false;
    dragging_index_ = -1;
    setCursor(Qt::ArrowCursor);
    emit layout_changed();
}

} // namespace smouse
