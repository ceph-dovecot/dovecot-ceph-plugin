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
}
#include "tools/rmb/rmb-commands.h"
#include "rados-cluster.h"
#include "rados-cluster-impl.h"
#include "rados-storage.h"
#include "rados-storage-impl.h"
#include "rados-dovecot-ceph-cfg.h"
#include "rados-dovecot-ceph-cfg-impl.h"
#include "rbox-storage.h"
#include "rbox-save.h"
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
  }
  int read_plugin_configuration(struct mail_user *user) {
    std::map<std::string, std::string> *map = config->get_config();
    for (std::map<std::string, std::string>::iterator it = map->begin(); it != map->end(); ++it) {
      std::string setting = it->first;
      const char *value = mail_user_plugin_getenv(user, setting.c_str());
      i_debug("read config:%s value: %s", setting.c_str(), value);
      config->update_metadata(setting, value);
    }
    config->set_config_valid(true);
    return 0;
  }

 public:
  librmb::RadosCluster *cluster;
  librmb::RadosStorage *storage;
  librmb::RadosDovecotCephCfg *config;
};

static int cmd_rmb_lspools_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  //  const char *ns_prefix = ctx->args[0];

  librmb::RmbCommands::RmbCommands::lspools();
  return 0;
}

static int open_connection_load_config(RboxDoveadmPlugin *plugin, struct mail_user *user) {
  int ret = -1;

  plugin->read_plugin_configuration(user);
  int open = plugin->open_connection();
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
  }
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin->config))->get_rados_ceph_cfg();
  ret = cfg->load_cfg();
  if (ret < 0) {
    i_error("error accessing configuration");
    return ret;
  }
  return ret;
}

static int cmd_rmb_config(std::map<std::string, std::string> &opts, struct mail_user *user) {
  RboxDoveadmPlugin plugin;
  plugin.read_plugin_configuration(user);
  int open = open_connection_load_config(&plugin, user);
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
  }
  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();
  rmb_cmds.configuration(true, *cfg);
  return 0;
}

static int cmd_rmb_search_run(std::map<std::string, std::string> &opts, struct mail_user *user, bool download,
                              librmb::CmdLineParser &parser) {
  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin, user);
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
  }

  opts["namespace"] = user->username;
  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);

  std::string uid;
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();

  librmb::RadosStorageMetadataModule *ms = rmb_cmds.init_metadata_storage_module(*cfg, &uid);
  if (ms == nullptr) {
    i_error(" Error initializing metadata module ");
    delete ms;
    return -1;
  }
  std::vector<librmb::RadosMailObject *> mail_objects;
  rmb_cmds.load_objects(ms, mail_objects, opts["sort"]);

  if (download) {
    rmb_cmds.set_output_path(&parser);
    rmb_cmds.query_mail_storage(&mail_objects, &parser, download);
  } else {
    rmb_cmds.query_mail_storage(&mail_objects, &parser, download);
  }
  delete ms;

  return 0;
}

static int cmd_rmb_config_show_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  std::map<std::string, std::string> opts;
  opts["print_cfg"] = "true";
  return cmd_rmb_config(opts, user);
}

static int cmd_rmb_config_update_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *update = ctx->args[0];

  if (update == NULL) {
    i_error("no update param given");
    return -1;
  }
  std::map<std::string, std::string> opts;
  opts["update"] = update;
  return cmd_rmb_config(opts, user);
}
static int cmd_rmb_config_create_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  RboxDoveadmPlugin plugin;
  plugin.read_plugin_configuration(user);
  int open = plugin.open_connection();
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
  }
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();
  int ret = cfg->load_cfg();
  if (ret < 0) {
    ret = cfg->save_cfg();
    if (ret < 0) {
      i_error("error creating configuration");
      return ret;
    }
    std::cout << "config created" << std::endl;
  } else {
    std::cout << "config already exist" << std::endl;
  }
  return 0;
}

static int cmd_rmb_ls_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  int ret = 1;
  const char *search_query = ctx->args[0];
  const char *sort = ctx->args[1];

  if (search_query == NULL) {
    i_error("no search query given");
    return -1;
  }

  std::map<std::string, std::string> opts;
  opts["ls"] = search_query;
  opts["sort"] = sort != NULL ? sort : "uid";
  librmb::CmdLineParser parser(opts["ls"]);

  if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
    ret = cmd_rmb_search_run(opts, user, false, parser);
  } else {
    i_error("invalid ls search query, %s", search_query);
  }
  return ret;
}
static int cmd_rmb_ls_mb_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  int ret = -1;
  std::map<std::string, std::string> opts;
  opts["ls"] = "mb";
  opts["sort"] = "uid";
  librmb::CmdLineParser parser(opts["ls"]);
  if (opts["ls"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
    ret = cmd_rmb_search_run(opts, user, false, parser);
  } else {
    i_error("invalid ls search query");
  }
  return ret;
}

static int cmd_rmb_get_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  int ret = -1;
  const char *search_query = ctx->args[0];
  const char *output_path = ctx->args[1];

  if (search_query == NULL) {
    i_error("no search query given");
    return -1;
  }

  std::map<std::string, std::string> opts;
  opts["get"] = search_query;
  if (output_path != NULL) {
    opts["out"] = output_path;
  }
  opts["sort"] = "uid";

  librmb::CmdLineParser parser(opts["get"]);
  if (opts["get"].compare("all") == 0 || opts["ls"].compare("-") == 0 || parser.parse_ls_string()) {
    ret = cmd_rmb_search_run(opts, user, true, parser);
  } else {
    i_error("invalid search query %s", search_query);
  }
  return ret;
}
static int cmd_rmb_set_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *oid = ctx->args[0];
  const char *key_value_pair = ctx->args[1];
  int ret = -1;
  if (oid == NULL || key_value_pair == NULL) {
    i_error("error check params: %s : %s ", oid, key_value_pair);
    return -1;
  }

  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin, user);
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
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
    return -1;
  }
  std::map<std::string, std::string> metadata;
  std::stringstream left(key_value_pair);
  std::vector<std::string> tokens;
  std::string item;
  while (std::getline(left, item, '=')) {
    tokens.push_back(item);
  }
  if (tokens.size() == 2) {
    metadata[tokens[0]] = tokens[1];
    ret = rmb_cmds.update_attributes(ms, &metadata);
    std::cout << " token " << tokens[0] << " : " << tokens[1] << std::endl;
  } else {
    std::cerr << "check params: key_value_pair(" << key_value_pair << ") is not valid: use key=value" << std::endl;
  }
  delete ms;
  return ret;
}

static int cmd_rmb_delete_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *oid = ctx->args[0];

  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin, user);
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
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
    return -1;
  }

  int ret = rmb_cmds.delete_mail(true);
  delete ms;
  return ret;
}
static int cmd_rmb_rename_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *new_user_name = ctx->args[0];

  RboxDoveadmPlugin plugin;
  int open = open_connection_load_config(&plugin, user);
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
  }

  std::map<std::string, std::string> opts;
  opts["to_rename"] = new_user_name;
  opts["namespace"] = user->username;

  librmb::RmbCommands rmb_cmds(plugin.storage, plugin.cluster, &opts);
  std::string uid;
  librmb::RadosCephConfig *cfg = (static_cast<librmb::RadosDovecotCephCfgImpl *>(plugin.config))->get_rados_ceph_cfg();
  librmb::RadosStorageMetadataModule *ms = rmb_cmds.init_metadata_storage_module(*cfg, &uid);
  if (ms == nullptr) {
    i_error(" Error initializing metadata module ");
    delete ms;
    return -1;
  }

  int ret = rmb_cmds.rename_user(cfg, true, uid);
  delete ms;
  return ret;
}
static int restore_index_entry(struct mail_user *user, const char *mailbox_name, const std::string &str_mail_guid,
                               const std::string &str_mail_oid) {
  struct mail_namespace *ns = mail_namespace_find_inbox(user->namespaces);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox_name, MAILBOX_FLAG_READONLY);
  if (mailbox_open(box) < 0) {
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
                                      struct mail_user *cur_mail_user, const char **error_r,
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
  ret = mail_storage_service_next(ctx->storage_service, cur_service_user, &cur_mail_user, error_r);

#else
  ret = mail_storage_service_next(ctx->storage_service, cur_service_user, &cur_mail_user);
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

    guid_128_t mail_guid, mail_oid;

    std::list<librmb::RadosMetadata>::iterator it_guid =
        std::find_if(it->metadata.begin(), it->metadata.end(),
                     [key_guid](librmb::RadosMetadata const &m) { return m.key == key_guid; });
    std::list<librmb::RadosMetadata>::iterator it_mb =
        std::find_if(it->metadata.begin(), it->metadata.end(),
                     [key_mbox_name](librmb::RadosMetadata const &m) { return m.key == key_mbox_name; });
   
    restore_index_entry(cur_mail_user, (*it_mb).bl.to_str().c_str(), (*it_guid).bl.to_str(), it->src_oid);
  }
  mail_user_unref(&cur_mail_user);

#if DOVECOT_PREREQ(2, 3)
  mail_storage_service_user_unref(&cur_service_user);
#else
  mail_storage_service_user_free(&cur_service_user);
#endif
  return 1;
}

static int cmd_rmb_save_log_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  const char *log_file = ctx->args[0];

  RboxDoveadmPlugin plugin;
  plugin.read_plugin_configuration(user);
  int open = plugin.open_connection();
  if (open < 0) {
    i_error("error opening rados connection, check config: %d", open);
    return open;
  }
  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;
  int ret = librmb::RmbCommands::delete_with_save_log(log_file, plugin.config->get_rados_cluster_name(),
                                                      plugin.config->get_rados_username(), &moved_items);

  for (std::map<std::string, std::list<librmb::RadosSaveLogEntry>>::iterator iter = moved_items.begin();
       iter != moved_items.end(); ++iter) {
    const char *error;
    struct mail_storage_service_user *cur_service_user;
    struct mail_user *cur_mail_user;
    ctx->storage_service_input.username = iter->first.c_str();
    doveadm_rmb_mail_next_user(ctx, &ctx->storage_service_input, cur_service_user, cur_mail_user, &error,
                               &iter->second);
  }

  return ret;
}

static void cmd_rmb_lspools_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 0) {
    doveadm_mail_help_name("rmb lspools");
  }
}
static void cmd_rmb_config_update_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 1) {
    doveadm_mail_help_name("rmb config update key=value");
  }
}
static void cmd_rmb_ls_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 2) {
    doveadm_mail_help_name("rmb ls <-|key=value> <uid|recv_date|save_date|phy_size>");
  }
}
static void cmd_rmb_get_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 3) {
    doveadm_mail_help_name("rmb get <-|key=value> <output path> <uid|recv_date|save_date|phy_size>");
  }
}
static void cmd_rmb_delete_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 1) {
    doveadm_mail_help_name("rmb delete <oid>");
  }
}
static void cmd_rmb_set_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 3) {
    doveadm_mail_help_name("rmb set <oid> <key=value> ");
  }
}

static void cmd_rmb_ls_mb_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 1) {
    doveadm_mail_help_name("rmb ls mb -u <user> ");
  }
}
static void cmd_rmb_rename_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 1) {
    doveadm_mail_help_name("rmb rename <username> -u <user> ");
  }
}
static void cmd_rmb_save_log_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 1) {
    doveadm_mail_help_name("rmb save_log path to save_log");
  }

}
struct doveadm_mail_cmd_context *cmd_rmb_lspools_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_lspools_run;
  ctx->v.init = cmd_rmb_lspools_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_config_show_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_config_show_run;
  ctx->v.init = cmd_rmb_lspools_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_config_create_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_config_create_run;
  ctx->v.init = cmd_rmb_lspools_init;
  return ctx;
}
struct doveadm_mail_cmd_context *cmd_rmb_config_update_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_config_update_run;
  ctx->v.init = cmd_rmb_config_update_init;
  return ctx;
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
struct doveadm_mail_cmd_context *cmd_rmb_save_log_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_save_log_run;
  ctx->v.init = cmd_rmb_save_log_init;
  return ctx;
}
