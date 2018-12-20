/* Copyright (c) 2018 - present, VE Software Inc. All rights reserved
 *
 * This source code is licensed under Apache 2.0 License
 *  (found in the LICENSE.Apache file in the root directory)
 */

#include "base/Base.h"
#include <readline/readline.h>
#include <readline/history.h>
#include "console/CliManager.h"
#include "client/cpp/GraphClient.h"

namespace nebula {
namespace graph {

const int32_t kMaxAuthInfoRetries = 3;
const int32_t kMaxUsernameLen = 16;
const int32_t kMaxPasswordLen = 24;
const int32_t kMaxCommandLineLen = 1024;


bool CliManager::connect(const std::string& addr,
                         uint16_t port,
                         const std::string& username,
                         const std::string& password) {
    char user[kMaxUsernameLen + 1];
    char pass[kMaxPasswordLen + 1];

    strncpy(user, username.c_str(), kMaxUsernameLen);
    user[kMaxUsernameLen] = '\0';
    strncpy(pass, password.c_str(), kMaxPasswordLen);
    pass[kMaxPasswordLen] = '\0';

    // Make sure username is not empty
    for (int32_t i = 0; i < kMaxAuthInfoRetries && !strlen(user); i++) {
        // Need to interactively get the username
        std::cout << "Username: ";
        std::cin.getline(user, kMaxUsernameLen);
        user[kMaxUsernameLen] = '\0';
    }
    if (!strlen(user)) {
        std::cout << "Authentication failed: "
                     "Need a valid username to authenticate\n\n";
        return false;
    }

    // Make sure password is not empty
    for (int32_t i = 0; i < kMaxAuthInfoRetries && !strlen(pass); i++) {
        // Need to interactively get the password
        std::cout << "Password: ";
        std::cin.getline(pass, kMaxPasswordLen);
        pass[kMaxPasswordLen] = '\0';
    }
    if (!strlen(pass)) {
        std::cout << "Authentication failed: "
                     "Need a valid password\n\n";
        return false;
    }

    addr_ = addr;
    port_ = port;
    username_ = user;

    auto client = std::make_unique<GraphClient>(addr_, port_);
    cpp2::ErrorCode res = client->connect(user, pass);
    if (res == cpp2::ErrorCode::SUCCEEDED) {
        std::cerr << "\nWelcome to Nebula Graph (Version 0.1)\n\n";
        cmdProcessor_ = std::make_unique<CmdProcessor>(std::move(client));
        return true;
    } else {
        // There is an error
        std::cout << "Authentication failed\n";
        return false;
    }
}


void CliManager::batch(const std::string& filename) {
    UNUSED(filename);
}


void CliManager::loop() {
    // TODO(dutor) Detect if `stdin' is being attached to a TTY
    std::string cmd;
    loadHistory();
    while (true) {
        if (!readLine(cmd)) {
            break;
        }
        if (cmd.empty()) {
            continue;
        }
        if (!cmdProcessor_->process(cmd)) {
            break;
        }
    }
    saveHistory();
}


bool CliManager::readLine(std::string &line) {
    auto ok = true;
    char prompt[256];
    static auto color = 0u;
    ::snprintf(prompt, sizeof(prompt), "\033[1;%umnebula> \033[0m", color++ % 6 + 31);
    auto *input = ::readline(prompt);

    do {
        // EOF
        if (input == nullptr) {
            fprintf(stdout, "\n");
            ok = false;
            break;
        }
        // Empty line
        if (input[0] == '\0') {
            line.clear();
            break;
        }
        line = folly::trimWhitespace(input).str();
        if (!line.empty()) {
            // Update command history
            updateHistory(input);
        }
    } while (false);

    ::free(input);

    return ok;
}


void CliManager::updateHistory(const char *line) {
    auto **hists = ::history_list();
    auto i = 0;
    // Search in history
    for (; i < ::history_length; i++) {
        auto *hist = hists[i];
        if (::strcmp(line, hist->line) == 0) {
            break;
        }
    }
    // New command
    if (i == ::history_length) {
        ::add_history(line);
        return;
    }
    // Found in history, make it lastest
    auto *hist = hists[i];
    for (; i < ::history_length - 1; i++) {
        hists[i] = hists[i + 1];
    }
    hists[i] = hist;
}


void CliManager::saveHistory() {
    std::string histfile;
    histfile += ::getenv("HOME");
    histfile += "/.nebula_history";
    auto *file = ::fopen(histfile.c_str(), "w+");
    if (file == nullptr) {
        return;     // fail silently
    }
    auto **hists = ::history_list();
    for (auto i = 0; i < ::history_length; i++) {
        fprintf(file, "%s\n", hists[i]->line);
    }
    ::fflush(file);
    ::fclose(file);
}


void CliManager::loadHistory() {
    std::string histfile;
    histfile += ::getenv("HOME");
    histfile += "/.nebula_history";
    auto *file = ::fopen(histfile.c_str(), "r");
    if (file == nullptr) {
        return;     // fail silently
    }
    char *line = nullptr;
    size_t size = 0;
    ssize_t read = 0;
    while ((read = ::getline(&line, &size, file)) != -1) {
        line[read - 1] = '\0';  // remove the trailing newline
        updateHistory(line);
    }
    ::free(line);
    ::fclose(file);
}

}  // namespace graph
}  // namespace nebula