// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QDockWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <memory>
#include <mutex>

#include "simulatedAI.h"
#include "tclCmdInputWidget.h"
#include "acceptRejectOverlay.h"
#include "utl/Logger.h"

namespace gui {

class SuperShellInputWidget : public TclCmdInputWidget
{
  Q_OBJECT

 public:
  explicit SuperShellInputWidget(QWidget* parent = nullptr);
  void setAIInterface(SimulatedAI* ai_interface);
  void setTclInterp(Tcl_Interp* interp,
                    bool do_init_openroad,
                    const std::function<void()>& post_or_init);

 public slots:
  void executeCommand(const QString& cmd, bool echo = true, bool silent = false) override;

 private:
  bool isAICommand(const QString& cmd);
  SimulatedAI* ai_interface_;
};

class SuperShellWidget : public QDockWidget
{
  Q_OBJECT

 public:
  explicit SuperShellWidget(QWidget* parent = nullptr);
  ~SuperShellWidget() override;

  void readSettings(QSettings* settings);
  void writeSettings(QSettings* settings);
  void setLogger(utl::Logger* logger);
  void setupTcl(Tcl_Interp* interp,
                bool interactive,
                bool do_init_openroad,
                const std::function<void()>& post_or_init);
  void setWidgetFont(const QFont& font);
  void bufferOutputs(bool state);
  void displayCustomBanner();

  enum class RejectionScenario { DRC_REPAIR, TIMING_REPAIR };
  void setRejectionScenario(RejectionScenario scenario) { rejection_scenario_ = scenario; }

 signals:
  void commandExecuted(bool is_ok);
  void commandAboutToExecute();
  void executionPaused();
  void exiting();
  void addToOutput(const QString& text, const QColor& color);

 public slots:
  void executeSilentCommand(const QString& command);
  void addResultToOutput(const QString& result, bool is_ok);
  void addCommandToOutput(const QString& cmd);
  void pause(int timeout);
  void setCommand(const QString& command);
  void handleAIResponse(const QString& response);
  void onCommandFinished(bool success);
  void setPauserToRunning();
  void resetPauser();
  void onCommandSuggested(const QString& command);

 private slots:
  void outputChanged();
  void unpause();
  void pauserClicked();
  void updatePauseTimeout();
  void addTextToOutput(const QString& text, const QColor& color);
  void flushReportBufferToOutput();

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void triggerPauseCountDown(int timeout);
  void startReportTimer();
  void addMsgToReportBuffer(const QString& text);
  void addLogToOutput(const QString& text, const QColor& color);
  void showOverlay();
  void onOverlayAccepted();
  void onOverlayRejected();

  QPlainTextEdit* output_;
  SuperShellInputWidget* input_;
  QPushButton* pauser_;
  std::unique_ptr<QTimer> pause_timer_;
  std::unique_ptr<QTimer> report_timer_;
  bool paused_;
  utl::Logger* logger_;
  bool buffer_outputs_;
  bool is_interactive_;

  template <typename Mutex>
  class GuiSink;
  std::shared_ptr<spdlog::sinks::sink> sink_;
  std::mutex reporting_;
  QString report_buffer_;
  const int max_output_line_length_ = 1000;
  static constexpr int kReportDisplayInterval = 50;

  const QColor cmd_msg_ = Qt::black;
  const QColor error_msg_ = Qt::red;
  const QColor ok_msg_ = Qt::blue;
  const QColor buffer_msg_ = QColor(0x30, 0x30, 0x30);

  bool banner_displayed_ = false;
  SimulatedAI* ai_interface_;
  QString pending_command_;
  AcceptRejectOverlay* overlay_;
  RejectionScenario rejection_scenario_ = RejectionScenario::DRC_REPAIR;
};

}  // namespace gui


