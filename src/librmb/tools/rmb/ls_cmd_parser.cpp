// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include "ls_cmd_parser.h"

#include <iostream>
#include <string>

namespace librmb {

CmdLineParser::~CmdLineParser() {
  for (auto &it : predicates) {
    delete it.second;
  }
}

Predicate *CmdLineParser::create_predicate(const std::string &_ls_value) {
  Predicate *p = new Predicate();

  size_t pos = _ls_value.find("=");
  pos = (pos == std::string::npos) ? _ls_value.find(">") : pos;
  pos = (pos == std::string::npos) ? _ls_value.find("<") : pos;

  p->key = _ls_value.substr(0, pos);
  p->op = _ls_value[pos];
  p->value = _ls_value.substr(pos + 1, _ls_value.length());
  p->valid = true;

  this->keys += p->key + " ";
  // std::cout << " predicate: key " << p->key << " op " << p->op << " value " << p->value << std::endl;
  return p;
}

void CmdLineParser::set_output_dir(const std::string out) {
  if (out.length() > 0 && out.at(0) == '~') {
    // Convert tilde to $HOME path (if exists)
    char outpath[PATH_MAX];
    char *home = getenv("HOME");
    if (home != NULL) {
      snprintf(outpath, sizeof(outpath), "%s", home);
      out_dir.clear();
      out_dir.append(outpath);
      if (out.length() > 1 && out.at(1) != '/') {
        out_dir.append(1, '/');
      }
      out_dir.append(out, 1, std::string::npos);
    } else {
      this->out_dir = out;
    }

  } else {
    this->out_dir = out;
  }
}

bool CmdLineParser::parse_ls_string() {
  std::string pred_sep = ";";

  size_t pos = ls_value.find(pred_sep);
  if (pos == std::string::npos) {
    // single condition.
    Predicate *p = create_predicate(ls_value);
    if (p->valid) {
      predicates[p->key] = p;
    }
    return p->valid;

  } else {
    int offset = 0;
    std::string tmp = ls_value;
    while (pos != std::string::npos) {
      tmp = tmp.substr(0, pos);

      Predicate *p = create_predicate(tmp);
      if (p->valid) {
        predicates[p->key] = p;
      }

      tmp = ls_value.substr(offset + pos + 1, ls_value.length());
      offset += pos + 1;
      pos = tmp.find(pred_sep);
    }
    Predicate *p = create_predicate(tmp);
    if (p->valid) {
      predicates[p->key] = p;
    }
    return p->valid;
  }
}
}  // namespace librmb
