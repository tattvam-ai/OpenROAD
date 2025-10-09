// SPDX-License-Identifier: BSD-3-Clause

#include "superShellWidget.h"
#include "mainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <mutex>

#include "gui/gui.h"
#include "spdlog/formatter.h"
#include "spdlog/sinks/base_sink.h"
#include "tclCmdInputWidget.h"

namespace gui {

// SuperShellInputWidget
SuperShellInputWidget::SuperShellInputWidget(QWidget* parent)
    : TclCmdInputWidget(parent), ai_interface_(nullptr)
{
}

void SuperShellInputWidget::setTclInterp(Tcl_Interp* interp,
                                         bool do_init_openroad,
                                         const std::function<void()>& post_or_init)
{
  TclCmdInputWidget::setTclInterp(interp, do_init_openroad, post_or_init);

  if (interp != nullptr) {
    const char* setup_cmd = R"(
      namespace eval ::supershell {}
    )";
    Tcl_Eval(interp, setup_cmd);
  }
}

void SuperShellInputWidget::setAIInterface(SimulatedAI* ai_interface)
{
  ai_interface_ = ai_interface;
}

bool SuperShellInputWidget::isAICommand(const QString& cmd)
{
  const QString c = cmd.toLower().trimmed();
  return c == "hc" || c == "help companion" || c.startsWith("help companion ")
         || (c.startsWith("hc ") && c.length() > 3) || c.contains("check timing")
         || c.contains("check setup") || c.contains("check setup violations")
         || c.contains("analyze the problematic power region?")
         || c.contains("optimize the timing path even if it involves compromising the area")
         || c.contains("fix all the design rule errors by making minimal modifications to the rest of the design");
}

void SuperShellInputWidget::executeCommand(const QString& cmd, bool echo, bool silent)
{
  if (cmd.isEmpty()) {
    return;
  }

  if (ai_interface_ && isAICommand(cmd)) {
    ai_interface_->processInput(cmd);
    clear();
    return;
  }

  emit commandAboutToExecute();
  TclCmdInputWidget::executeCommand(cmd, echo, silent);
  emit commandFinishedExecuting(true);
}

// SuperShellWidget
SuperShellWidget::SuperShellWidget(QWidget* parent)
    : QDockWidget("Chip Companion", parent),
      output_(new QPlainTextEdit(this)),
      input_(new SuperShellInputWidget(this)),
      pauser_(new QPushButton("Idle", this)),
      pause_timer_(std::make_unique<QTimer>()),
      report_timer_(std::make_unique<QTimer>()),
      paused_(false),
      logger_(nullptr),
      buffer_outputs_(false),
      is_interactive_(true),
      sink_(nullptr),
      banner_displayed_(false),
      ai_interface_(new SimulatedAI(this)),
      overlay_(new AcceptRejectOverlay(this))
{
  setObjectName("chip_companion");

  output_->setReadOnly(true);
  pauser_->setEnabled(false);
  pauser_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
  pause_timer_->setSingleShot(true);

  QHBoxLayout* inner_layout = new QHBoxLayout;
  inner_layout->addWidget(pauser_);
  inner_layout->addWidget(input_);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->addWidget(output_, 1);
  layout->addLayout(inner_layout);

  QWidget* container = new QWidget(this);
  container->setLayout(layout);

  connect(input_, &SuperShellInputWidget::textChanged, this, &SuperShellWidget::outputChanged);
  connect(input_, &SuperShellInputWidget::exiting, this, &SuperShellWidget::exiting);
  connect(input_, &SuperShellInputWidget::commandAboutToExecute, this, &SuperShellWidget::commandAboutToExecute);
  connect(input_, &SuperShellInputWidget::commandAboutToExecute, this, &SuperShellWidget::setPauserToRunning);
  connect(input_, &SuperShellInputWidget::addCommandToOutput, this, &SuperShellWidget::addCommandToOutput);
  connect(input_, &SuperShellInputWidget::addResultToOutput, this, &SuperShellWidget::addResultToOutput);
  connect(input_, &SuperShellInputWidget::addTextToOutput, this, &SuperShellWidget::addTextToOutput, Qt::QueuedConnection);
  connect(input_, &SuperShellInputWidget::commandFinishedExecuting, this, &SuperShellWidget::resetPauser);
  connect(input_, &SuperShellInputWidget::commandFinishedExecuting, this, &SuperShellWidget::commandExecuted);
  connect(input_, &SuperShellInputWidget::commandFinishedExecuting, this, &SuperShellWidget::flushReportBufferToOutput);
  connect(output_, &QPlainTextEdit::textChanged, this, &SuperShellWidget::outputChanged);
  connect(pauser_, &QPushButton::pressed, this, &SuperShellWidget::pauserClicked);
  connect(pause_timer_.get(), &QTimer::timeout, this, &SuperShellWidget::unpause);
  connect(report_timer_.get(), &QTimer::timeout, this, &SuperShellWidget::flushReportBufferToOutput);
  connect(this, &SuperShellWidget::addToOutput, this, &SuperShellWidget::addTextToOutput, Qt::QueuedConnection);

  connect(ai_interface_, &SimulatedAI::responseReady, this, &SuperShellWidget::handleAIResponse);
  connect(ai_interface_, &SimulatedAI::commandSuggested, this, &SuperShellWidget::onCommandSuggested);
  connect(ai_interface_, &SimulatedAI::showOverlay, this, &SuperShellWidget::showOverlay);

  connect(overlay_, &AcceptRejectOverlay::accepted, this, &SuperShellWidget::onOverlayAccepted);
  connect(overlay_, &AcceptRejectOverlay::rejected, this, &SuperShellWidget::onOverlayRejected);

  setWidget(container);
  input_->setAIInterface(ai_interface_);

  connect(input_, &SuperShellInputWidget::commandFinishedExecuting, this, [this](bool success) {
    this->onCommandFinished(success);
    ai_interface_->onCommandFinished();
  });
}

void SuperShellWidget::flushReportBufferToOutput()
{
  std::unique_lock guard(reporting_, std::try_to_lock);
  if (!guard.owns_lock()) {
    QTimer::singleShot(kReportDisplayInterval, this, &SuperShellWidget::flushReportBufferToOutput);
    return;
  }
  if (report_buffer_.isEmpty()) {
    return;
  }
  addTextToOutput(report_buffer_, buffer_msg_);
  report_buffer_.clear();
}

SuperShellWidget::~SuperShellWidget()
{
  disconnect(input_, &SuperShellInputWidget::textChanged, this, &SuperShellWidget::outputChanged);
  if (logger_ != nullptr) {
    logger_->removeSink(sink_);
  }
}

void SuperShellWidget::setupTcl(Tcl_Interp* interp,
                                bool interactive,
                                bool do_init_openroad,
                                const std::function<void()>& post_or_init)
{
  is_interactive_ = interactive;
  // Keep SuperShell Tcl setup minimal in GUI to avoid tclreadline conflicts.
  Tcl_Eval(interp, "namespace eval ::chipcompanion {}");
  input_->setTclInterp(interp, do_init_openroad, post_or_init);
}

void SuperShellWidget::executeSilentCommand(const QString& command)
{
  input_->executeCommand(command, false, true);
}

void SuperShellWidget::setPauserToRunning()
{
  pauser_->setText("Running");
  pauser_->setStyleSheet("background-color: red");
}

void SuperShellWidget::resetPauser()
{
  pauser_->setText("Idle");
  pauser_->setStyleSheet("");
}

void SuperShellWidget::addCommandToOutput(const QString& cmd)
{
  const QString first_line_prefix = ">>> ";
  const QString continue_line_prefix = "... ";
  QString command = first_line_prefix + cmd;
  command.replace("\n", "\n" + continue_line_prefix);
  addToOutput(command, cmd_msg_);
}

void SuperShellWidget::addResultToOutput(const QString& result, bool is_ok)
{
  if (result.isEmpty()) {
    return;
  }
  if (is_ok) {
    addToOutput(result, ok_msg_);
  } else {
    try {
      auto msg = result.toStdString();
      if (msg.find(TclCmdInputWidget::kExitString) == std::string::npos) {
        logger_->error(utl::GUI, 71, msg);
      }
    } catch (const std::runtime_error& /*e*/) {
      if (!is_interactive_) {
        throw;
      }
    }
  }
}

void SuperShellWidget::addLogToOutput(const QString& text, const QColor& color)
{
  addToOutput(text, color);
}

void SuperShellWidget::startReportTimer()
{
  report_timer_->start(kReportDisplayInterval);
}

void SuperShellWidget::addMsgToReportBuffer(const QString& text)
{
  std::lock_guard guard(reporting_);
  report_buffer_ += text;
}

void SuperShellWidget::addTextToOutput(const QString& text, const QColor& color)
{
  output_->moveCursor(QTextCursor::End);
  QString output_text = text;
  if (text.endsWith('\n')) {
    output_text.chop(1);
  }
  QStringList output;
  for (QString& text_line : output_text.split('\n')) {
    if (text_line.size() > max_output_line_length_) {
      text_line = text_line.left(max_output_line_length_ - 3);
      text_line += "...";
    }
    output.append(text_line.toHtmlEscaped());
  }
  QString html = "<p style=\"color:" + color.name() + ";white-space: pre;\">";
  html += output.join("<br>");
  html += "</p>";
  output_->appendHtml(html);
}

void SuperShellWidget::readSettings(QSettings* settings)
{
  settings->beginGroup(objectName());
  input_->readSettings(settings);
  settings->endGroup();
}

void SuperShellWidget::writeSettings(QSettings* settings)
{
  settings->beginGroup(objectName());
  input_->writeSettings(settings);
  settings->endGroup();
}

void SuperShellWidget::pause(int timeout)
{
  QString prior_text = pauser_->text();
  bool prior_enable = pauser_->isEnabled();
  QString prior_style = pauser_->styleSheet();
  pauser_->setText("Continue");
  pauser_->setStyleSheet("background-color: yellow");
  pauser_->setEnabled(true);
  paused_ = true;

  emit executionPaused();
  input_->setReadOnly(true);
  triggerPauseCountDown(timeout);
  while (paused_) {
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
  }
  pauser_->setText(prior_text);
  pauser_->setStyleSheet(prior_style);
  pauser_->setEnabled(prior_enable);
  input_->setReadOnly(false);
  emit commandAboutToExecute();
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void SuperShellWidget::setCommand(const QString& command)
{
  input_->setText(command);
}

void SuperShellWidget::unpause()
{
  paused_ = false;
}

void SuperShellWidget::triggerPauseCountDown(int timeout)
{
  if (timeout == 0) {
    return;
  }
  pause_timer_->setInterval(timeout);
  pause_timer_->start();
  QTimer::singleShot(timeout, this, &SuperShellWidget::updatePauseTimeout);
  updatePauseTimeout();
}

void SuperShellWidget::updatePauseTimeout()
{
  if (!paused_) {
    return;
  }
  const int one_second = 1000;
  if (pause_timer_->isActive()) {
    const int seconds = pause_timer_->remainingTime() / one_second;
    pauser_->setText("Continue (" + QString::number(seconds) + "s)");
    QTimer::singleShot(one_second, this, &SuperShellWidget::updatePauseTimeout);
  }
}

void SuperShellWidget::pauserClicked()
{
  pause_timer_->stop();
  paused_ = false;
}

void SuperShellWidget::outputChanged()
{
  output_->ensureCursorVisible();
  if (!buffer_outputs_) {
    buffer_outputs_ = true;
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    buffer_outputs_ = false;
  }
}

void SuperShellWidget::bufferOutputs(bool state)
{
  buffer_outputs_ = state;
}

void SuperShellWidget::resizeEvent(QResizeEvent* event)
{
  input_->setMaxHeight(event->size().height() - output_->sizeHint().height());
  QDockWidget::resizeEvent(event);
}

void SuperShellWidget::setWidgetFont(const QFont& font)
{
  QDockWidget::setFont(font);
  output_->setFont(font);
  input_->setWidgetFont(font);
}

void SuperShellWidget::displayCustomBanner()
{
  if (banner_displayed_) {
    return;
  }
  const QString banner =
      "Welcome to Chip Companion!\nType 'help companion' for available commands.\n----------------------------------------";
  addTextToOutput(banner, QColor(255, 165, 0));
  banner_displayed_ = true;
}

template <typename Mutex>
class SuperShellWidget::GuiSink : public spdlog::sinks::base_sink<Mutex>
{
 public:
  explicit GuiSink(SuperShellWidget* widget) : widget_(widget) {}

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    // Filter out splash/license lines specifically in Chip Companion
    const std::string raw(msg.payload.data(), msg.payload.size());
    if (raw.rfind("OpenROAD v", 0) == 0
        || raw.find("Features included") != std::string::npos
        || raw.find("This program is licensed") != std::string::npos
        || raw.find("Components of this program") != std::string::npos) {
      return;
    }

    spdlog::memory_buf_t formatted;
    mutex_.lock();
    this->formatter_->format(msg, formatted);
    mutex_.unlock();
    const QString formatted_msg = QString::fromStdString(std::string(formatted.data(), formatted.size()));

    if (msg.level == spdlog::level::level_enum::off) {
      widget_->addMsgToReportBuffer(formatted_msg);
      if (QThread::currentThread() == widget_->thread()) {
        if (!widget_->report_timer_->isActive()) {
          widget_->startReportTimer();
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }
    } else {
      const QColor& msg_color = msg.level >= spdlog::level::level_enum::err ? widget_->error_msg_ : widget_->buffer_msg_;
      widget_->flushReportBufferToOutput();
      widget_->addLogToOutput(formatted_msg, msg_color);
    }

    if (QThread::currentThread() == widget_->thread()) {
      if (!formatted_msg.contains("XcbConnection")) {
        QCoreApplication::sendPostedEvents(widget_);
      }
    }
  }

  void flush_() override {}

 private:
  SuperShellWidget* widget_;
  std::mutex mutex_;
};

void SuperShellWidget::setLogger(utl::Logger* logger)
{
  sink_ = std::make_shared<GuiSink<spdlog::details::null_mutex>>(this);
  logger_ = logger;
  logger->addSink(sink_);
  displayCustomBanner();
}

void SuperShellWidget::handleAIResponse(const QString& response)
{
  if (response.contains("I have optimized hold time for you") || response.contains("Hold time is now clean") || response.contains("DRC clean!")) {
    addTextToOutput("\n" + response, QColor(0, 255, 0));
  } else {
    addTextToOutput("\n" + response, QColor(255, 165, 0));
  }
}

void SuperShellWidget::onCommandSuggested(const QString& command)
{
  if (!command.isEmpty()) {
    input_->TclCmdInputWidget::executeCommand(command, true, false);
    input_->clear();
  }
}

void SuperShellWidget::showOverlay()
{
  auto* main_window = qobject_cast<gui::MainWindow*>(parent());
  if (!main_window) {
    return;
  }
  auto* central_widget = main_window->centralWidget();
  if (!central_widget) {
    return;
  }
  QPoint central_pos = central_widget->mapToGlobal(QPoint(0, 0));
  QSize central_size = central_widget->size();
  int overlay_x = central_pos.x() + (central_size.width() - overlay_->width()) / 2;
  int overlay_y = central_pos.y() + central_size.height() - overlay_->height() - 20;
  overlay_->move(overlay_x, overlay_y);
  overlay_->show();
}

void SuperShellWidget::onOverlayAccepted()
{
  addTextToOutput("\nDesign changes accepted.", QColor(0, 255, 0));
  if (!pending_command_.isEmpty()) {
    input_->TclCmdInputWidget::executeCommand(pending_command_, true, false);
    input_->clear();
    pending_command_.clear();
  }
  overlay_->hide();
}

void SuperShellWidget::onOverlayRejected()
{
  addTextToOutput("\nDesign changes rejected. Changes reverted...", QColor(255, 0, 0));
  switch (rejection_scenario_) {
    case RejectionScenario::DRC_REPAIR:
      input_->TclCmdInputWidget::executeCommand("# revert DRC actions", false, true);
      break;
    case RejectionScenario::TIMING_REPAIR:
      input_->TclCmdInputWidget::executeCommand("# revert timing actions", false, true);
      break;
  }
  input_->clear();
  pending_command_.clear();
  overlay_->hide();
  ai_interface_->onCommandFinished();
}

void SuperShellWidget::onCommandFinished(bool /*success*/)
{
}

}  // namespace gui


