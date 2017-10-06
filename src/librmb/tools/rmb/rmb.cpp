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

#include "rmb.h"
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <errno.h>
#include <stdlib.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

#include <algorithm>  // std::sort

#include <limits>

#include "../../rados-cluster.h"
#include "../../rados-cluster-impl.h"
#include "../../rados-storage.h"
#include "../../rados-storage-impl.h"
#include "rados-mail-object.h"
#include "ls_cmd_parser.h"
#include "mailbox_tools.h"

static void argv_to_vec(int argc, const char **argv, std::vector<const char *> *args) {
  args->insert(args->end(), argv + 1, argv + argc);
}

static void dashes_to_underscores(const char *input, char *output) {
  char c = 0;
  char *o = output;
  const char *i = input;
  // first two characters are copied as-is
  *o = *i++;
  if (*o++ == '\0')
    return;
  *o = *i++;
  if (*o++ == '\0')
    return;
  for (; ((c = *i)); ++i) {
    if (c == '=') {
      strcpy(o, i);
      return;
    }
    if (c == '-')
      *o++ = '_';
    else
      *o++ = c;
  }
  *o++ = '\0';
}

static int va_ceph_argparse_witharg(std::vector<const char *> *args, std::vector<const char *>::iterator *i,
                                    std::string *ret, std::ostream &oss, va_list ap) {
  const char *first = *(*i);
  char tmp[strlen(first) + 1];
  dashes_to_underscores(first, tmp);
  first = tmp;

  // does this argument match any of the possibilities?
  while (1) {
    const char *a = va_arg(ap, char *);
    if (a == NULL)
      return 0;

    int strlen_a = strlen(a);
    char a2[strlen_a + 1];

    dashes_to_underscores(a, a2);
    if (strncmp(a2, first, strlen(a2)) == 0) {
      if (first[strlen_a] == '=') {
        *ret = first + strlen_a + 1;
        *i = args->erase(*i);
        return 1;
      } else if (first[strlen_a] == '\0') {
        // find second part (or not)
        if (*i + 1 == args->end()) {
          oss << "Option " << *(*i) << " requires an argument." << std::endl;
          *i = args->erase(*i);
          return -EINVAL;
        }
        *i = args->erase(*i);
        *ret = *(*i);
        *i = args->erase(*i);
        return 1;
      }
    }
  }
}

static bool ceph_argparse_witharg(std::vector<const char *> *args, std::vector<const char *>::iterator *i,
                                  std::string *ret, ...) {
  int r;
  va_list ap;
  va_start(ap, ret);
  r = va_ceph_argparse_witharg(args, i, ret, std::cerr, ap);
  va_end(ap);
  if (r < 0)
    _exit(1);
  return r != 0;
}

/** Once we see a standalone double dash, '--', we should remove it and stop
 * looking for any other options and flags. */
static bool ceph_argparse_double_dash(std::vector<const char *> *args, std::vector<const char *>::iterator *i) {
  if (strcmp(*(*i), "--") == 0) {
    *i = args->erase(*i);
    return true;
  }
  return false;
}

static void usage(std::ostream &out) {
  out << "usage: rmb [options] [commands]\n"
         "   -p pool\n"
         "        pool where mail data is saved, if not given mail_storage is used\n"
         "   -N namespace e.g. dovecot user name\n"
         "        specify the namespace/user to use for the mails\n"
         "   -O path to store the boxes. If not given, $HOME/rmb is used\n"
         "\n"
         "MAIL COMMANDS\n"
         "    ls     -   list all mails and mailbox statistic\n"
         "           all list all mails and mailbox statistic\n"
         "           <XATTR><OP><VALUE> e.g. U=7, \"U<7\", \"U>7\"\n"
         "                      <VALUE> e.g. R= %Y-%m-%d %H:%M (\"R=2017-08-22 14:30\")\n"
         "                      <OP> =,>,< for strings only = is supported.\n"
         "    get     - download mails to file\n"
         "            <XATTR><OP><VALUE> e.g. U=7, \"U<7\", \"U>7\"\n"
         "                      <VALUE> e.g. R= %Y-%m-%d %H:%M (\"R=2017-08-22 14:30\")\n"
         "                      <OP> =,>,< for strings only = is supported.\n"
         "    set     oid XATTR value e.g. U 1 B INBOX\n"
         "MAILBOX COMMANDS\n"
         "    ls     mb  list all mailboxes\n"
         "\n";
}

static void usage_exit() {
  usage(std::cerr);
  exit(1);
}

static void query_mail_storage(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::CmdLineParser *parser,
                               bool download, librmb::RadosStorage *storage) {
  std::map<std::string, librmb::RadosMailBox *> mailbox;

  for (std::vector<librmb::RadosMailObject *>::iterator it = mail_objects->begin(); it != mail_objects->end(); ++it) {
    std::string mailbox_key = std::string(1, static_cast<char>(librmb::RBOX_METADATA_MAILBOX_GUID));
    std::string mailbox_guid = (*it)->get_xvalue(mailbox_key);
    std::string mailbox_orig_name_key = std::string(1, static_cast<char>(librmb::RBOX_METADATA_ORIG_MAILBOX));
    std::string mailbox_orig_name = (*it)->get_xvalue(mailbox_orig_name_key);

    // std::cout << " mailbox _guid : " << mailbox_guid << std::endl;
    if (parser->contains_key(mailbox_key)) {
      //   std::cout << " containing key " << std::endl;
      librmb::Predicate *p = parser->get_predicate(mailbox_key);
      if (!p->eval(mailbox_guid)) {
        continue;
      }
    }
    if (mailbox.count(mailbox_guid) > 0) {
      mailbox[mailbox_guid]->add_mail((*it));
      mailbox[mailbox_guid]->add_to_mailbox_size((*it)->get_object_size());
    } else {
      mailbox[mailbox_guid] = new librmb::RadosMailBox(mailbox_guid, 1, mailbox_orig_name);
      mailbox[mailbox_guid]->set_xattr_filter(parser);
      mailbox[mailbox_guid]->add_mail((*it));
      mailbox[mailbox_guid]->add_to_mailbox_size((*it)->get_object_size());
    }
  }
  std::cout << "mailbox_count: " << mailbox.size() << std::endl;

  {
    for (std::map<std::string, librmb::RadosMailBox *>::iterator it = mailbox.begin(); it != mailbox.end(); ++it) {
      if (it->second->get_mail_count() == 0) {
        continue;
      }
      std::cout << it->second->to_string() << std::endl;

      if (download) {
        librmb::MailboxTools tools(it->second, parser->get_output_dir());
        if (tools.init_mailbox_dir() < 0) {
          std::cout << " error initializing output dir : " << parser->get_output_dir() << std::endl;
          break;
        }

        for (std::vector<librmb::RadosMailObject *>::iterator it_mail = it->second->get_mails().begin();
             it_mail != it->second->get_mails().end(); ++it_mail) {
          const std::string oid = (*it_mail)->get_oid();

          time_t pm_time;
          uint64_t size_r = 0;
          storage->stat_mail(oid, &size_r, &pm_time);

          librados::bufferlist buffer;

          char *mail_buffer = new char[size_r + 1];
          (*it_mail)->set_mail_buffer(mail_buffer);

          (*it_mail)->set_object_size(size_r);
          int read = storage->read_mail(&buffer, oid);
          if (read > 0) {
            memcpy(mail_buffer, buffer.to_str().c_str(), read + 1);
            if (tools.save_mail((*it_mail)) < 0) {
              std::cout << " error saving mail : " << oid << " to " << tools.get_mailbox_path() << std::endl;
            }
          }

          delete[] mail_buffer;
        }
      }
    }
  }

  for (auto &it : mailbox) {
    delete it.second;
  }
}

static void release_exit(std::vector<librmb::RadosMailObject *> *mail_objects, librmb::RadosCluster *cluster,
                         bool exit) {
  for (auto mo : *mail_objects) {
    delete mo;
  }
  cluster->deinit();
  if (exit == true) {
    usage_exit();
  }
}

int main(int argc, const char **argv) {
  std::vector<librmb::RadosMailObject *> mail_objects;

  std::vector<const char *> args;
  argv_to_vec(argc, argv, &args);
  std::string val;
  std::map<std::string, std::string> opts;

  std::map<std::string, std::string> xattr;
  unsigned int idx = 0;
  std::vector<const char *>::iterator i;
  for (i = args.begin(); i != args.end();) {
    if (ceph_argparse_double_dash(&args, &i)) {
      break;
    } else if (ceph_argparse_witharg(&args, &i, &val, "-p", "--pool", static_cast<char>(NULL))) {
      opts["pool"] = val;
    } else if (ceph_argparse_witharg(&args, &i, &val, "-N", "--namespace", static_cast<char>(NULL))) {
      opts["namespace"] = val;
    } else if (ceph_argparse_witharg(&args, &i, &val, "ls", "--ls", static_cast<char>(NULL))) {
      opts["ls"] = val;
    } else if (ceph_argparse_witharg(&args, &i, &val, "get", "--get", static_cast<char>(NULL))) {
      opts["get"] = val;
    } else if (ceph_argparse_witharg(&args, &i, &val, "-O", "--out", static_cast<char>(NULL))) {
      opts["out"] = val;
    } else if (ceph_argparse_witharg(&args, &i, &val, "set", "--set", static_cast<char>(NULL))) {
      opts["set"] = val;
      std::cout << " set : " << val << std::endl;
    } else {
      if (idx + 1 < args.size()) {
        xattr[args[idx]] = args[idx + 1];
        idx++;
      }
      ++idx;
      ++i;
    }
  }
  if (opts.size() < 3) {
    usage_exit();
  }

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name(opts["pool"]);
  std::string ns(opts["namespace"]);

  int open_connection = storage.open_connection(pool_name, ns);

  if (open_connection < 0) {
    std::cout << " error opening rados connection" << std::endl;
    return -1;
  }
  librados::NObjectIterator iter(storage.get_io_ctx().nobjects_begin());
  while (iter != storage.get_io_ctx().nobjects_end()) {
    librmb::RadosMailObject *mail = new librmb::RadosMailObject();
    mail->set_oid(iter->get_oid());

    storage.get_io_ctx().getxattrs(iter->get_oid(), *mail->get_xattr());
    uint64_t object_size = 0;
    time_t save_date_rados = 0;
    storage.get_io_ctx().stat(iter->get_oid(), &object_size, &save_date_rados);
    mail->set_object_size(object_size);
    mail->set_rados_save_date(save_date_rados);

    // print_rados_mail(storage->get_io_ctx(), iter->get_oid());
    ++iter;

    mail_objects.push_back(mail);
  }

  if (opts.find("ls") != opts.end()) {
    librmb::CmdLineParser parser(opts["ls"]);
    if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0) {
      query_mail_storage(&mail_objects, &parser, false, nullptr);
    } else if (parser.parse_ls_string()) {
      query_mail_storage(&mail_objects, &parser, false, nullptr);
    } else {
      // tear down.
      release_exit(&mail_objects, &cluster, true);
    }
  } else if (opts.find("get") != opts.end()) {
    librmb::CmdLineParser parser(opts["get"]);

    if (opts.find("out") != opts.end()) {
      parser.set_output_dir(opts["out"]);
    } else {
      char outpath[PATH_MAX];
      char *home = getenv("HOME");
      if (home != NULL) {
        snprintf(outpath, sizeof(outpath), "%s/rmb", home);
      } else {
        snprintf(outpath, sizeof(outpath), "rmb");
      }
      parser.set_output_dir(outpath);
    }
    if (opts["get"].compare("all") == 0 || opts["get"].compare("-") == 0) {
      query_mail_storage(&mail_objects, &parser, true, &storage);
    } else if (parser.parse_ls_string()) {
      query_mail_storage(&mail_objects, &parser, true, &storage);
    } else {
      // tear down.
      release_exit(&mail_objects, &cluster, true);
    }
  } else if (opts.find("set") != opts.end()) {
    std::string oid = opts["set"];
    if (oid.empty() || xattr.size() < 1) {
      usage_exit();
    }

    for (std::map<std::string, std::string>::iterator it = xattr.begin(); it != xattr.end(); ++it) {
      std::cout << oid << "=> " << it->first << " = " << it->second << '\n';
      librmb::rbox_metadata_key ke = static_cast<librmb::rbox_metadata_key>(it->first[0]);
      librmb::RadosXAttr attr(ke, it->second);
      storage.set_metadata(oid, attr);
    }

  }
  else {
    // tear down.
    release_exit(&mail_objects, &cluster, true);
  }
  // tear down.
  release_exit(&mail_objects, &cluster, false);
}


