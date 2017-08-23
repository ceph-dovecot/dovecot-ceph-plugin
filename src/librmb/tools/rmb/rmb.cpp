/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "rmb.h"

#include <iostream>
#include "rados-cluster.h"
#include "rados-storage.h"
#include "rados-mail-object.h"
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <stdlib.h>
#include <errno.h>
#include "limits.h"
#include <algorithm>  // std::sort

#include "ls_cmd_parser.h"

using namespace std;
using namespace librmb;

void argv_to_vec(int argc, const char **argv, std::vector<const char *> &args) {
  args.insert(args.end(), argv + 1, argv + argc);
}
bool split_dashdash(const std::vector<const char *> &args, std::vector<const char *> &options,
                    std::vector<const char *> &arguments) {
  bool dashdash = false;
  for (std::vector<const char *>::const_iterator i = args.begin(); i != args.end(); ++i) {
    if (dashdash) {
      arguments.push_back(*i);
    } else {
      if (strcmp(*i, "--") == 0)
        dashdash = true;
      else
        options.push_back(*i);
    }
  }
  return dashdash;
}
bool get_next_token(const string &s, size_t &pos, const char *delims, string &token) {
  int start = s.find_first_not_of(delims, pos);
  int end;

  if (start < 0) {
    pos = s.size();
    return false;
  }

  end = s.find_first_of(delims, start);
  if (end >= 0)
    pos = end + 1;
  else {
    pos = end = s.size();
  }

  token = s.substr(start, end - start);
  return true;
}
void get_str_vec(const string &str, const char *delims, vector<string> &str_vec) {
  size_t pos = 0;
  string token;
  str_vec.clear();

  while (pos < str.size()) {
    if (get_next_token(str, pos, delims, token)) {
      if (token.size() > 0) {
        str_vec.push_back(token);
      }
    }
  }
}
std::vector<std::string> get_str_vec(const std::string &str) {
  std::vector<std::string> str_vec;
  const char *delims = ";,= \t";
  get_str_vec(str, delims, str_vec);
  return str_vec;
}

void env_to_vec(std::vector<const char *> &args, const char *name) {
  if (!name)
    name = "CEPH_ARGS";
  char *p = getenv(name);
  if (!p)
    return;

  bool dashdash = false;
  std::vector<const char *> options;
  std::vector<const char *> arguments;
  if (split_dashdash(args, options, arguments))
    dashdash = true;

  std::vector<const char *> env_options;
  std::vector<const char *> env_arguments;
  static vector<string> str_vec;
  std::vector<const char *> env;
  str_vec.clear();
  get_str_vec(p, " ", str_vec);
  for (vector<string>::iterator i = str_vec.begin(); i != str_vec.end(); ++i)
    env.push_back(i->c_str());
  if (split_dashdash(env, env_options, env_arguments))
    dashdash = true;

  args.clear();
  args.insert(args.end(), options.begin(), options.end());
  args.insert(args.end(), env_options.begin(), env_options.end());
  if (dashdash)
    args.push_back("--");
  args.insert(args.end(), arguments.begin(), arguments.end());
  args.insert(args.end(), env_arguments.begin(), env_arguments.end());
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

int va_ceph_argparse_witharg(std::vector<const char *> &args, std::vector<const char *>::iterator &i, std::string *ret,
                             std::ostream &oss, va_list ap) {
  const char *first = *i;
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
        i = args.erase(i);
        return 1;
      } else if (first[strlen_a] == '\0') {
        // find second part (or not)
        if (i + 1 == args.end()) {
          oss << "Option " << *i << " requires an argument." << std::endl;
          i = args.erase(i);
          return -EINVAL;
        }
        i = args.erase(i);
        *ret = *i;
        i = args.erase(i);
        return 1;
      }
    }
  }
}
void ceph_arg_value_type(const char *nextargstr, bool *bool_option, bool *bool_numeric) {
  bool is_numeric = true;
  bool is_float = false;
  bool is_option;

  if (nextargstr == NULL) {
    return;
  }

  if (strlen(nextargstr) < 2) {
    is_option = false;
  } else {
    is_option = (nextargstr[0] == '-') && (nextargstr[1] == '-');
  }

  for (unsigned int i = 0; i < strlen(nextargstr); i++) {
    if (!(nextargstr[i] >= '0' && nextargstr[i] <= '9')) {
      // May be negative numeral value
      if ((i == 0) && (strlen(nextargstr) >= 2)) {
        if (nextargstr[0] == '-')
          continue;
      }
      if ((nextargstr[i] == '.') && (is_float == false)) {
        is_float = true;
        continue;
      }

      is_numeric = false;
      break;
    }
  }

  // -<option>
  if (nextargstr[0] == '-' && is_numeric == false) {
    is_option = true;
  }

  *bool_option = is_option;
  *bool_numeric = is_numeric;

  return;
}

long long strict_strtoll(const char *str, int base, std::string *err) {
  char *endptr;
  std::string errStr;
  errno = 0; /* To distinguish success/failure after call (see man page) */
  long long ret = strtoll(str, &endptr, base);

  if (endptr == str) {
    errStr = "Expected option value to be integer, got '";
    errStr.append(str);
    errStr.append("'");
    *err = errStr;
    return 0;
  }
  if ((errno == ERANGE && (ret == LLONG_MAX || ret == LLONG_MIN)) || (errno != 0 && ret == 0)) {
    errStr = "The option value '";
    errStr.append(str);
    errStr.append("'");
    errStr.append(" seems to be invalid");
    *err = errStr;
    return 0;
  }
  if (*endptr != '\0') {
    errStr = "The option value '";
    errStr.append(str);
    errStr.append("'");
    errStr.append(" contains invalid digits");
    *err = errStr;
    return 0;
  }
  *err = "";
  return ret;
}

int strict_strtol(const char *str, int base, std::string *err) {
  std::string errStr;
  long long ret = strict_strtoll(str, base, err);
  if (!err->empty())
    return 0;
  if ((ret <= INT_MIN) || (ret >= INT_MAX)) {
    errStr = "The option value '";
    errStr.append(str);
    errStr.append("'");
    errStr.append(" seems to be invalid");
    *err = errStr;
    return 0;
  }
  return static_cast<int>(ret);
}
float strict_strtof(const char *str, std::string *err) {
  char *endptr;
  errno = 0; /* To distinguish success/failure after call (see man page) */
  float ret = strtof(str, &endptr);
  if (errno == ERANGE) {
    ostringstream oss;
    oss << "strict_strtof: floating point overflow or underflow parsing '" << str << "'";
    *err = oss.str();
    return 0.0;
  }
  if (endptr == str) {
    ostringstream oss;
    oss << "strict_strtof: expected float, got: '" << str << "'";
    *err = oss.str();
    return 0;
  }
  if (*endptr != '\0') {
    ostringstream oss;
    oss << "strict_strtof: garbage at end of string. got: '" << str << "'";
    *err = oss.str();
    return 0;
  }
  *err = "";
  return ret;
}

struct strict_str_convert {
  const char *str;
  std::string *err;
  strict_str_convert(const char *str, std::string *err) : str(str), err(err) {}

  inline operator float() const { return strict_strtof(str, err); }
  inline operator int() const { return strict_strtol(str, 10, err); }
  inline operator long long() const { return strict_strtoll(str, 10, err); }
};
template <class T>
bool ceph_argparse_witharg(std::vector<const char *> &args, std::vector<const char *>::iterator &i, T *ret,
                           std::ostream &oss, ...) {
  int r;
  va_list ap;
  bool is_option = false;
  bool is_numeric = true;
  std::string str;
  va_start(ap, oss);
  r = va_ceph_argparse_witharg(args, i, &str, oss, ap);
  va_end(ap);
  if (r == 0) {
    return false;
  } else if (r < 0) {
    return true;
  }

  ceph_arg_value_type(str.c_str(), &is_option, &is_numeric);
  if ((is_option == true) || (is_numeric == false)) {
    *ret = EXIT_FAILURE;
    if (is_option == true) {
      oss << "Missing option value";
    } else {
      oss << "The option value '" << str << "' is invalid";
    }
    return true;
  }

  std::string err;
  T myret = strict_str_convert(str.c_str(), &err);
  *ret = myret;
  if (!err.empty()) {
    oss << err;
  }
  return true;
}

template bool ceph_argparse_witharg<int>(std::vector<const char *> &args, std::vector<const char *>::iterator &i,
                                         int *ret, std::ostream &oss, ...);

template bool ceph_argparse_witharg<long long>(std::vector<const char *> &args, std::vector<const char *>::iterator &i,
                                               long long *ret, std::ostream &oss, ...);

template bool ceph_argparse_witharg<float>(std::vector<const char *> &args, std::vector<const char *>::iterator &i,
                                           float *ret, std::ostream &oss, ...);

bool ceph_argparse_witharg(std::vector<const char *> &args, std::vector<const char *>::iterator &i, std::string *ret,
                           std::ostream &oss, ...) {
  int r;
  va_list ap;
  va_start(ap, oss);
  r = va_ceph_argparse_witharg(args, i, ret, oss, ap);
  va_end(ap);
  return r != 0;
}

bool ceph_argparse_witharg(std::vector<const char *> &args, std::vector<const char *>::iterator &i, std::string *ret,
                           ...) {
  int r;
  va_list ap;
  va_start(ap, ret);
  r = va_ceph_argparse_witharg(args, i, ret, cerr, ap);
  va_end(ap);
  if (r < 0)
    _exit(1);
  return r != 0;
}

/** Once we see a standalone double dash, '--', we should remove it and stop
 * looking for any other options and flags. */
bool ceph_argparse_double_dash(std::vector<const char *> &args, std::vector<const char *>::iterator &i) {
  if (strcmp(*i, "--") == 0) {
    i = args.erase(i);
    return true;
  }
  return false;
}
void usage(ostream &out) {
  out << "usage: rmb [options] [commands]\n"
         "   -p pool\n"
         "        pool where mail data is saved, if not given mail_storage is used \n"
         "   -N namespace e.g. dovecot user name\n"
         "   --namespace=namespace\n"
         "        specify the namespace/user to use for the mails\n"
         "\n"
         "MAIL COMMANDS\n"
         "    ls     -   list all mails and mailbox statistic \n"
         "           all list all mails and mailbox statistic \n"
         "           <XATTR><OP><VALUE> e.g. U=7, \"U<7\", \"U>7\" \n"
         "                      <VALUE> e.g. R= %Y-%m-%d %H:%M (\"R=2017-08-22 14:30\")\n"
         "                      <OP> =,>,< for strings only = is supported.\n"
         "MAILBOX COMMANDS\n"
         "    ls     mb  list all mailboxes"
         "\n";
  //"MAILBOX COMMANDS\n"
  //"\n";
}

static void usage_exit() {
  usage(cerr);
  exit(1);
}

void print_mailbox_stat(vector<RadosMailObject *> mail_objects, CmdLineParser &parser) {
  std::map<std::string, RadosMailBox *> mailbox;

  for (std::vector<RadosMailObject *>::iterator it = mail_objects.begin(); it != mail_objects.end(); ++it) {
    std::string mailbox_key = std::string(1, (char)RBOX_METADATA_MAILBOX_GUID);

    std::string mailbox_guid = (*it)->get_xvalue(mailbox_key);
    if (parser.contains_key(mailbox_key)) {
      Predicate *p = parser.get_predicate(mailbox_key);
      if (!p->eval(mailbox_guid)) {
        continue;
      }
    }
    if (mailbox.count(mailbox_guid) > 0) {
      mailbox[mailbox_guid]->add_mail((*it));
      mailbox[mailbox_guid]->add_to_mailbox_size((*it)->get_object_size());
    } else {
      mailbox[mailbox_guid] = new RadosMailBox(mailbox_guid, 1);
      mailbox[mailbox_guid]->set_xattr_filter(&parser);
      mailbox[mailbox_guid]->add_mail((*it));
      mailbox[mailbox_guid]->add_to_mailbox_size((*it)->get_object_size());
    }
  }
  std::cout << "mailbox_count: " << mailbox.size() << std::endl;

  {
    for (std::map<std::string, RadosMailBox *>::iterator it = mailbox.begin(); it != mailbox.end(); ++it)
      std::cout << it->second->to_string() << std::endl;
  }
}


int main(int argc, const char **argv) {
  vector<RadosMailObject *> mail_objects;

  vector<const char *> args;
  argv_to_vec(argc, argv, args);
  env_to_vec(args, NULL);
  std::string val;
  std::map<std::string, std::string> opts;

  std::vector<const char *>::iterator i;
  for (i = args.begin(); i != args.end();) {
    if (ceph_argparse_double_dash(args, i)) {
      break;
    } else if (ceph_argparse_witharg(args, i, &val, "-p", "--pool", (char *)NULL)) {
      opts["pool"] = val;
      // std::cout << "pool: " << val << std::endl;
    } else if (ceph_argparse_witharg(args, i, &val, "-N", "--namespace", (char *)NULL)) {
      opts["namespace"] = val;
      // std::cout << "namespace: " << val << std::endl;
    } else if (ceph_argparse_witharg(args, i, &val, "ls", "--ls", (char *)NULL)) {
      opts["ls"] = val;
    } else {
      ++i;
    }
  }

  if (opts.size() < 3) {
    usage_exit();
  }

  RadosStorage *storage = NULL;
  RadosCluster cluster;
  std::string pool_name(opts["pool"]);
  std::string ns(opts["namespace"]);

  int open_connection = cluster.open_connection(&storage, pool_name, ns);

  if (open_connection < 0) {
    std::cout << " error opening rados connection" << std::endl;
    return -1;
  }
  librados::NObjectIterator iter(storage->get_io_ctx().nobjects_begin());
  while (iter != storage->get_io_ctx().nobjects_end()) {

    RadosMailObject *mail = new RadosMailObject();
    mail->set_oid(iter->get_oid());

    storage->get_io_ctx().getxattrs(iter->get_oid(), *mail->get_xattr());
    uint64_t object_size = 0;
    time_t save_date_rados = 0;
    storage->get_io_ctx().stat(iter->get_oid(), &object_size, &save_date_rados);
    mail->set_object_size(object_size);
    mail->set_rados_save_date(save_date_rados);

    // print_rados_mail(storage->get_io_ctx(), iter->get_oid());
    ++iter;

    mail_objects.push_back(mail);
  }
  CmdLineParser parser(opts["ls"]);
  if (opts.find("ls") != opts.end()) {
    if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0) {
      print_mailbox_stat(mail_objects, parser);
    } else if (parser.parse_ls_string()) {
      print_mailbox_stat(mail_objects, parser);
    } else {
      // tear down.
      cluster.deinit();
      usage_exit();
    }
  } else {
    // tear down.
    cluster.deinit();
    usage_exit();
  }
  /* for (std::vector<RadosMailObject *>::iterator it = mail_objects.begin(); it != mail_objects.end(); ++it) {
     std::cout << ' ' << (*it)->to_string();
   }
 */
  // tear down.
  cluster.deinit();
}
