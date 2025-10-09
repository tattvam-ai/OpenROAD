// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QObject>
#include <QString>
#include <QQueue>
#include <QPair>

namespace gui {

// Custom struct to hold command queue items
struct CommandQueueItem {
    QString response;
    QString command;
    bool showOverlay;

    CommandQueueItem(const QString& resp, const QString& cmd, bool overlay = false)
        : response(resp), command(cmd), showOverlay(overlay) {}
};

class SimulatedAI : public QObject
{
  Q_OBJECT

public:
  explicit SimulatedAI(QObject* parent = nullptr);
  ~SimulatedAI() override = default;

  // Process user input and generate simulated response
  void processInput(const QString& userInput);

signals:
  // Signal emitted when AI has a response
  void responseReady(const QString& response);
  
  // Signal emitted when AI suggests a command
  void commandSuggested(const QString& command);
  
  // Signal emitted when a command finishes (internal use)
  void commandFinished();

  // Signal emitted when a command finishes (internal use)
  void showOverlay();

public slots:
  // Slot to be called when a command finishes execution
  void onCommandFinished();

private slots:
  // Slot to process the next response-command pair in the queue
  void processNextResponseCommand();

private:
  // Helper methods for response generation
  QString generateHelpResponse();
  QString generateTimingResponse();
  QString generateDefaultResponse(const QString& userInput);
  
  // Command queue for sequential execution
  QQueue<CommandQueueItem> responseCommandQueue;
};

}  // namespace gui


