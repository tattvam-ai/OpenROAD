// SPDX-License-Identifier: BSD-3-Clause

#include "acceptRejectOverlay.h"
#include <new>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>

namespace gui {

AcceptRejectOverlay::AcceptRejectOverlay(QWidget* parent)
    : QWidget(parent)
{
    // Set window flags for frameless, always-on-top window
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    setupUI();
}

void AcceptRejectOverlay::setupUI()
{
    // Create buttons
    acceptButton_ = new QPushButton("Accept", this);
    rejectButton_ = new QPushButton("Reject", this);

    // Style buttons
    QString buttonStyle = R"(
        QPushButton {
            background-color: #4CAF50;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-size: 14px;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #45a049;
        }
        QPushButton:pressed {
            background-color: #3d8b40;
        }
    )";

    acceptButton_->setStyleSheet(buttonStyle);
    rejectButton_->setStyleSheet(buttonStyle.replace("#4CAF50", "#f44336")
                                              .replace("#45a049", "#e53935")
                                              .replace("#3d8b40", "#d32f2f"));

    // Connect button signals
    connect(acceptButton_, &QPushButton::clicked, this, &AcceptRejectOverlay::accepted);
    connect(rejectButton_, &QPushButton::clicked, this, &AcceptRejectOverlay::rejected);

    // Create layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(acceptButton_);
    buttonLayout->addWidget(rejectButton_);
    buttonLayout->setSpacing(10);
    buttonLayout->setContentsMargins(10, 10, 10, 10);

    setLayout(buttonLayout);

    // Set initial size
    resize(250, 60);
}

void AcceptRejectOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragPosition_ = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void AcceptRejectOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - dragPosition_);
        event->accept();
    }
}

void AcceptRejectOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw semi-transparent background
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    // Draw rounded rectangle background
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    painter.fillPath(path, QColor(255, 255, 255, 230));
}

} // namespace gui 


