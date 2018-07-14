// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 * Copyright (c) 2007-2017 Dovecot authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
extern "C" {
#include "lib.h"
#include "module-dir.h"
#include "str.h"
#include "hash.h"
#include "dict.h"
#include "imap-match.h"
#include "doveadm-settings.h"
#include "mail-index-private.h"
#include "doveadm-mail.h"
#include "doveadm-rbox-plugin.h"
#include "mail-user.h"
#include "guid.h"
#include "mail-namespace.h"
#include "mail-search-parser-private.h"
#include "mail-search.h"
#include "mail-search-build.h"
#include "doveadm-cmd.h"
#include "doveadm-mail.h"
#include "istream.h"
#include "doveadm-print.h"
}
#include "tools/rmb/rmb-commands.h"
#include "rados-cluster.h"
#include "rados-cluster-impl.h"
#include "rados-storage.h"
#include "rados-storage-impl.h"
#include "rados-dovecot-ceph-cfg.h"
#include "rados-dovecot-ceph-cfg-impl.h"
#include "rados-namespace-manager.h"
#include "rbox-storage.h"
#include "rbox-save.h"
#include "rbox-storage.hpp"

#include <algorithm>



class RboxDoveadmPlugin {
 public:
  RboxDoveadmPlugin() {
    this->cluster = new librmb::RadosClusterImpl();
    this->storage = new librmb::RadosStorageImpl(cluster);
    this->config = new librmb::RadosDovecotCephCfgImpl(&storage->get_io_ctx());
  }
  ~RboxDoveadmPlugin() {
    if (config != nullptr) {
      delete config;
    }
    if (storage != nullptr) {
      storage->close_connection();
      delete storage;
    }
    if (cluster != nullptr) {
      cluster->deinit();
      delete cluster;
    }
  }
  int open_connection() {
    int ret = -1;
    if (config == nullptr) {
      return -1;
    }
    ret = storage->open_connection(config->get_pool_name(), config->get_rados_cluster_name(),
                                   config->get_rados_username());
    return ret;
    struct check_indices_cmd_context {
      struct doveadm_mail_cmd_context ctx;
      bool delete_not_referenced_objects;
    };

    struct delete_cmd_context {
      struct doveadm_mail_cmd_context ctx;
      ARRAY_TYPE(const_string) mailboxes;
      bool recursive;
      bool require_empty;
#if DOVECOT_PREREQ(2, 3)
      bool unsafe;
#endif
      bool subscriptions;
      pool_t pool;
    };
  }
  int read_plugin_configuration(struct mail_user *user) {
    if (user == NULL) {
      return 0;
    }
    std::map<std::string, std::string> *map = config->get_config();
    for (std::map<std::string, std::string>::iterator it = map->begin(); it != map->end(); ++it) {
      std::string setting = it->first;

      const char *value = mail_user_plugin_getenv(user, setting.c_str());
      if (value != NULL) {
        config->update_metadata(setting, value);
      }
    }
    config->set_config_valid(true);
    return 0;
  }
  int read_doveadm_plugin_configuration() {
    std::map<std::string, std::string> *map = config->get_config();
    for (std::map<std::string, std::string>::iterator it = map->begin(); it != map->end(); ++it) {
      std::string setting = it->first;

      const char *value = doveadm_plugin_getenv(setting.c_str());
      if (value != NULL) {
        config->update_metadata(setting, value);
      }
    }
    config->set_config_valid(true);
    return 0;
  }

 public:
  librmb::RadosCluster *cluster;
  librmb::RadosStorage *storage;
  librmb::RadosDovecotCephCfg *config;
};

static int open_connection_load_config(RboxDoveadmPlugin *plugin) {
  int ret = -1;

  plugin->read_doveadm_plugin_configuration();
  int open = plugin->open_connection();
  if (open < 0) {
    i_error("Error opening rados connection. Errorcode: %d", open);
    return 0;
  }
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin->config))->get_rados_ceph_cfg();
  ret = cfg->load_cfg();
  if (ret < 0) {
    i_error("Error accessing configuration. Errorcode: %d", ret);
    return ret;
  }
  return ret;
}
static int cmd_rmb_config(std::map<std::string, std::string> &opts) {
  RboxDoveadmPlugin plugin;
  plugin.read_doveadm_plugin_configuration();
  int open = open_connection_load_config(&plugin);
  if (open < 0) {
    i_error("Error opening rados connection. Errorcode: %d", open);
    return open;
  }
  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();
  int ret = rmb_cmds.configuration(true, *cfg);
  if (ret < 0) {
    i_error("Error processing ceph configuration. Errorcode: %d", ret);
    return -1;
  }
  return 0;
}
static int cmd_rmb_search_run(std::map<std::string, std::string> &opts, struct mail_user *user, bool download,
                              librmb::CmdLineParser &parser, std::vector<librmb::RadosMailObject *> &mail_objects,
                              bool silent, bool load_metadata = true) {
  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin);
  if (open < 0) {
    i_error("Error opening rados connection. Errorcode: %d", open);
    return 0;
  }

  opts["namespace"] = user->username;
  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);

  std::string uid;
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

  librmb::RadosStorageMetadataModule *ms = rmb_cmds.init_metadata_storage_module(*cfg, &uid);
  if (ms == nullptr) {
    i_error(" Error initializing metadata module");
    delete ms;
    return -1;
  }

  int ret = rmb_cmds.load_objects(ms, mail_objects, opts["sort"], load_metadata);
  if (ret < 0) {
    i_error("Error loading ceph objects. Errorcode: %d", ret);
    delete ms;
    return ret;
  }

  if (download) {
    rmb_cmds.set_output_path(&parser);
  }
  ret = rmb_cmds.query_mail_storage(&mail_objects, &parser, download, silent);
  if (ret < 0) {
    i_error("Error query mail storage. Errorcode: %d", ret);
  }
  delete ms;

  return ret;
}

static int cmd_rmb_ls_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *search_query = ctx->args[0];
  const char *sort = ctx->args[1];

  if (search_query == NULL) {
    i_error("no search query given");
    ctx->exit_code = -1;
    return 0;
  }

  std::map<std::string, std::string> opts;
  opts["ls"] = search_query;
  opts["sort"] = sort != NULL ? sort : "uid";
  librmb::CmdLineParser parser(opts["ls"]);

  if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
    std::vector<librmb::RadosMailObject *> mail_objects;
    ctx->exit_code = cmd_rmb_search_run(opts, user, false, parser, mail_objects, false);
    for (auto mo : mail_objects) {
      delete mo;
    }
  } else {
    i_error("invalid ls search query, %s", search_query);
    ctx->exit_code = -1;
  }
  return 0;
}
static int cmd_rmb_ls_mb_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  int ret = -1;
  std::map<std::string, std::string> opts;
  opts["ls"] = "mb";
  opts["sort"] = "uid";
  librmb::CmdLineParser parser(opts["ls"]);
  if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
    std::vector<librmb::RadosMailObject *> mail_objects;
    ctx->exit_code = cmd_rmb_search_run(opts, user, false, parser, mail_objects, false);
    for (auto mo : mail_objects) {
      delete mo;
    }
  } else {
    i_error("invalid ls search query");
    ctx->exit_code = -1;
  }
  return ret;
}

static int cmd_rmb_get_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *search_query = ctx->args[0];
  const char *output_path = ctx->args[1];

  if (search_query == NULL) {
    i_error("no search query given");
    ctx->exit_code = -1;
    return 0;
  }

  std::map<std::string, std::string> opts;
  opts["get"] = search_query;
  if (output_path != NULL) {
    opts["out"] = output_path;
  }
  opts["sort"] = "uid";

  librmb::CmdLineParser parser(opts["get"]);
  if (opts["get"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
    std::vector<librmb::RadosMailObject *> mail_objects;
    ctx->exit_code = cmd_rmb_search_run(opts, user, true, parser, mail_objects, false);
    for (auto mo : mail_objects) {
      delete mo;
    }

  } else {
    i_error("invalid search query %s", search_query);
    ctx->exit_code = -1;
  }
  return 0;
}
static int cmd_rmb_set_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *oid = ctx->args[0];
  const char *key_value_pair = ctx->args[1];

  if (!ctx->iterate_single_user) {
    i_error("set command is only available for single user!");
    ctx->exit_code = -1;
    return 0;
  }

  if (oid == NULL || key_value_pair == NULL) {
    i_error("error check params: %s : %s ", oid, key_value_pair);
    ctx->exit_code = -1;
    return 0;
  }

  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin);
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    ctx->exit_code = open;
    return 0;
  }

  std::map<std::string, std::string> opts;
  opts["set"] = oid;
  opts["namespace"] = user->username;

  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);

  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

  std::string uid;
  librmb::RadosStorageMetadataModule *ms = rmb_cmds.init_metadata_storage_module(*cfg, &uid);
  if (ms == nullptr) {
    i_error(" Error initializing metadata module ");
    delete ms;
    ctx->exit_code = -1;
    return 0;
  }
  std::map<std::string, std::string> metadata;
  std::stringstream left(key_value_pair);
  std::vector<std::string> tokens;
  std::string item;
  while (std::getline(left, item, '=')) {
    tokens.push_back(item);
  }
  if (tokens.size() == 2) {
    if (tokens[0].size() > 1) {
      ctx->exit_code = -1;
      std::cerr << "check key " << tokens[0] << " is not a valid attribute" << std::endl;
    } else {
      metadata[tokens[0]] = tokens[1];
      ctx->exit_code = rmb_cmds.update_attributes(ms, &metadata);
      std::cerr << " token " << tokens[0] << " : " << tokens[1] << std::endl;
    }
  } else {
    std::cerr << "check params: key_value_pair(" << key_value_pair << ") is not valid: use key=value" << std::endl;
    ctx->exit_code = -1;
  }
  delete ms;
  return 0;
}

static int cmd_rmb_delete_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *oid = ctx->args[0];
  if (oid == NULL) {
    i_error("no oid given");
    ctx->exit_code = -1;
    return 0;
  }

  if (!ctx->iterate_single_user) {
    i_error("delete command is only available for single user!");
    ctx->exit_code = -1;
    return 0;
  }

  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin);
  if (open < 0) {
    i_error("Error opening rados connection. Errorcode: %d", open);
    ctx->exit_code = open;
    return 0;
  }

  std::map<std::string, std::string> opts;
  opts["to_delete"] = oid;
  opts["namespace"] = user->username;

  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

  std::string uid;
  librmb::RadosStorageMetadataModule *ms = rmb_cmds.init_metadata_storage_module(*cfg, &uid);
  if (ms == nullptr) {
    i_error(" Error initializing metadata module ");
    delete ms;
    ctx->exit_code = -1;
    return 0;
  }
  ctx->exit_code = rmb_cmds.delete_mail(true);
  if (ctx->exit_code < 0) {
    i_error("Error deleting mail. Errorcode: %d", ctx->exit_code);
  }
  delete ms;
  return 0;
}
static int cmd_rmb_rename_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *new_user_name = ctx->args[0];
  if (new_user_name == NULL) {
    i_error("no username given");
    ctx->exit_code = -1;
    return 0;
  }

  if (!ctx->iterate_single_user) {
    i_error("rename command is only available for single user");
    ctx->exit_code = -1;
    return 0;
  }

  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin);
  if (open < 0) {
    i_error("Error opening rados connection. Errorcode: %d", open);
    ctx->exit_code = open;
    return 0;
  }

  std::map<std::string, std::string> opts;
  opts["to_rename"] = new_user_name;
  opts["namespace"] = user->username;

  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);
  std::string uid;
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

  ctx->exit_code = rmb_cmds.rename_user(cfg, true, user->username);
  if (ctx->exit_code < 0) {
    i_error("Error renaming user. Errorcode: %d", ctx->exit_code);
  }

  return 0;
}
static int restore_index_entry(struct mail_user *user, const char *mailbox_name, const std::string &str_mail_guid,
                               const std::string &str_mail_oid) {
  struct mail_namespace *ns = mail_namespace_find_inbox(user->namespaces);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox_name, MAILBOX_FLAG_READONLY);
  if (mailbox_open(box) < 0) {
    i_error("Error opening mailbox %s", mailbox_name);
    return -1;
  }
#if DOVECOT_PREREQ(2, 3)
  char reason[256];
  memset(reason, '\0', sizeof(reason));
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#else
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#endif

  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  const struct mail_index_header *hdr = mail_index_get_header(save_ctx->transaction->view);
  uint32_t next_uid, seq = 0;
  // modify index
  if (hdr->next_uid != 0) {
    // found a uid;
    next_uid = hdr->next_uid;
  } else {
    next_uid = 1;
  }
/* add to index */
#if DOVECOT_PREREQ(2, 3)

  if ((save_ctx->transaction->flags & MAILBOX_TRANSACTION_FLAG_FILL_IN_STUB) == 0) {
    mail_index_append(save_ctx->transaction->itrans, next_uid, &seq);
  } else {
    seq = save_ctx->data.stub_seq;
  }
#else
  mail_index_append(save_ctx->transaction->itrans, next_uid, &seq);
#endif

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  i_zero(&rec);
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)save_ctx;

  guid_128_t mail_guid, mail_oid;
  guid_128_from_string(str_mail_guid.c_str(), mail_guid);
  guid_128_from_string(str_mail_oid.c_str(), mail_oid);

  memcpy(rec.guid, mail_guid, sizeof(mail_guid));
  memcpy(rec.oid, mail_oid, sizeof(mail_oid));
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;

  mail_index_update_ext(save_ctx->transaction->itrans, seq, mbox->ext_id, &rec, NULL);

  save_ctx->transaction->save_count++;
  r_ctx->finished = TRUE;
  save_ctx->finishing = FALSE;
  save_ctx->unfinished = FALSE;
  save_ctx->saving = FALSE;
  if (mailbox_transaction_commit(&trans) < 0) {
    return -1;
  }

  mailbox_free(&box);
  return 0;
}

static int doveadm_rmb_mail_next_user(struct doveadm_mail_cmd_context *ctx,
                                      const struct mail_storage_service_input *input,
                                      struct mail_storage_service_user *cur_service_user,
                                      struct mail_user **cur_mail_user, const char **error_r,
                                      std::list<librmb::RadosSaveLogEntry> *entries) {
  const char *error, *ip;
  int ret;

  ip = net_ip2addr(&input->remote_ip);
  if (ip[0] == '\0')
    i_set_failure_prefix("doveadm(%s): ", input->username);
  else
    i_set_failure_prefix("doveadm(%s,%s): ", ip, input->username);

  /* see if we want to execute this command via (another)
     doveadm server */
  ret = doveadm_mail_server_user(ctx, input, error_r);
  if (ret != 0) {
    i_debug("doveadm_mail_server_user");
    return ret;
  }

  ret = mail_storage_service_lookup(ctx->storage_service, input, &cur_service_user, &error);
  if (ret <= 0) {
    if (ret < 0) {
      *error_r = t_strdup_printf("User lookup failed: %s", error);
    }
    return ret;
  }
#if DOVECOT_PREREQ(2, 3)
  ret = mail_storage_service_next(ctx->storage_service, cur_service_user, cur_mail_user, error_r);

#else
  ret = mail_storage_service_next(ctx->storage_service, cur_service_user, cur_mail_user);
#endif
  if (ret < 0) {
    *error_r = "User init failed";
#if DOVECOT_PREREQ(2, 3)
    mail_storage_service_user_unref(&cur_service_user);
#else
    mail_storage_service_user_free(&cur_service_user);
#endif
  return ret;
  }
  for (std::list<librmb::RadosSaveLogEntry>::iterator it = entries->begin(); it != entries->end(); ++it) {
    // do something here!

    std::string key_guid(1, static_cast<char>(librmb::RBOX_METADATA_GUID));
    std::string key_mbox_name(1, static_cast<char>(librmb::RBOX_METADATA_ORIG_MAILBOX));

    std::list<librmb::RadosMetadata>::iterator it_guid =
        std::find_if(it->metadata.begin(), it->metadata.end(),
                     [key_guid](librmb::RadosMetadata const &m) { return m.key == key_guid; });
    std::list<librmb::RadosMetadata>::iterator it_mb =
        std::find_if(it->metadata.begin(), it->metadata.end(),
                     [key_mbox_name](librmb::RadosMetadata const &m) { return m.key == key_mbox_name; });

    restore_index_entry(*cur_mail_user, (*it_mb).bl.to_str().c_str(), (*it_guid).bl.to_str(), it->src_oid);
  }
  mail_user_unref(cur_mail_user);

#if DOVECOT_PREREQ(2, 3)
  mail_storage_service_user_unref(&cur_service_user);
#else
  mail_storage_service_user_free(&cur_service_user);
#endif
  return 1;
}

static int cmd_rmb_revert_log_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *log_file = ctx->args[0];

  if (!ctx->iterate_single_user) {
    i_error("revert command is only available for single user");
    ctx->exit_code = -1;
    return 0;
  }

  if (log_file == NULL) {
    i_error("Error: no logfile given!");
    ctx->exit_code = -1;
    return 0;
  }

  RboxDoveadmPlugin plugin;
  plugin.read_plugin_configuration(user);
  int open = plugin.open_connection();
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    ctx->exit_code = open;
    return 0;
  }
  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;
  ctx->exit_code = librmb::RmbCommands::delete_with_save_log(log_file, plugin.config->get_rados_cluster_name(),
                                                             plugin.config->get_rados_username(), &moved_items);

  for (std::map<std::string, std::list<librmb::RadosSaveLogEntry>>::iterator iter = moved_items.begin();
       iter != moved_items.end(); ++iter) {
    const char *error;

    struct mail_storage_service_user *cur_service_user = NULL;

    struct mail_user *cur_mail_user = NULL;
    ctx->storage_service_input.username = iter->first.c_str();
    doveadm_rmb_mail_next_user(ctx, &ctx->storage_service_input, cur_service_user, &cur_mail_user, &error,
                               &iter->second);
  }

  return 0;
}

static int iterate_mailbox(struct mail_namespace *ns, const struct mailbox_info *info,
                           std::vector<librmb::RadosMailObject *> &mail_objects) {
  int ret = 0;
  struct mailbox_transaction_context *mailbox_transaction;
  struct mail_search_context *search_ctx;
  struct mail_search_args *search_args;
  struct mail *mail;

  struct mailbox *box = mailbox_alloc(ns->list, info->vname, MAILBOX_FLAG_SAVEONLY);

  if (mailbox_open(box) < 0) {
    i_error("Error opening mailbox %s", info->vname);
    return -1;
  }
#if DOVECOT_PREREQ(2, 3)
  char reason[256];
  memset(reason, '\0', sizeof(reason));
  mailbox_transaction = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);

#else
  mailbox_transaction = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#endif

  search_args = mail_search_build_init();
  mail_search_build_add(search_args, SEARCH_ALL);

  search_ctx = mailbox_search_init(mailbox_transaction, search_args, NULL, static_cast<mail_fetch_field>(0), NULL);
  mail_search_args_unref(&search_args);
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  std::cout << "box: " << info->vname << std::endl;
  int mail_count = 0;
  int mail_count_missing = 0;
  while (mailbox_search_next(search_ctx, &mail)) {
    ++mail_count;
    const struct obox_mail_index_record *obox_rec;
    const void *rec_data;
    mail_index_lookup_ext(mail->transaction->view, mail->seq, mbox->ext_id, &rec_data, NULL);
    obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);

    if (obox_rec == nullptr) {
      std::cerr << "no valid extended header for mail with uid: " << mail->uid << std::endl;
      continue;
    }
    std::string guid = guid_128_to_string(obox_rec->guid);
    std::string oid = guid_128_to_string(obox_rec->oid);

    auto it_mail = std::find_if(mail_objects.begin(), mail_objects.end(),
                                [oid](librmb::RadosMailObject *m) { return m->get_oid().compare(oid) == 0; });

    if (it_mail == mail_objects.end()) {
      std::cout << "   missing mail object: uid=" << mail->uid << " guid=" << guid << " oid : " << oid
                << " available: " << (it_mail != mail_objects.end()) << std::endl;
      ++mail_count_missing;
    } else {
      mail_objects.erase(it_mail);  // calls destructor of RadosMailObject*
    }
  }
  if (mailbox_search_deinit(&search_ctx) < 0) {
    return -1;
  }
  if (mailbox_transaction_commit(&mailbox_transaction) < 0) {
    return -1;
  }
  mailbox_free(&box);
  std::cout << "   mails total: " << mail_count << ", missing mails in objectstore: " << mail_count_missing
            << std::endl;

  if (mail_count_missing > 0) {
    std::cout << "NOTE: you can fix(remove) the invalid index entries by using doveadm force-resync" << std::endl;
  }
  return ret;
}

static int check_namespace_mailboxes(struct mail_namespace *ns, std::vector<librmb::RadosMailObject *> &mail_objects) {
  struct mailbox_list_iterate_context *iter;
  const struct mailbox_info *info;
  int ret = 0;

  iter = mailbox_list_iter_init(ns->list, "*", static_cast<enum mailbox_list_iter_flags>(
                                                   MAILBOX_LIST_ITER_RAW_LIST | MAILBOX_LIST_ITER_RETURN_NO_FLAGS));
  while ((info = mailbox_list_iter_next(iter)) != NULL) {
    if ((info->flags & (MAILBOX_NONEXISTENT | MAILBOX_NOSELECT)) == 0) {
      ret = iterate_mailbox(ns, info, mail_objects);
      if (ret < 0) {
        ret = -1;
        break;
      }
    }
  }
  if (mailbox_list_iter_deinit(&iter) < 0)
    ret = -1;
  return ret;
}

static int cmd_rmb_check_indices_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  struct check_indices_cmd_context *ctx_ = (struct check_indices_cmd_context *)ctx;

  std::map<std::string, std::string> opts;
  opts["ls"] = "-";  // search all objects
  opts["sort"] = "uid";
  librmb::CmdLineParser parser(opts["ls"]);
  parser.parse_ls_string();
  std::vector<librmb::RadosMailObject *> mail_objects;
  ctx->exit_code = cmd_rmb_search_run(opts, user, false, parser, mail_objects, true, false);
  if (ctx->exit_code < 0) {
    return 0;
  }

  struct mail_namespace *ns = mail_namespace_find_inbox(user->namespaces);
  for (; ns != NULL; ns = ns->next) {
    check_namespace_mailboxes(ns, mail_objects);
  }

  if (mail_objects.size() > 0) {
    std::cout << std::endl << "There are mail objects without a index reference: " << std::endl;
    std::cout << "NOTE: you can fix(restore) the lost index entries by using doveadm force-resync or delete the "
                 "unrefrenced objects from objectstore "
                 "with the delete_not_referenced_objects option"
              << std::endl;
    ctx->exit_code = 1;
  }

  RboxDoveadmPlugin plugin;

  int open = open_connection_load_config(&plugin);
  if (open < 0) {
    i_error("Error open connection to cluster %d", open);
    ctx->exit_code = open;
    return 0;
  }
  opts["namespace"] = user->username;

  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

  std::string uid;
  librmb::RadosStorageMetadataModule *ms = rmb_cmds.init_metadata_storage_module(*cfg, &uid);
  if (ms == nullptr) {
    i_error(" Error initializing metadata module ");
    delete ms;
    ctx->exit_code = -1;
    return 0;
  }

  for (auto mo : mail_objects) {
    std::cout << mo->to_string("  ") << std::endl;
    if (open >= 0 && ctx_->delete_not_referenced_objects) {
      std::cout << "mail object: " << mo->get_oid().c_str()
                << " deleted: " << (plugin.storage->delete_mail(mo) < 0 ? " FALSE " : " TRUE") << std::endl;
      ctx->exit_code = 2;
     }
     delete mo;
  }
  delete ms;

  return 0;
}

static int i_strcmp_reverse_p(const char *const *s1, const char *const *s2) { return -strcmp(*s1, *s2); }
static int get_child_mailboxes(struct mail_user *user, ARRAY_TYPE(const_string) * mailboxes, const char *name) {
  struct mailbox_list_iterate_context *iter;
  struct mail_namespace *ns;
  const struct mailbox_info *info;
  const char *pattern, *child_name;

  ns = mail_namespace_find(user->namespaces, name);
  pattern = name[0] == '\0' ? "*" : t_strdup_printf("%s%c*", name, mail_namespace_get_sep(ns));
  iter = mailbox_list_iter_init(ns->list, pattern, MAILBOX_LIST_ITER_RETURN_NO_FLAGS);
  while ((info = mailbox_list_iter_next(iter)) != NULL) {
    child_name = t_strdup(info->vname);
    array_append(mailboxes, &child_name, 1);
  }
  return mailbox_list_iter_deinit(&iter);
}

static int cmd_mailbox_delete_run(struct doveadm_mail_cmd_context *_ctx, struct mail_user *user) {
  struct delete_cmd_context *ctx = (struct delete_cmd_context *)_ctx;
  struct mail_namespace *ns;
  struct mailbox *box;

  if (!_ctx->iterate_single_user) {
    i_error("delete command is only available for single user");
    _ctx->exit_code = -1;
    return 0;
  }

  const char *const *namep;
  ARRAY_TYPE(const_string) recursive_mailboxes;
  const ARRAY_TYPE(const_string) *mailboxes = &ctx->mailboxes;
  enum mailbox_flags mailbox_flags = static_cast<enum mailbox_flags>(0);
  int ret = 0, ret2;
#if DOVECOT_PREREQ(2, 3)
  if (ctx->unsafe)
    mailbox_flags |= MAILBOX_FLAG_DELETE_UNSAFE;
#endif

  i_debug("cmd_mailbox_delete_run");

  if (ctx->recursive) {
    t_array_init(&recursive_mailboxes, 32);
    array_foreach(&ctx->mailboxes, namep) {
      if (get_child_mailboxes(user, &recursive_mailboxes, *namep) < 0) {
        doveadm_mail_failed_error(_ctx, MAIL_ERROR_TEMP);
        ret = -1;
      }
      if ((*namep)[0] != '\0')
        array_append(&recursive_mailboxes, namep, 1);
    }
    array_sort(&recursive_mailboxes, i_strcmp_reverse_p);
    mailboxes = &recursive_mailboxes;
  }
  array_foreach(mailboxes, namep) {
    const char *name = *namep;
    ns = mail_namespace_find(user->namespaces, name);
    box = mailbox_alloc(ns->list, name, mailbox_flags);
#if DOVECOT_PREREQ(2, 3)
    mailbox_set_reason(box, _ctx->cmd->name);
    struct mail_storage *storage = mailbox_get_storage(box);
#endif
    ret2 = ctx->require_empty ? mailbox_delete_empty(box) : mailbox_delete(box);
    if (ret2 < 0) {
#if DOVECOT_PREREQ(2, 3)
      i_error("Can't delete mailbox %s: %s", name, mailbox_get_last_internal_error(box, NULL));
#else
      i_error("Can't delete mailbox %s %d", name, ret2);
#endif
      doveadm_mail_failed_mailbox(_ctx, box);
      ret = -1;
    }
    if (ctx->subscriptions) {
      if (mailbox_set_subscribed(box, FALSE) < 0) {
#if DOVECOT_PREREQ(2, 3)
        i_error("Can't unsubscribe mailbox %s: %s", name, mail_storage_get_last_internal_error(storage, NULL));
#else
        i_error("Can't unsubscribe mailbox %s ", name);
#endif
        doveadm_mail_failed_mailbox(_ctx, box);
        ret = -1;
      }
    }
    mailbox_free(&box);
  }
  _ctx->exit_code = ret;
  return 0;
}


static int cmd_rmb_mailbox_delete_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  int ret = cmd_mailbox_delete_run(ctx, user);
  if (ret == 0 && ctx->exit_code == 0) {
    RboxDoveadmPlugin plugin;
    i_debug("cleaning up rbox specific files and objects :ret=%d", ret);
    plugin.read_plugin_configuration(user);
    int open = open_connection_load_config(&plugin);
    if (open < 0) {
      i_error("error opening rados connection, check config: %d", open);
      ctx->exit_code = open;
      return 0;
    }
    std::map<std::string, std::string> opts;
    opts["namespace"] = user->username;
    librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);

    std::string uid;
    librmb::RadosCephConfig *cfg =
        (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

    if (cfg->is_user_mapping()) {
      // we need to delete the namespace object.
      // iterate over all mailboxes, if we have no more mails, delete the user namespace object
      // for the current user.
      librmb::RadosNamespaceManager mgr(plugin.config);
      check_users_mailbox_delete_ns_object(user, plugin.config, &mgr, plugin.storage);
    }
  }
  return 0;
}

static void cmd_rmb_ls_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] == NULL) {
    doveadm_mail_help_name("rmb ls");
  }
}
static void cmd_rmb_get_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] == NULL || args[1] == NULL) {
    doveadm_mail_help_name("rmb get");
  }
}
static void cmd_rmb_delete_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] == NULL) {
    doveadm_mail_help_name("rmb delete");
  }
}
static void cmd_rmb_set_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] == NULL || args[1] == NULL) {
    doveadm_mail_help_name("rmb set");
  }
}

static void cmd_rmb_ls_mb_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] != NULL) {
    doveadm_mail_help_name("rmb ls mb");
  }
}
static void cmd_rmb_rename_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] == NULL) {
    doveadm_mail_help_name("rmb rename");
  }
}
static void cmd_rmb_revert_log_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] == NULL) {
    doveadm_mail_help_name("rmb revert");
  }
}

static void cmd_rmb_check_indices_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (args[0] != NULL) {
    doveadm_mail_help_name("rmb check indices");
  }
}
static void cmd_rmb_mailbox_delete_init(struct doveadm_mail_cmd_context *_ctx ATTR_UNUSED, const char *const args[]) {
  struct delete_cmd_context *ctx = (struct delete_cmd_context *)_ctx;
  const char *name;
  unsigned int i;

  if (args[0] == NULL){
    doveadm_mail_help_name("rmb mailbox delete");
  }
  doveadm_mailbox_args_check(args);
  for (i = 0; args[i] != NULL; i++) {
    name = p_strdup(ctx->pool, args[i]);
    array_append(&ctx->mailboxes, &name, 1);
  }
  array_sort(&ctx->mailboxes, i_strcmp_reverse_p);
}

struct doveadm_mail_cmd_context *cmd_rmb_ls_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_ls_run;
  ctx->v.init = cmd_rmb_ls_init;
  return ctx;
}

struct doveadm_mail_cmd_context *cmd_rmb_get_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_get_run;
  ctx->v.init = cmd_rmb_get_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_set_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_set_run;
  ctx->v.init = cmd_rmb_set_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_delete_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_delete_run;
  ctx->v.init = cmd_rmb_delete_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_ls_mb_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_ls_mb_run;
  ctx->v.init = cmd_rmb_ls_mb_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_rename_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_rename_run;
  ctx->v.init = cmd_rmb_rename_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_revert_log_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_revert_log_run;
  ctx->v.init = cmd_rmb_revert_log_init;
  return ctx;
}



static bool cmd_check_indices_parse_arg(struct doveadm_mail_cmd_context *_ctx, int c) {
  struct check_indices_cmd_context *ctx = (struct check_indices_cmd_context *)_ctx;

  switch (c) {
    case 'd':
      ctx->delete_not_referenced_objects = true;
      break;
    default:
      break;
  }
  return true;
}

struct doveadm_mail_cmd_context *cmd_rmb_check_indices_alloc(void) {
  struct check_indices_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct check_indices_cmd_context);
  ctx->ctx.v.run = cmd_rmb_check_indices_run;
  ctx->ctx.v.init = cmd_rmb_check_indices_init;
  ctx->ctx.v.parse_arg = cmd_check_indices_parse_arg;
  ctx->ctx.getopt_args = "d";
  return &ctx->ctx;
}

static bool cmd_mailbox_delete_parse_arg(struct doveadm_mail_cmd_context *_ctx, int c) {
  struct delete_cmd_context *ctx = (struct delete_cmd_context *)_ctx;

  switch (c) {
    case 'r':
      ctx->recursive = TRUE;
      break;
    case 's':
      ctx->subscriptions = TRUE;
      break;
    case 'e':
      ctx->require_empty = TRUE;
      break;
#if DOVECOT_PREREQ(2, 3)
    case 'Z':
      ctx->unsafe = TRUE;
      break;
#endif
    default:
      i_debug("unkown iption");
      return FALSE;
  }
  return TRUE;
}



struct doveadm_mail_cmd_context *cmd_rmb_mailbox_delete_alloc(void) {
  struct delete_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct delete_cmd_context);
  ctx->ctx.v.run = cmd_rmb_mailbox_delete_run;
  ctx->ctx.v.init = cmd_rmb_mailbox_delete_init;
  ctx->ctx.v.parse_arg = cmd_mailbox_delete_parse_arg;
  ctx->ctx.getopt_args = "rs";
  ctx->pool = pool_alloconly_create("doveadm mailbox delete pool", 512);
  p_array_init(&ctx->mailboxes, ctx->ctx.pool, 16);
  return &ctx->ctx;
}
struct config_options {
  char *user_name;
  char *pool_name;
};

int cmd_rmb_config_show(int argc, char *argv[]) {
  std::map<std::string, std::string> opts;
  opts["print_cfg"] = "true";
  return cmd_rmb_config(opts);
}
int cmd_rmb_config_create(int argc, char *argv[]) {
  RboxDoveadmPlugin plugin;
  plugin.read_doveadm_plugin_configuration();
  int open = plugin.open_connection();
  if (open < 0) {
    i_error("Error opening rados connection. Errorcode: %d", open);
		return -1;
  }
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();
  int ret = cfg->load_cfg();
  if (ret < 0) {
    ret = cfg->save_cfg();
    if (ret < 0) {
      i_error("error creating configuration %d", ret);
			return -1;
    }
    std::cout << "config created" << std::endl;
  } else {
    std::cout << "config already exist" << std::endl;
    return 1;
  }
  return 0;
}

int cmd_rmb_config_update(int argc, char *argv[]) {
  if (argc < 1) {
    i_error("usage: dovecot rmb config update key=value");
    return -1;
  }
  char *update = argv[1];
  if (update == NULL) {
    i_error("no update param given");
    return -1;
  }
  std::map<std::string, std::string> opts;
  opts["update"] = update;
  int ret = cmd_rmb_config(opts);
  return ret;
}

int cmd_rmb_lspools(int argc, char *argv[]) { return librmb::RmbCommands::RmbCommands::lspools(); }
