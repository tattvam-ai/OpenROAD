// SPDX-License-Identifier: BSD-3-Clause

#include <tcl.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "ord/InitOpenRoad.hh"
#include "ord/OpenRoad.hh"
// Provide a shim for ord::tclInit to satisfy GUI static linkage.
// GUI will not call this when we pass a null interp to startGui.
namespace ord {
int tclInit(Tcl_Interp* /*interp*/) { return TCL_OK; }
}

static void printGreeting()
{
  std::cout << "I am your Chip Companion. How can I help you?" << std::endl;
  std::cout << "Type 'help companion' (or 'hc') for tips, 'exit' to quit." << std::endl;
}

int main(int argc, char* argv[])
{
  // Create and initialize Tcl interpreter
  Tcl_Interp* interp = Tcl_CreateInterp();
  if (Tcl_Init(interp) != TCL_OK) {
    std::cerr << "Error: failed to initialize Tcl: "
              << Tcl_GetStringResult(interp) << std::endl;
    return EXIT_FAILURE;
  }

  // Initialize OpenROAD subsystems via public Tcl init
  // ord::tclAppInit(interp);

  printGreeting();

  std::string line;
  while (std::cout << "chipcompanion> " && std::getline(std::cin, line)) {
    // Trim
    auto ltrim = [](std::string& s) {
      size_t i = s.find_first_not_of(" \t\r\n");
      if (i == std::string::npos) {
        s.clear();
      } else if (i > 0) {
        s.erase(0, i);
      }
    };
    auto rtrim = [](std::string& s) {
      size_t i = s.find_last_not_of(" \t\r\n");
      if (i == std::string::npos) {
        s.clear();
      } else if (i + 1 < s.size()) {
        s.erase(i + 1);
      }
    };
    ltrim(line);
    rtrim(line);
    if (line.empty()) {
      continue;
    }

    // Basic commands
    if (line == "exit" || line == "quit") {
      break;
    }
    if (line == "help companion" || line == "hc") {
      printGreeting();
      continue;
    }
    // kept out to keep this companion lean and headless.

    // Before the Tcl_Eval section:
  if (line == "gui" || line == "launch_gui") {
    // Adjust the path if needed (or rely on PATH)
    const char* cmd = "/home/ubuntu/OpenROAD/build/bin/openroad -gui &";
    int rc = std::system(cmd);
    if (rc != 0) {
      std::cerr << "Failed to launch GUI; ensure the path is correct: "
                << cmd << std::endl;
    }
    continue;
  }

    // Forward to Tcl
    if (Tcl_Eval(interp, line.c_str()) != TCL_OK) {
      std::cerr << Tcl_GetStringResult(interp) << std::endl;
    } else {
      const char* result = Tcl_GetStringResult(interp);
      if (result && *result) {
        std::cout << result << std::endl;
      }
    }
  }

  // Clean exit
  // Optionally delete the Tcl interpreter (OpenROAD cleans up on process exit)
  Tcl_DeleteInterp(interp);
  return EXIT_SUCCESS;
}


