// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace gui {

class AcceptRejectOverlay : public QWidget
{
  Q_OBJECT

 public:
  explicit AcceptRejectOverlay(QWidget* parent = nullptr);
  ~AcceptRejectOverlay() = default;

 signals:
  void accepted();
  void rejected();

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  QPushButton* acceptButton_;
  QPushButton* rejectButton_;
  QPoint dragPosition_;

  void setupUI();
};

}  // namespace gui


