
// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#include <list>
extern "C" {
#include "dovecot-all.h"

#include "rbox-sync.h"
#include "debug-helper.h"
#include "data-stack.h"
}

#include "rbox-sync-rebuild.h"

#include "rbox-storage.hpp"
#include "rbox-mail.h"
#include "encoding.h"
#include "../librmb/rados-mail.h"
#include "../librmb/rados-util.h"
#include "rados-types.h"


using librmb::RadosMail;
using librmb::rbox_metadata_key;

int rbox_sync_add_object(struct index_rebuild_context *ctx, const std::string &oi, librmb::RadosMail *mail_obj,
                         bool alt_storage, uint32_t next_uid) {
  FUNC_START();
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)ctx->box;
  char *xattr_mail_uid = NULL;
  librmb::RadosUtils::get_metadata(rbox_metadata_key::RBOX_METADATA_MAIL_UID, mail_obj->get_metadata(),
                                   &xattr_mail_uid);
  char *xattr_guid = NULL;
  librmb::RadosUtils::get_metadata(rbox_metadata_key::RBOX_METADATA_GUID, mail_obj->get_metadata(), &xattr_guid);
  struct mail_storage *storage = ctx->box->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;
  uint32_t seq;

  mail_index_append(ctx->trans, next_uid, &seq);

  T_BEGIN { 
    uint32_t uid = std::stoi(xattr_mail_uid);
    // uid = INT32_MAX if a previous force-resync detected, that the mail object has a mailbox guid which 
    //       is no longer valid.
    if(uid != INT32_MAX && !mail_obj->is_lost_object()){
      // there should be a previous index entry available (if index exist)
      index_rebuild_index_metadata(ctx, seq, uid); }
    }
  T_END;

#ifdef DEBUG
  i_debug("added to index %d", next_uid);
#endif
  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  i_zero(&rec);
  // convert oid and guid to
  guid_128_t oid;
  if (guid_128_from_string(oi.c_str(), oid) < 0) {
      i_error("converting oid failed : guid_128 oi.c_str() string (%s), next_uid(%d)", oi.c_str(), next_uid);
      FUNC_END();
      return -1;
  }
  guid_128_t guid;
  if (guid_128_from_uuid_string(xattr_guid, guid) < 0) {
      i_error("converting guid failed : guid_128 oi.c_str() string (%s), next_uid(%d)", oi.c_str(), next_uid);
      FUNC_END();
      return -1; 
  }

  memcpy(rec.guid, guid, sizeof(guid));
  memcpy(rec.oid, oid, sizeof(oid));

  mail_index_update_ext(ctx->trans, seq, rbox->ext_id, &rec, NULL);
  if (alt_storage) {
    mail_index_update_flags(ctx->trans, seq, MODIFY_ADD, (enum mail_flags)RBOX_INDEX_FLAG_ALT);
  }

  // update uid.
  librmb::RadosMetadata mail_uid(librmb::RBOX_METADATA_MAIL_UID, next_uid);
  std::string s_oid = *mail_obj->get_oid();
  std::list<librmb::RadosMetadata> to_update;
  to_update.push_back(mail_uid);
  if (!r_storage->ms->get_storage()->update_metadata(s_oid, to_update)) {
    i_warning("update of MAIL_UID failed: for object: %s , uid: %d", mail_obj->get_oid()->c_str(), next_uid);
  }
#ifdef DEBUG
    i_debug("rebuilding %s , with uid=%d", oi.c_str(), next_uid);
#endif
  FUNC_END();
  return 0;
}

std::map<std::string, std::list<librmb::RadosMail>> load_rados_mail_metadata(
            bool alt_storage,     
            struct rbox_storage *r_storage,
            librados::NObjectIterator &iter) {
  
  std::map<std::string, std::list<librmb::RadosMail>> rados_mails;

  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    
    librmb::RadosMail mail_object;
    mail_object.set_oid((*iter).get_oid());

    int load_metadata_ret;
    if (alt_storage) {
      r_storage->ms->get_storage()->set_io_ctx(&r_storage->alt->get_io_ctx());
      load_metadata_ret = r_storage->ms->get_storage()->load_metadata(&mail_object);
    } else {
      load_metadata_ret = r_storage->ms->get_storage()->load_metadata(&mail_object);
    }
   
    if (load_metadata_ret < 0 || !librmb::RadosUtils::validate_metadata(mail_object.get_metadata())) {    
      i_warning("metadata for object : %s is not valid, skipping object ", mail_object.get_oid()->c_str());
      ++iter;
      continue;
    }
    
    if (load_metadata_ret >= 0) {
      char *mailbox_guid = NULL;
      librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, 
                                       mail_object.get_metadata(), 
                                       &mailbox_guid
                                      );
      std::string mails_guid(mailbox_guid);

      if(rados_mails.count(mailbox_guid)){
        rados_mails[mailbox_guid].push_back(mail_object);
      }else{
        std::list<librmb::RadosMail> list_mail_objects;
        list_mail_objects.push_back(mail_object);
        rados_mails[mailbox_guid]= list_mail_objects;
      }
    }
    ++iter;   
  }
  return rados_mails;
}

// find objects with mailbox_guid 'U' attribute
int rbox_sync_rebuild_entry(struct index_rebuild_context *ctx, std::map<std::string, std::list<librmb::RadosMail>> &rados_mails,
                            struct rbox_sync_rebuild_ctx *rebuild_ctx) {
  FUNC_START();
  struct mail_storage *storage = ctx->box->storage;

  struct rbox_mailbox *rbox = (struct rbox_mailbox *)ctx->box;

  const struct mail_index_header *hdr = mail_index_get_header(ctx->trans->view);

  if (rebuild_ctx->next_uid == INT_MAX) {
    rebuild_ctx->next_uid = hdr->next_uid != 0 ? hdr->next_uid : 1;
  }

  int sync_add_objects_ret = 0;
  
  std::string mailbox_guid(guid_128_to_string(rbox->mailbox_guid));  

  if(!rados_mails.count(mailbox_guid)){
      i_info("no mails for mailbox guid: %s",mailbox_guid.c_str());
      FUNC_END();
      return 0;
  }
  
  std::list<librmb::RadosMail>::iterator it;
  for(it=rados_mails[mailbox_guid].begin(); it!=rados_mails[mailbox_guid].end(); ++it){   
    
    if(it->is_restored()){
      // if this is second run, do not add the mail again.
      i_debug("skipping already restored mail! oid: %s",it->get_oid()->c_str());
      continue; 
    }  
    
    sync_add_objects_ret =
        rbox_sync_add_object(ctx, *it->get_oid(), &(*it), rebuild_ctx->alt_storage, rebuild_ctx->next_uid);
    i_info("re-adding mail oid:(%s) with uid: %d to mailbox %s (%s) ", it->get_oid()->c_str(), rebuild_ctx->next_uid, mailbox_guid.c_str(), ctx->box->name );

    if (sync_add_objects_ret < 0) {
      i_error("sync_add_object: oid(%s), alt_storage(%d),uid(%d)", it->get_oid()->c_str(),
              rebuild_ctx->alt_storage, rebuild_ctx->next_uid);
      break;
    }
    it->set_restored(true);
    it->set_index_ref(true);
    i_debug("restored rados_mail: %s",it->to_string(" ").c_str());  

    rebuild_ctx->next_uid++;

  }

  if (sync_add_objects_ret < 0) {
    i_error("error rbox_sync_add_objects for mbox %s", ctx->box->name);
    mailbox_set_deleted(ctx->box);
    mail_storage_set_critical(storage, "find mailbox(%s) failed: %m", ctx->box->name);
    FUNC_END();
    return -1;
  }

  FUNC_END();
  return sync_add_objects_ret;
}

void rbox_sync_set_uidvalidity(struct index_rebuild_context *ctx) {
  FUNC_START();
  uint32_t uid_validity;

  /* if uidvalidity is set in the old index, use it */
  uid_validity = mail_index_get_header(ctx->view)->uid_validity;
  if (uid_validity == 0)
    uid_validity = rbox_get_uidvalidity_next(ctx->box->list);

  mail_index_update_header(ctx->trans, offsetof(struct mail_index_header, uid_validity), &uid_validity,
                           sizeof(uid_validity), TRUE);
  FUNC_END();
}

void rbox_sync_update_header(struct index_rebuild_context *ctx) {
  FUNC_START();
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)ctx->box;
  struct rbox_index_header hdr;
  bool need_resize;

  if (rbox_read_header(rbox, &hdr, FALSE, &need_resize) < 0)
    i_zero(&hdr);
  if (guid_128_is_empty(hdr.mailbox_guid))
    guid_128_generate(hdr.mailbox_guid);
  if (++hdr.rebuild_count == 0)
    hdr.rebuild_count = 1;
  /* mailbox is being reset. this gets written directly there */
  mail_index_set_ext_init_data(ctx->box->index, rbox->hdr_ext_id, &hdr, sizeof(hdr));
  FUNC_END();
}

int rbox_sync_index_rebuild_objects(struct index_rebuild_context *ctx, std::map<std::string, std::list<librmb::RadosMail>> &rados_mails) {
  FUNC_START();
  
  int ret = 0;
  pool_t pool;

  rbox_sync_set_uidvalidity(ctx);
  struct rbox_sync_rebuild_ctx *rebuild_ctx;

  pool = pool_alloconly_create("rbox rebuild pool", 256);

  rebuild_ctx = p_new(pool, struct rbox_sync_rebuild_ctx, 1);
  i_zero(rebuild_ctx);
  rebuild_ctx->alt_storage = false;
  rebuild_ctx->next_uid = INT_MAX;

  ret = rbox_sync_rebuild_entry(ctx, rados_mails, rebuild_ctx);

  rbox_sync_update_header(ctx);
  pool_unref(&pool);
  
  FUNC_END();
  return ret;
}

int rbox_storage_rebuild_in_context(struct rbox_storage *r_storage, bool force, bool firstTry) {
  FUNC_START();

  struct mail_user *user = r_storage->storage.user;
  std::map<std::string, std::list<librmb::RadosMail>> rados_mails;

  struct mail_namespace *ns = mail_namespace_find_inbox(user->namespaces);

  struct mail_namespace *ns_mailbox = nullptr;
  //TODO: save inbox namespace:
  //      assign unassigned mails to inbox  
  for (; ns != NULL; ns = ns->next) {
    // iter_guid will be initialized in repair_namespace,
    // as we first need to call open_mailbox to initialize
    // the rados_connection successfully and list objects in
    // the user namespace

    repair_namespace(ns, force, r_storage, rados_mails);
  }

  i_info("Repair done checking for unassigned mails ");
  std::map<std::string, std::list<librmb::RadosMail>>::iterator it;
  long count_not_assigned = 0;
  long count_assigned = 0;
  for(it=rados_mails.begin(); it!=rados_mails.end(); ++it){      
    std::list<librmb::RadosMail>::iterator list_it;
    for(list_it=it->second.begin(); list_it!=it->second.end(); ++list_it){
      count_not_assigned += list_it->is_restored() ? 0 : 1;
      count_assigned += list_it->is_restored() ? 1 : 0;

    }    
  }
  i_info("total unassigned mails %ld", count_not_assigned);

  i_info("Total assigned mails %ld", count_assigned);
  i_info("Total unassigned mails %ld", count_not_assigned);

  if(count_not_assigned > 0) 
  {
    std::string last_known_mailbox_guid;
    struct mail_namespace *ns_second = mail_namespace_find_inbox(user->namespaces);

    if(find_default_mailbox_guid(ns_second, &last_known_mailbox_guid) < 0)
    {
      // bad no default mailbox found
      i_warning("unable to find inbox guid => unreferenced mails can not automatically be re-assigned ");
      return 0;
    }
    int unassigned_counter = 0;
    for(it=rados_mails.begin(); it!=rados_mails.end(); ++it)
    {      
      std::list<librmb::RadosMail>::iterator list_it;
      for(list_it=it->second.begin(); list_it!=it->second.end(); ++list_it){
        if(list_it->is_restored()){
          continue;
        }
        librmb::RadosMetadata metadata;
        metadata.convert(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID, last_known_mailbox_guid);
    
        librmb::RadosMetadata metadata_uid;
        metadata_uid.convert(rbox_metadata_key::RBOX_METADATA_MAIL_UID, INT32_MAX);
  
        librados::ObjectWriteOperation write_mail_uid;
        write_mail_uid.setxattr(metadata_uid.key.c_str(), metadata_uid.bl);
        write_mail_uid.setxattr(metadata.key.c_str(), metadata.bl);

        if (r_storage->s->get_io_ctx().operate(*list_it->get_oid(), &write_mail_uid) < 0) {
            i_info("Unable to reset metadata to guid : %s",last_known_mailbox_guid.c_str());
        }else {
            i_info("(%d) Mailbox guid for mail (oid=%s) restored to %s (INBOX) => re-run force-resync to assign them ",unassigned_counter, list_it->get_oid()->c_str(),last_known_mailbox_guid.c_str());
        }
        unassigned_counter++;
        list_it->set_lost_object(true);
      }
    }
    if(unassigned_counter>0){
      if(firstTry){
        // try again.... but only once, as we do not want to end up in an endless loop.
        return rbox_storage_rebuild_in_context(r_storage,force, false); 
      }else{
        i_error("still unassigned mail objects in ceph namespace, manual intervention required.");
        return -1; 
      }
      
    }
    
  }
  
  FUNC_END();
  return 0;
}

int find_default_mailbox_guid(struct mail_namespace *ns, std::string *mailbox_guid) {
  for (; ns != NULL; ns = ns->next) {
    if(find_inbox_mailbox_guid(ns, mailbox_guid) == 0 ){
       return 0;
    }
  }
  // none found
  return -1;
}

int find_inbox_mailbox_guid(struct mail_namespace *ns, std::string *mailbox_guid) {
  FUNC_START();
  struct mailbox_list_iterate_context *iter;
  const struct mailbox_info *info;
  int ret = -1;    
  
  iter = mailbox_list_iter_init(ns->list, "*", static_cast<mailbox_list_iter_flags>(MAILBOX_LIST_ITER_RAW_LIST |
                                                                                    MAILBOX_LIST_ITER_RETURN_NO_FLAGS));
  while ((info = mailbox_list_iter_next(iter)) != NULL) {
    i_debug("checking mailbox %s",info->vname);
      
    if ((info->flags & (MAILBOX_NONEXISTENT | MAILBOX_NOSELECT)) == 0) {

      struct mailbox *box = mailbox_alloc(ns->list, info->vname, MAILBOX_FLAG_SAVEONLY);
      if (box->virtual_vfuncs != NULL) {
        /* the namespace has multiple storages. or is virtual box */
        mailbox_free(&box);
        return -1;
      }

      if (mailbox_open(box) < 0) {
        FUNC_END();
        return -1;
      }
      
      if (strcmp(info->vname, "INBOX") == 0){
        char* guid = guid_128_to_string(((struct rbox_mailbox *)box)->mailbox_guid);  
        i_info("found default INBOX mailbox with guid: %s",guid);
        *mailbox_guid = guid;
        ret = 0;
      }
      mailbox_free(&box);
    }
  }
 
  if (mailbox_list_iter_deinit(&iter) < 0) {
    ret = -1;
  }

  FUNC_END();
  return ret;
}


int repair_namespace(struct mail_namespace *ns, bool force, struct rbox_storage *r_storage, std::map<std::string, std::list<librmb::RadosMail>> &rados_mails) {
  FUNC_START();
  struct mailbox_list_iterate_context *iter;
  const struct mailbox_info *info;
  int ret = 0;

  iter = mailbox_list_iter_init(ns->list, "*", static_cast<mailbox_list_iter_flags>(MAILBOX_LIST_ITER_RAW_LIST |
                                                                                    MAILBOX_LIST_ITER_RETURN_NO_FLAGS));
  while ((info = mailbox_list_iter_next(iter)) != NULL) {
    
    if ((info->flags & (MAILBOX_NONEXISTENT | MAILBOX_NOSELECT)) == 0) {

      struct mailbox *box = mailbox_alloc(ns->list, info->vname, MAILBOX_FLAG_SAVEONLY);
      if (box->storage != &r_storage->storage ||
          box->virtual_vfuncs != NULL) {
        /* the namespace has multiple storages. or is virtual box */
        mailbox_free(&box);
        return 0;
      }

      if (mailbox_open(box) < 0) {
        FUNC_END();
        return -1;
      }

      mail_index_lock_sync(box->index, "LOCKED_FOR_REPAIR");
      
      if(rados_mails.size() == 0) {
        if (rbox_open_rados_connection(box, false) < 0) {
          i_error("rbox_sync_index_rebuild_objects: cannot open rados connection");
          FUNC_END();
          return -1;
        }
        i_info("Ceph connection established using namespace: %s",r_storage->s->get_namespace().c_str());
        i_info("Loading mails... ");
        librados::NObjectIterator *iter_guid = new librados::NObjectIterator(r_storage->s->find_mails(nullptr));
        i_info("Loading mail metadata...");
        rados_mails = load_rados_mail_metadata(false,r_storage,*iter_guid);
        i_info("Mails completely loaded ");
        std::map<std::string, std::list<librmb::RadosMail>>::iterator it;
        for(it=rados_mails.begin(); it!=rados_mails.end(); ++it){          
          i_info("Found mails for mailbox_guid: %s: mails : %ld",it->first.c_str(), it->second.size());
        }
      }

      ret = rbox_sync_index_rebuild((struct rbox_mailbox *)box, force, rados_mails);

      if (ret < 0) {
        i_error("error resync (%s), error(%d), force(%d)", info->vname, ret, force);
      }

      mail_index_unlock(box->index, "UNLOCKED_FOR_REPAIR");
      
      mailbox_free(&box);
    }
  }
 
  if (mailbox_list_iter_deinit(&iter) < 0) {
    ret = -1;
  }

  FUNC_END();
  return ret;
}

int rbox_sync_index_rebuild(struct rbox_mailbox *rbox, bool force, std::map<std::string, std::list<librmb::RadosMail>> &rados_mails) {
  struct index_rebuild_context *ctx;
  struct mail_index_view *view;
  struct mail_index_transaction *trans;
  struct rbox_index_header hdr;
  bool need_resize;
  int ret;
  FUNC_START();

  // get mailbox guid
  if (!force && rbox_read_header(rbox, &hdr, FALSE, &need_resize) == 0) {
    if (hdr.rebuild_count != rbox->storage->corrupted_rebuild_count && hdr.rebuild_count != 0) {
      /* already rebuilt by someone else */
      i_warning("index already rebuild by someone else %d c_rebuild_count =%d", hdr.rebuild_count,
                rbox->storage->corrupted_rebuild_count);
      rbox->storage->corrupted_rebuild_count = 0;
      FUNC_END();
      return 0;
    }
#ifdef DEBUG
    i_debug("index could not be opened");
#endif
  }
  i_info("rbox %s: Rebuilding index, guid: %s , mailbox_name: %s, alt_storage: %s", mailbox_get_path(&rbox->box),
            guid_128_to_string(rbox->mailbox_guid), rbox->box.name, rbox->box.list->set.alt_dir);

  view = mail_index_view_open(rbox->box.index);

  trans = mail_index_transaction_begin(view, MAIL_INDEX_TRANSACTION_FLAG_EXTERNAL);

  ctx = index_index_rebuild_init(&rbox->box, view, trans);

  ret = rbox_sync_index_rebuild_objects(ctx, rados_mails);

#ifdef DEBUG
  i_debug("rebuild finished");
#endif
  index_index_rebuild_deinit(&ctx, rbox_get_uidvalidity_next);

  if (ret < 0) {
    mail_index_transaction_rollback(&trans);
  } else {
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_MAIL_INDEX_HDR_FLAG_FSCKD
    mail_index_unset_fscked(trans);
#endif
    ret = mail_index_transaction_commit(&trans);
  }
  hdr.rebuild_count++;
  rbox->storage->corrupted_rebuild_count = 0;
  mail_index_view_close(&view);
  FUNC_END();
  return ret;
}
