// SPDX-License-Identifier: BSD-3-Clause

#include "simulatedAI.h"
#include "superShellWidget.h"
#include <QTimer>

namespace gui {

SimulatedAI::SimulatedAI(QObject* parent)
    : QObject(parent)
{
    connect(this, &SimulatedAI::commandFinished, this, &SimulatedAI::processNextResponseCommand);
}

void SimulatedAI::processInput(const QString& input)
{
    QString cmd = input.toLower().trimmed();

    if (cmd == "hc" || cmd == "help companion" || cmd.startsWith("help companion ") || cmd.startsWith("hc ")) {
        QString topic;
        if (cmd == "hc" || cmd == "help companion") {
            topic = "";
        } else if (cmd.startsWith("help companion ")) {
             topic = cmd.length() > 15 ? cmd.mid(15).trimmed() : "";
        } else {
            topic = cmd.length() > 3 ? cmd.mid(3).trimmed() : "";
        }

        if (topic.isEmpty()) {
            emit responseReady("I am your personalized chip assistant! Here's what I can help you with:\n\n"
                             "1. Timing Analysis & Fixes\n"
                             "2. Power Analysis & Optimization\n"
                             "3. Design Verification\n"
                             "4. Circuit Analysis\n"
                             "Type 'help companion <topic>' or 'hc <topic>' for more details about any area.\n"
                             "Or simply describe your issue, and I'll help you fix it!");
        } else if (topic == "timing") {
            emit responseReady("Timing Analysis & Fixes:\n\n"
                             "I can help you with:\n"
                             "- Check timing violations\n"
                             "- Fix hold time violations\n"
                             "- Fix setup time violations\n"
                             "- Optimize critical paths\n\n"
                             "Let me know what you want to do :) ");
        } else if (topic == "power") {
            emit responseReady("Power Analysis & Optimization:\n\n"
                             "I can help you with:\n"
                             "1. Check power consumption\n"
                             "2. Optimize power usage\n"
                             "3. Analyze power hotspots\n"
                             "4. IR Drop Analysis\n\n"
                             "Just describe your power-related issue, and I'll help you optimize it ;) ");
        } else {
            emit responseReady("Sadly I'm still learning about " + topic + ". Please try another topic.");
        }
        return;
    }

    if (cmd.contains("could you analyze the problematic power region?")) {
        responseCommandQueue.clear();
        QTimer::singleShot(500, this, [this]() {
            responseCommandQueue.enqueue(CommandQueueItem("Analyzing IR drop...", "analyze_power_grid -net VDD", false));
            QTimer::singleShot(1000, this, [this]() {
                responseCommandQueue.enqueue(CommandQueueItem("Loading Heatmap", "", false));
                 QTimer::singleShot(300, this, [this]() {
                responseCommandQueue.enqueue(CommandQueueItem("The problematic region is as shown...", "gui::zoom_to 460 630 603 718", false));
                processNextResponseCommand();
                 });
            });
        });
        processNextResponseCommand();
        return;
    }

    if (cmd.contains("check setup") || cmd.contains("setup violations")) {
        responseCommandQueue.clear();
        responseCommandQueue.enqueue(CommandQueueItem("Checking setup timing...", "report_worst_slack -max -digits 3", true));
        QTimer::singleShot(1000, this, [this]() {
            responseCommandQueue.enqueue(CommandQueueItem("Optimizing setup timing...", "repair_timing -setup -verbose", true));
            QTimer::singleShot(1000, this, [this]() {
                responseCommandQueue.enqueue(CommandQueueItem("Verifying setup timing after fix...", "report_worst_slack -max -digits 3; detailed_placement", true));
                processNextResponseCommand();
            });
        });
        processNextResponseCommand();
        return;
    }

    if (cmd.contains("optimize the timing path even if it involves compromising the area")) {
        responseCommandQueue.clear();
        responseCommandQueue.enqueue(CommandQueueItem("Locating the problematic path for hold time violation...", "", false));
             QTimer::singleShot(1000, this, [this]() {
                responseCommandQueue.enqueue(CommandQueueItem("Suggesting hold timing...", "", false));
                QTimer::singleShot(1000, this, [this]() {
                    responseCommandQueue.enqueue(CommandQueueItem("Optimizing hold timing by inserting buffers...", "", false));

                    QTimer::singleShot(1000, this, [this]() {
                        responseCommandQueue.enqueue(CommandQueueItem("", "repair_timing -hold -allow_setup_violations -verbose", false));
                
                        QTimer::singleShot(1000, this, [this]() {
                            responseCommandQueue.enqueue(CommandQueueItem("", 
                                "gui::highlight_inst hold139; gui::highlight_inst hold140; gui::highlight_inst hold141; "
                                "gui::highlight_inst hold142; gui::highlight_inst hold143; gui::highlight_inst hold144; "
                                "gui::highlight_inst hold145; gui::highlight_inst hold146; gui::highlight_inst hold147; "
                                "gui::highlight_inst hold148; gui::highlight_inst hold149; gui::highlight_inst hold150; "
                                "gui::highlight_inst hold151; gui::highlight_inst hold152; gui::highlight_inst hold153;",
                                false));
                            QTimer::singleShot(1000, this, [this]() {
                                responseCommandQueue.enqueue(CommandQueueItem("Verifying hold timing...", "", false));
                                responseCommandQueue.enqueue(CommandQueueItem("", "report_worst_slack -min -digits 3", false));
                                responseCommandQueue.enqueue(CommandQueueItem("", "detailed_placement", false));
                                QTimer::singleShot(800, this, [this]() {
                                    responseCommandQueue.enqueue(CommandQueueItem("I have optimized hold time for you. Hold time is now clean!", "", true));
                                    processNextResponseCommand();
                                });
                            });
                        });
                    });
                });
            });
        processNextResponseCommand();
        return;
    }

    if (cmd.contains("fix all the design rule errors by making minimal modifications to the rest of the design")) {
        responseCommandQueue.clear();
        QTimer::singleShot(200, this, [this]() {
            responseCommandQueue.enqueue(CommandQueueItem("Overlapped cells found. Suggesting fixes...", "", false));
            QTimer::singleShot(1000, this, [this]() {
                responseCommandQueue.enqueue(CommandQueueItem("Start fixing: moving cells...", "place_inst -name _417_ -origin {63.94 114.24};gui::highlight_inst _417_;", false));
            
                QTimer::singleShot(1000, this, [this]() {
                    responseCommandQueue.enqueue(CommandQueueItem("", "place_inst -name _439_ -origin {107.64 76.16};gui::highlight_inst _439_;", false));
                    QTimer::singleShot(1000, this, [this]() {
                        responseCommandQueue.enqueue(CommandQueueItem("", "place_inst -name _201_ -origin {106.26 62.56};gui::highlight_inst _201_;", false));
                        QTimer::singleShot(1000, this, [this]() {
                            responseCommandQueue.enqueue(CommandQueueItem("", "place_inst -name _416_ -origin {67.16 78.88};gui::highlight_inst _416_;", false));
                            QTimer::singleShot(1000, this, [this]() {
                                responseCommandQueue.enqueue(CommandQueueItem("Suggested design is functionally equivalent to the existing design.", "", false));
                                responseCommandQueue.enqueue(CommandQueueItem("I have cleaned up DRC errors for you, we got rid of the overlapped cells!", "", true));
                                processNextResponseCommand();
                            });
                        });
                    });
                });
            });
        });
        return;
    }
   
    emit responseReady("Your command is not recognized. Try 'help companion' or 'hc' to see what I can help you with, "
                      "or describe your issue directly, and I'll guide you through the solution.");
}

void SimulatedAI::processNextResponseCommand()
{
    if (!responseCommandQueue.isEmpty()) {
        CommandQueueItem next = responseCommandQueue.dequeue();
        emit responseReady(next.response);
        if (!next.command.isEmpty()) {
            emit commandSuggested(next.command);
        }
        if (!responseCommandQueue.isEmpty()) {
            QTimer::singleShot(500, this, &SimulatedAI::processNextResponseCommand);
        } else if (next.showOverlay) {
            auto* shell = qobject_cast<SuperShellWidget*>(parent());
            if (shell) {
                shell->setRejectionScenario(SuperShellWidget::RejectionScenario::TIMING_REPAIR);
            }
            QTimer::singleShot(500, this, [this]() {
                emit showOverlay();
            });
        }
    }
}

void SimulatedAI::onCommandFinished()
{
    if (!responseCommandQueue.isEmpty()) {
        QTimer::singleShot(500, this, &SimulatedAI::processNextResponseCommand);
    }
}

}  // namespace gui


