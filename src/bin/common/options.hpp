/*
    +--------------------------------------------------------------------+
    | libmemcached - C/C++ Client Library for memcached                  |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted under the terms of the BSD license.    |
    | You should have received a copy of the license in a bundled file   |
    | named LICENSE; in case you did not receive a copy you can review   |
    | the terms online at: https://opensource.org/licenses/BSD-3-Clause  |
    +--------------------------------------------------------------------+
    | Copyright (c) 2006-2014 Brian Aker   https://datadifferential.com/ |
    | Copyright (c) 2020 Michael Wallner   <mike@php.net>                |
    +--------------------------------------------------------------------+
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <climits>
#include <functional>
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

#include "libmemcached/common.h"

class client_options {
public:

  struct extended_option {
    option opt;
    std::string help;
    std::function<bool(client_options &, extended_option &)> parse;
    std::function<bool(const client_options &, const extended_option &, memcached_st *)> apply;
    const char *arg;
    bool set;
  };

  std::vector<extended_option> options;
  std::vector<extended_option> defaults;

  const char *prog_name;
  const char *prog_vers;
  const char *prog_desc;
  const char *prog_argp;

  client_options(const char *prg, const char *ver, const char *dsc, const char *arg = nullptr)
  : options{}
  , defaults{}
  , prog_name{prg}
  , prog_vers{ver}
  , prog_desc{dsc}
  , prog_argp{arg}
  {

    def("help", 'h', no_argument, "Print this help.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *) {
      if (ext.set) {
        opt.print_help();
        exit(EXIT_SUCCESS);
      }
      return true;
    };
    def("version", 'V', no_argument, "Print program version.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *) {
      if (ext.set) {
        opt.print_version();
        exit(EXIT_SUCCESS);
      }
      return true;
    };

    def("verbose", 'v', no_argument, "Print more informational output.")
        .parse = [](client_options &opt, extended_option &) {
      opt.unset("quiet");
      return true;
    };
    def("debug", 'd', no_argument, "Print output useful only for debugging.")
        .parse = [](client_options &opt, extended_option &) {
      opt.set("verbose");
      opt.unset("quiet");
      return true;
    };
    def("quiet", 'q', no_argument, "Print no output, not even errors.")
        .parse = [](client_options &opt, extended_option &) {
      opt.unset("verbose");
      opt.unset("debug");
      return true;
    };

    def("password", 'p', required_argument, "SASL password.");
    def("username", 'u', required_argument, "SASL username.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      if (auto username = ext.arg) {
        if (!LIBMEMCACHED_WITH_SASL_SUPPORT) {
          if (!opt.isset("quiet")) {
            std::cerr
                << "SASL username was supplied, but binary was not built with SASL support.\n";
            return false;
          }
        }
        if (memc) {
          if (MEMCACHED_SUCCESS
              != memcached_set_sasl_auth_data(memc, username, opt.argof("password"))) {
            if (!opt.isset("quiet")) {
              std::cerr << memcached_last_error_message(memc);
            }
            return false;
          }
        }
      }
      return true;
    };

    def("binary", 'b', no_argument, "Use the binary memcached protocol.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      if (MEMCACHED_SUCCESS != memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, ext.set)) {
        if(!opt.isset("quiet")) {
          std::cerr << memcached_last_error_message(memc);
        }
        return false;
      }
      return true;
    };
    def("buffer", 'B', no_argument, "Buffer requests.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      if (MEMCACHED_SUCCESS != memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_BUFFER_REQUESTS, ext.set)) {
        if(!opt.isset("quiet")) {
          std::cerr << memcached_last_error_message(memc);
        }
        return false;
      }
      return true;
    };
    def("non-blocking", 'n', no_argument, "Use non-blocking connections.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      if (MEMCACHED_SUCCESS != memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NO_BLOCK, ext.set)) {
        if(!opt.isset("quiet")) {
          std::cerr << memcached_last_error_message(memc);
        }
        return false;
      }
      return true;
    };
    def("tcp-nodelay", 'N', no_argument, "Disable Nagle's algorithm.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      if (MEMCACHED_SUCCESS != memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_TCP_NODELAY, ext.set)) {
        if(!opt.isset("quiet")) {
          std::cerr << memcached_last_error_message(memc);
        }
        return false;
      }
      return true;
    };
    def("servers", 's', required_argument, "List of servers to connect to.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      auto servers = ext.arg;
      if (!servers) {
        if (opt.isset("verbose")) {
          std::cerr << "Checking environment for a server list in MEMCACHED_SERVERS.\n";
        }
        servers = getenv("MEMCACHED_SERVERS");
        if (!servers || !*servers) {
          if (!opt.isset("quiet")) {
            std::cerr << "No servers provided.\n";
          }
          return false;
        }
      }

      auto server_list = memcached_servers_parse(servers);
      if (!server_list || !memcached_server_list_count(server_list)) {
        if (!opt.isset("quiet")) {
          std::cerr << "Invalid server list provided: '" << servers << "'\n";
        }
        if (server_list) {
          memcached_server_list_free(server_list);
        }
        return false;
      }

      if (MEMCACHED_SUCCESS != memcached_server_push(memc, server_list)) {
        if (!opt.isset("quiet")) {
          std::cerr << memcached_last_error_message(memc);
        }
        memcached_server_list_free(server_list);
        return false;
      }
      memcached_server_list_free(server_list);
      return true;
    };
    def("hash", 'H', required_argument, "Key hashing method.")
        .apply = [](const client_options &opt, const extended_option &ext, memcached_st *memc) {
      if (ext.set) {
        std::string hash_wanted{ext.arg};
        memcached_hash_t hash = MEMCACHED_HASH_DEFAULT;

        std::transform(hash_wanted.begin(), hash_wanted.end(), hash_wanted.begin(), ::toupper);

        if (opt.isset("verbose")) {
          std::cerr << "Checking for hash '" << hash_wanted << "'.\n";
        }
        for (int h = MEMCACHED_HASH_DEFAULT; h < MEMCACHED_HASH_MAX; ++h) {
          auto hash_type = static_cast<memcached_hash_t>(h);
          std::string hash_string{libmemcached_string_hash(hash_type)};

          if (hash_wanted.length() == hash_string.length()) {
            auto ci = std::equal(hash_string.begin(), hash_string.end(), hash_wanted.begin(),
                                 [](int a, int b) { return ::toupper(a) == b; });
            if (ci) {
              hash = hash_type;
              break;
            }
          }
        }
        if (hash == MEMCACHED_HASH_DEFAULT) {
          if (!opt.isset("quiet")) {
            std::cerr << "Could not find hash '" << hash_wanted << "'.\n";
          }
        }
        if (MEMCACHED_SUCCESS != memcached_behavior_set_key_hash(memc, hash)) {
          if (!opt.isset("quiet")) {
            std::cerr << memcached_last_error_message(memc);
          }
          return false;
        }
      }
      return true;
    };
  }

  extended_option &def(option opt, std::string help) {
    defaults.emplace_back(extended_option{opt, std::move(help), {}, {}, nullptr, false});
    return defaults.back();
  }

  extended_option &def(const char *name, char flag, int has_arg, const char *help) {
    return def(option{name, has_arg, nullptr, flag}, help);
  }

  extended_option &add(extended_option ext) {
    options.emplace_back(std::move(ext));
    return options.back();
  }

  extended_option &add(option opt, std::string help) {
    options.emplace_back(extended_option{opt, std::move(help), nullptr, nullptr, nullptr, false});
    return options.back();
  }

  extended_option &add(const char *name, char flag, int has_arg, const char *help) {
    return add(option{name, has_arg, nullptr, flag}, help);
  }

  extended_option &get(const std::string &name) {
    // UB if not found
    return *std::find_if(options.begin(), options.end(), [&name](extended_option &ext) {
      return ext.opt.name && ext.opt.name == name;
    });
  }
  extended_option &get(int c) {
    // UB if not found
    return *std::find_if(options.begin(), options.end(), [c](extended_option &ext) {
      return ext.opt.val == c || (c == 1 && ext.opt.val == '-');
    });
  }

  const extended_option &get(const std::string &name) const {
    for (const auto &ext_opt : options) {
      if (ext_opt.opt.name && ext_opt.opt.name == name) {
        return ext_opt;
      }
    }
    return null_ext_opt;
  }
  const extended_option &get(int c) const {
    for (const auto &ext_opt : options) {
      if (ext_opt.opt.val == c) {
        return ext_opt;
      } else if (c == 1 && ext_opt.opt.val == '-') {
        // GNU argv extension
        return ext_opt;
      }
    }
    return null_ext_opt;
  }

  bool isset(const std::string &name) const {
    return get(name).set;
  }
  bool isset(int c) const {
    return get(c).set;
  }

  void unset(const std::string &name) {
    auto &opt = get(name);
    opt.set = false;
    opt.arg = nullptr;
  }
  void unset(int c) {
    auto &opt = get(c);
    opt.set = false;
    opt.arg = nullptr;
  }

  void set(const std::string &name, bool set_ = true, const char *optarg_ = nullptr) {
    auto &opt = get(name);
    opt.set = set_;
    opt.arg = optarg_;
  }
  void set(int c, bool set_ = true, const char *optarg_ = nullptr) {
    auto &opt = get(c);
    opt.set = set_;
    opt.arg = optarg_;
  }

  const char *argof(const std::string &name) const {
    return get(name).arg;
  }
  const char *argof(int c) const {
    return get(c).arg;
  }

  const extended_option &operator[](const std::string &name) const {
    return get(name);
  }
  const extended_option &operator[](int c) const {
    return get(c);
  }

  void print_version() const;
  void print_help() const;

  bool parse(int argc, char *argv[], char ***argp = nullptr);
  bool apply(memcached_st *memc);

private:
  static option null_opt;
  static const extended_option null_ext_opt;
};
