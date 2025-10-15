// SPDX-License-Identifier: BSD-3-Clause

#include <tcl.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <array>
#include <cstdio>

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

// Shell escape the string
static std::string shellEscape(const std::string& s)
{
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

static std::string runPythonAI(const std::string& query)
{
  // Get the python path
  const char* py = std::getenv("CHIP_PYTHON");
  if (!py || std::string(py).empty()) {
    py = "python3";
  }
  // Get the script path
  const char* script = std::getenv("CHIP_AI_SCRIPT");
  // If the script path is not set, return an error
  if (!script || std::string(script).empty()) {
    return "[AI ERROR] Set CHIP_AI_SCRIPT to your main.py path.";
  }
  // Create the command that safely injects the script path and query into the python script, merges stderr to stdout
  std::string cmd = std::string(py) + " " + shellEscape(script) + " --query " + shellEscape(query) + " 2>&1";


  std::array<char, 4096> buf{};
  // Accumulates all the text read from the subprocess into one string
  std::string out;
  // Starts a shell command (in cmd) and opens a read-only pipe to the child process’s stdout 
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return "[AI ERROR] Failed to spawn Python.";
  }
  while (fgets(buf.data(), buf.size(), pipe)) {
    out.append(buf.data());
  }
  pclose(pipe);
  return out;
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
    if (line.rfind("ai", 0) == 0) {
      std::string prompt;
      if (line == "ai") {
        std::cout << "question> ";
        std::getline(std::cin, prompt);
      } else if (line.size() > 3 && line[2] == ' ') {
        prompt = line.substr(3);
      }
      if (prompt.empty()) {
        std::cout << "[AI] Empty prompt." << std::endl;
        continue;
      }
      std::string ai_out = runPythonAI(prompt);
      std::cout << ai_out << std::endl;
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

    // Default: route any non-built-in input to AI (Tcl routing temporarily disabled)
    {
      std::string ai_out = runPythonAI(line);
      std::cout << ai_out << std::endl;
      continue;
    }

    /*
    // Forward to Tcl (disabled for now while focusing on AI prompts)
    if (Tcl_Eval(interp, line.c_str()) != TCL_OK) {
      std::cerr << Tcl_GetStringResult(interp) << std::endl;
    } else {
      const char* result = Tcl_GetStringResult(interp);
      if (result && *result) {
        std::cout << result << std::endl;
      }
    }
    */
  }

  // Clean exit
  // Optionally delete the Tcl interpreter (OpenROAD cleans up on process exit)
  Tcl_DeleteInterp(interp);
  return EXIT_SUCCESS;
}


