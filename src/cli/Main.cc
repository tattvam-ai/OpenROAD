// SPDX-License-Identifier: BSD-3-Clause

#include <tcl.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <array>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ord/InitOpenRoad.hh"
#include "ord/OpenRoad.hh"
// Provide a shim for ord::tclInit to satisfy GUI static linkage.
// GUI will not call this when we pass a null interp to startGui.
namespace ord {
int tclInit(Tcl_Interp* /*interp*/) { return TCL_OK; }
}

static void printGreeting()
{
  std::cout << "I am your AI Chip Assistant. How can I help you?" << std::endl;
  std::cout << "Type 'exit' to quit." << std::endl;
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

// Minimal Unix domain socket RPC to execute Tcl commands for the Python agent.
static std::string g_socketPath;
static int g_listenFd = -1;

static bool setupTclSocket()
{
  pid_t pid = getpid();
  g_socketPath = "/tmp/chip_assistant_" + std::to_string(pid) + ".sock";
  // Remove any stale socket file
  ::unlink(g_socketPath.c_str());

  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::perror("socket");
    return false;
  }
  // Set non-blocking listen socket
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", g_socketPath.c_str());
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::perror("bind");
    ::close(fd);
    return false;
  }
  ::chmod(g_socketPath.c_str(), 0600);
  if (::listen(fd, 4) < 0) {
    std::perror("listen");
    ::close(fd);
    return false;
  }
  g_listenFd = fd;
  ::setenv("CHIP_TCL_SOCKET", g_socketPath.c_str(), 1);
  return true;
}

static void serviceOneTclRpc(Tcl_Interp* interp)
{
  if (g_listenFd < 0) return;
  sockaddr_un caddr{};
  socklen_t clen = sizeof(caddr);
  int cfd = ::accept(g_listenFd, reinterpret_cast<sockaddr*>(&caddr), &clen);
  if (cfd < 0) {
    return; // no pending client
  }
  std::string req;
  char buf[4096];
  while (true) {
    ssize_t n = ::read(cfd, buf, sizeof(buf));
    if (n <= 0) break;
    req.append(buf, buf + n);
    if (req.find('\n') != std::string::npos) break;
    if (req.size() > 1024 * 1024) break; // 1MB cap
  }
  // Extract very simply: {"cmd":"..."}
  std::string cmd;
  auto key = req.find("\"cmd\"");
  if (key != std::string::npos) {
    auto colon = req.find(':', key);
    if (colon != std::string::npos) {
      auto q1 = req.find('"', colon + 1);
      auto q2 = (q1 == std::string::npos) ? std::string::npos : req.find('"', q1 + 1);
      if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
        cmd = req.substr(q1 + 1, q2 - q1 - 1);
      }
    }
  }
  std::string resp;
  if (cmd.empty()) {
    resp = "{\"ok\":false,\"error\":\"invalid request\"}\n";
  } else {
    int rc = Tcl_Eval(interp, cmd.c_str());
    const char* tres = Tcl_GetStringResult(interp);
    std::string payload = tres ? std::string(tres) : std::string();
    std::string esc;
    esc.reserve(payload.size() + 16);
    for (char c : payload) {
      if (c == '"' || c == '\\') esc.push_back('\\');
      if (c == '\n') { esc += "\\n"; continue; }
      if (c == '\r') { esc += "\\r"; continue; }
      esc.push_back(c);
    }
    if (rc != TCL_OK) {
      resp = std::string("{\"ok\":false,\"error\":\"") + esc + "\"}\n";
    } else {
      resp = std::string("{\"ok\":true,\"result\":\"") + esc + "\"}\n";
    }
  }
  ssize_t _wn = ::write(cfd, resp.c_str(), resp.size());
  (void)_wn;
  ::close(cfd);
}

int main(int argc, char* argv[])
{
  // Add sdc check
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    // Need 4 total args after --sdc: the flag itself + 3 file paths
    if (arg == "--sdc" && i + 4 <= argc) {
        std::string input_file = argv[i+1];
        std::string prompt_file = argv[i+2];
        std::string output_file = argv[i+3];

        std::string py = std::getenv("CHIP_PYTHON") ? std::getenv("CHIP_PYTHON") : "python3";
        std::string script = std::getenv("CHIP_AI_SCRIPT") ? std::getenv("CHIP_AI_SCRIPT") : "main.py";

        std::string cmd = py + " " + shellEscape(script) +
                          " --sdc " + shellEscape(input_file) +
                          " " + shellEscape(prompt_file) +
                          " " + shellEscape(output_file);
        int rc = std::system(cmd.c_str());
        return rc; // exit after SDC generation
    }
}


  // Create and initialize Tcl interpreter
  Tcl_Interp* interp = Tcl_CreateInterp();
  if (Tcl_Init(interp) != TCL_OK) {
    std::cerr << "Error: failed to initialize Tcl: "
              << Tcl_GetStringResult(interp) << std::endl;
    return EXIT_FAILURE;
  }

  // Initialize OpenROAD Tcl commands if available in link; temporarily disabled due to linking.
  // To enable, ensure ord::tclAppInit is in a linked library and uncomment below.
  // if (ord::tclAppInit(interp) != TCL_OK) {
  //   std::cerr << "Error: failed to initialize OpenROAD Tcl commands: "
  //             << Tcl_GetStringResult(interp) << std::endl;
  //   Tcl_DeleteInterp(interp);
  //   return EXIT_FAILURE;
  // }

  // Setup Tcl RPC socket for Python agent tools
  if (!setupTclSocket()) {
    std::cerr << "Warning: Tcl RPC socket not available; Tcl tools disabled." << std::endl;
  }

  
  printGreeting();
  std::string line;
  // Default routing mode: Tcl unless CHIP_AI_DEFAULT is set ("1", "true")
  bool ai_default = false;
  if (const char* env = std::getenv("CHIP_AI_DEFAULT")) {
    std::string val = env;
    for (auto& c : val) c = std::tolower(c);
    ai_default = (val == "1" || val == "true" || val == "yes");
  }
  // Optional: if Tcl eval fails, fall back to AI when CHIP_AI_FALLBACK is set
  bool ai_fallback = false;
  if (const char* env = std::getenv("CHIP_AI_FALLBACK")) {
    std::string val = env;
    for (auto& c : val) c = std::tolower(c);
    ai_fallback = (val == "1" || val == "true" || val == "yes");
  }
  while (std::cout << "chipaiassistant> " && std::getline(std::cin, line)) {
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
    // Service at most one pending Tcl RPC per loop
    serviceOneTclRpc(interp);

    // Runtime mode switches
    if (line == "mode ai") {
      ai_default = true;
      std::cout << "Mode switched: AI is now default for non-builtins." << std::endl;
      continue;
    }
    if (line == "mode tcl") {
      ai_default = false;
      std::cout << "Mode switched: Tcl is now default for non-builtins." << std::endl;
      continue;
    }
    // kept out to keep assista lean and headless.

    // Before the Tcl_Eval section:
  if (line == "gui" || line == "launch_gui") {
    // Launch OpenROAD GUI using OPENROAD_BIN if set, otherwise rely on PATH
    const char* env_bin = std::getenv("OPENROAD_BIN");
    std::string gui_cmd = (env_bin && std::string(env_bin).size())
                          ? (std::string(env_bin) + " -gui &")
                          : std::string("openroad -gui &");
    int rc = std::system(gui_cmd.c_str());
    if (rc != 0) {
      std::cerr << "Failed to launch GUI; tried: " << gui_cmd
                << ". Set OPENROAD_BIN or ensure 'openroad' is in PATH." << std::endl;
    }
    continue;
  }

    
    // Route non-builtins per current mode
    if (ai_default) {
      std::string ai_out = runPythonAI(line);
      std::cout << ai_out << std::endl;
      continue;
    }

    // Forward to Tcl; optionally fall back to AI on error
    if (Tcl_Eval(interp, line.c_str()) != TCL_OK) {
      const char* err = Tcl_GetStringResult(interp);
      if (ai_fallback) {
        std::string ai_out = runPythonAI(line);
        std::cout << ai_out << std::endl;
        continue;
      }
      if (err && *err) {
        std::cerr << err << std::endl;
      } else {
        std::cerr << "Tcl error" << std::endl;
      }
    } else {
      const char* result = Tcl_GetStringResult(interp);
      if (result && *result) {
        std::cout << result << std::endl;
      }
    }
  }

  // Clean exit
  // Optionally delete the Tcl interpreter (OpenROAD cleans up on process exit)
  if (g_listenFd >= 0) {
    ::close(g_listenFd);
    if (!g_socketPath.empty()) {
      ::unlink(g_socketPath.c_str());
    }
  }
  Tcl_DeleteInterp(interp);
  return EXIT_SUCCESS;
}


