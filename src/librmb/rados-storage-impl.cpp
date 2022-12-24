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

#include "rados-storage-impl.h"

#include <algorithm>
#include <list>
#include <set>
#include <string>
#include <utility>
#include <thread>
#include <mutex>

#include "rados-util.h"

#include <rados/librados.hpp>

#include "encoding.h"
#include "limits.h"
#include "rados-metadata-storage-impl.h"

using std::pair;
using std::string;

using ceph::bufferlist;

using librmb::RadosStorageImpl;

#define DICT_USERNAME_SEPARATOR '/'
const char *RadosStorageImpl::CFG_OSD_MAX_WRITE_SIZE = "osd_max_write_size";
const char *RadosStorageImpl::CFG_OSD_MAX_OBJECT_SIZE= "osd_max_object_size";

RadosStorageImpl::RadosStorageImpl(RadosCluster *_cluster) {
  cluster = _cluster;
  max_write_size = 10;
  max_object_size = 134217728; //ceph default 128MB
  io_ctx_created = false;
  wait_method = WAIT_FOR_COMPLETE_AND_CB;
}

RadosStorageImpl::~RadosStorageImpl() {}

//DEPRECATED!!!!! -> moved to rbox-save.cpp
int RadosStorageImpl::split_buffer_and_exec_op(RadosMail *current_object,
                                               librados::ObjectWriteOperation *write_op_xattr,
                                               const uint64_t &max_write) {
  
  if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }

  int ret_val = 0;
  uint64_t write_buffer_size = current_object->get_mail_size();

  assert(max_write > 0);

  if (write_buffer_size == 0 || 
      max_write <= 0) {      
    ret_val = -1;
    return ret_val;
  }

  ret_val = get_io_ctx().operate(*current_object->get_oid(), write_op_xattr);

  if(ret_val< 0){
    ret_val = -1;
    return ret_val;
  }

  uint64_t rest = write_buffer_size % max_write;
  int div = write_buffer_size / max_write + (rest > 0 ? 1 : 0);
  for (int i = 0; i < div; ++i) {

    // split the buffer.
    librados::bufferlist tmp_buffer;
    
    librados::ObjectWriteOperation write_op;

    int offset = i * max_write;

    uint64_t length = max_write;
    if (write_buffer_size < ((i + 1) * length)) {
      length = rest;
    }
#ifdef HAVE_ALLOC_HINT_2
    write_op.set_alloc_hint2(write_buffer_size, length, librados::ALLOC_HINT_FLAG_COMPRESSIBLE);
#else
    write_op.set_alloc_hint(write_buffer_size, length);
#endif
    if (div == 1) {
      write_op.write(0, *current_object->get_mail_buffer());
    } else {
      tmp_buffer.clear();
      tmp_buffer.substr_of(*current_object->get_mail_buffer(), offset, length);
      write_op.write(offset, tmp_buffer);
    }
    
    ret_val = get_io_ctx().operate(*current_object->get_oid(), &write_op);
    if(ret_val < 0){
      ret_val = -1;
      break;
    }
  }
  // deprecated unused
  current_object->set_write_operation(nullptr);
  current_object->set_completion(nullptr);
  current_object->set_active_op(0);
    
  // free mail's buffer cause we don't need it anymore
  librados::bufferlist *mail_buffer = current_object->get_mail_buffer();
  delete mail_buffer;

  return ret_val;
}

int RadosStorageImpl::save_mail(const std::string &oid, librados::bufferlist &buffer) {
  return get_io_ctx().write_full(oid, buffer);
}

int RadosStorageImpl::read_mail(const std::string &oid, librados::bufferlist *buffer) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }
  size_t max = INT_MAX;
  return get_io_ctx().read(oid, *buffer, max, 0);
}

int RadosStorageImpl::delete_mail(RadosMail *mail) {
  int ret = -1;

  if (cluster->is_connected() && io_ctx_created && mail != nullptr) {
    ret = delete_mail(*mail->get_oid());
  }
  return ret;
}
int RadosStorageImpl::delete_mail(const std::string &oid) {
  if (!cluster->is_connected() || oid.empty() || !io_ctx_created) {
    return -1;
  }
  return get_io_ctx().remove(oid);
}

bool RadosStorageImpl::execute_operation(std::string &oid, librados::ObjectWriteOperation *write_op_xattr) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return false;
  }
  return get_io_ctx().operate(oid, write_op_xattr) >=0 ? true : false;
}

bool RadosStorageImpl::append_to_object(std::string &oid, librados::bufferlist &bufferlist, int length) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return false;
  }
  return get_io_ctx().append(oid, bufferlist, length) >=0 ? true : false;
}
int RadosStorageImpl::read_operate(const std::string &oid, librados::ObjectReadOperation *read_operation, librados::bufferlist *bufferlist) {
if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }
  return get_io_ctx().operate(oid, read_operation, bufferlist);
}

int RadosStorageImpl::aio_operate(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                                  librados::ObjectWriteOperation *op) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }

  if (io_ctx_ != nullptr) {
    return io_ctx_->aio_operate(oid, c, op);
  } else {
    return get_io_ctx().aio_operate(oid, c, op);
  }
}

int RadosStorageImpl::stat_mail(const std::string &oid, uint64_t *psize, time_t *pmtime) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }
  return get_io_ctx().stat(oid, psize, pmtime);
}

void RadosStorageImpl::set_namespace(const std::string &_nspace) {
  get_io_ctx().set_namespace(_nspace);
  this->nspace = _nspace;
}

librados::NObjectIterator RadosStorageImpl::find_mails(const RadosMetadata *attr) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return librados::NObjectIterator::__EndObjectIterator;
  }

  if (attr != nullptr) {
    std::string filter_name = PLAIN_FILTER_NAME;
    ceph::bufferlist filter_bl;

    encode(filter_name, filter_bl);
    encode("_" + attr->key, filter_bl);
    encode(attr->bl.to_str(), filter_bl);

    return get_io_ctx().nobjects_begin(filter_bl);
  } else {
    return get_io_ctx().nobjects_begin();
  }
}
/**
 * POC Implementation: 
 * 
 * see in prod how it behaves. 
 * 
 **/
std::set<std::string> RadosStorageImpl::find_mails_async(const RadosMetadata *attr, 
                                                         std::string &pool_name,
                                                         int num_threads,
                                                         void (*ptr)(std::string&)){

    std::set<std::string> oid_list;
    std::mutex oid_list_mutex;
    
    // Thread!!
    auto f = [](const std::vector<std::string> &list, 
                std::mutex &oid_mutex, 
                std::set<std::string> &oids, 
                librados::IoCtx *io_ctx,
                void (*ptr)(std::string&),
                std::string osd,
                ceph::bufferlist &filter) {

        int total_count = 0;

        for (auto const &pg: list) {

          uint64_t ppool;
          uint32_t pseed;
          int r = sscanf(pg.c_str(), "%llu.%x", (long long unsigned *)&ppool, &pseed);
          
          librados::NObjectIterator iter= io_ctx->nobjects_begin(pseed);
          int count = 0;
          while (iter != librados::NObjectIterator::__EndObjectIterator) {
            std::string oid = iter->get_oid();
            {
              std::lock_guard<std::mutex> guard(oid_mutex);          
              oids.insert(oid);  
              count++;
            }          
            iter++;
          }       
          total_count+=count;
          std::string t = "osd "+ osd +" pg done " + pg + " objects " + std::to_string(count);
          (*ptr)(t);    
        } 
        std::string t = "done with osd "+ osd + " total: " + std::to_string(total_count);
        (*ptr)(t);             
    }; 


    //std::string pool_mame = "mail_storage";
    std::map<std::string, std::vector<std::string>> osd_pg_map = cluster->list_pgs_osd_for_pool(pool_name);
    std::vector<std::thread> threads;

    std::string filter_name = PLAIN_FILTER_NAME;
    ceph::bufferlist filter_bl;

    encode(filter_name, filter_bl);
    encode("_" + attr->key, filter_bl);
    encode(attr->bl.to_str(), filter_bl);
    std::string msg_1 = "buffer set";
    (*ptr)(msg_1); 

    for (const auto& x : osd_pg_map){
      if(threads.size() == num_threads){        
        threads[0].join();
        threads.erase(threads.begin());            
      }
      //TODO: update parser that this will not end here filter out invalid osd
      if(x.first == "oon") {
        std::string create_msg = "skipping thread for osd: "+ x.first;
        (*ptr)(create_msg);         
        continue;
      }
      threads.push_back(std::thread(f, std::ref(x.second),std::ref(oid_list_mutex),std::ref(oid_list), &get_io_ctx(), ptr, x.first, std::ref(filter_bl)));
      std::string create_msg = "creating thread for osd: "+ x.first;
      (*ptr)(create_msg);       
    }


    for (auto const &thread: threads) {      
        thread.join();
    }   
    return oid_list;
}
librados::IoCtx &RadosStorageImpl::get_io_ctx() { return io_ctx; }
librados::IoCtx &RadosStorageImpl::get_recovery_io_ctx() { return recovery_io_ctx; }

int RadosStorageImpl::open_connection(const std::string &poolname, const std::string &index_pool,
                                      const std::string &clustername,
                                      const std::string &rados_username) {
  if (cluster->is_connected() && io_ctx_created) {
    // cluster is already connected!
    return 1;
  }

  if (cluster->init(clustername, rados_username) < 0) {
    return -1;
  }
  return create_connection(poolname, index_pool);
}
int RadosStorageImpl::open_connection(const std::string &poolname,
                                      const std::string &clustername,
                                      const std::string &rados_username) {
  if (cluster->is_connected() && io_ctx_created) {
    // cluster is already connected!
    return 1;
  }

  if (cluster->init(clustername, rados_username) < 0) {
    return -1;
  }
  return create_connection(poolname, poolname);
}
int RadosStorageImpl::open_connection(const string &poolname, const string &index_pool) {
  if (cluster->init() < 0) {
    return -1;
  }
  return create_connection(poolname, index_pool);
}

int RadosStorageImpl::open_connection(const string &poolname) {
  if (cluster->init() < 0) {
    return -1;
  }
  return create_connection(poolname, poolname);
}

int RadosStorageImpl::create_connection(const std::string &poolname, const std::string &index_pool){
  // pool exists? else create
  int err = cluster->io_ctx_create(poolname, &io_ctx);
  if (err < 0) {
    return err;
  }

  err = cluster->recovery_index_io_ctx(index_pool, &recovery_io_ctx);
  if (err < 0) {
    return err;
  }
  string max_write_size_str;
  err = cluster->get_config_option(RadosStorageImpl::CFG_OSD_MAX_WRITE_SIZE, &max_write_size_str);
  if (err < 0) {
    return err;
  }
  max_write_size = std::stoi(max_write_size_str);
 
  string max_object_size_str;
  err = cluster->get_config_option(RadosStorageImpl::CFG_OSD_MAX_OBJECT_SIZE, &max_object_size_str);
  if (err < 0) {
    return err;
  }
  max_object_size = std::stoi(max_object_size_str);
  
  if (err == 0) {
    io_ctx_created = true;
  }
  
  // set the poolname
  pool_name = poolname;
  return 0;
}

void RadosStorageImpl::close_connection() {
  if (cluster != nullptr && io_ctx_created) {
    cluster->deinit();
  }
}
bool RadosStorageImpl::wait_for_write_operations_complete(librados::AioCompletion *completion,
                                                          librados::ObjectWriteOperation *write_operation) {
  if (completion == nullptr) {
    return true;  // failed!
  }

  bool failed = false;

  switch (wait_method) {
    case WAIT_FOR_COMPLETE_AND_CB:
      completion->wait_for_complete_and_cb();
      break;
    case WAIT_FOR_SAFE_AND_CB:
      completion->wait_for_safe_and_cb();
      break;
    default:
      completion->wait_for_complete_and_cb();
      break;
  }
  failed = completion->get_return_value() < 0 || failed ? true : false;
  // clean up
  completion->release();

  return failed;
}
//DEPRECATED and buggy
bool RadosStorageImpl::wait_for_rados_operations(const std::list<librmb::RadosMail *> &object_list) {
  bool ctx_failed = false;
  if(object_list.size() == 0){
    return ctx_failed;
  }
  // wait for all writes to finish!
  // imaptest shows it's possible that begin -> continue -> finish cycle is invoked several times before
  // rbox_transaction_save_commit_pre is called.
  for (std::list<librmb::RadosMail *>::const_iterator it_cur_obj = object_list.begin(); it_cur_obj != object_list.end();
       ++it_cur_obj) {
    // if we come from copy mail, there is no operation to wait for.
    if ((*it_cur_obj)->has_active_op()) {
      bool op_failed =
          wait_for_write_operations_complete((*it_cur_obj)->get_completion(), (*it_cur_obj)->get_write_operation());

      ctx_failed = ctx_failed ? ctx_failed : op_failed;
      (*it_cur_obj)->set_active_op(0);
      // (*it_cur_obj)->set_completion(nullptr);
      (*it_cur_obj)->set_write_operation(nullptr);
    }
    // free mail's buffer cause we don't need it anymore
    librados::bufferlist *mail_buffer = (*it_cur_obj)->get_mail_buffer();
    delete mail_buffer;
  }
  return ctx_failed;
}

// assumes that destination io ctx is current io_ctx;
int RadosStorageImpl::move(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                           std::list<RadosMetadata> &to_update, bool delete_source) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }

  int ret = 0;
  librados::ObjectWriteOperation write_op;
  librados::IoCtx src_io_ctx, dest_io_ctx;

  librados::AioCompletion *completion = librados::Rados::aio_create_completion();

  // destination io_ctx is current io_ctx
  dest_io_ctx = io_ctx;

  if (strcmp(src_ns, dest_ns) != 0) {
    src_io_ctx.dup(dest_io_ctx);
    src_io_ctx.set_namespace(src_ns);
    dest_io_ctx.set_namespace(dest_ns);

#if LIBRADOS_VERSION_CODE >= 30000
    write_op.copy_from(src_oid, src_io_ctx, 0, 0);
#else
    write_op.copy_from(src_oid, src_io_ctx, 0);
#endif
  } else {
    src_io_ctx = dest_io_ctx;
    time_t t;
    uint64_t size;
    ret = src_io_ctx.stat(src_oid, &size, &t);
    if (ret < 0) {
      return ret;
    }
  }

  // because we create a copy, save date needs to be updated
  // as an alternative we could use &ctx->data.save_date here if we save it to xattribute in write_metadata
  // and restore it in read_metadata function. => save_date of copy/move will be same as source.
  // write_op.mtime(&ctx->data.save_date);
  time_t save_time = time(NULL);
  write_op.mtime(&save_time);

  // update metadata
  for (std::list<RadosMetadata>::iterator it = to_update.begin(); it != to_update.end(); ++it) {
    write_op.setxattr((*it).key.c_str(), (*it).bl);
  }
  ret = aio_operate(&dest_io_ctx, dest_oid, completion, &write_op);
  if (ret >= 0) {
    completion->wait_for_complete();
    ret = completion->get_return_value();
    if (delete_source && strcmp(src_ns, dest_ns) != 0 && ret == 0) {
      ret = src_io_ctx.remove(src_oid);
    }
  }
  completion->release();
  // reset io_ctx
  dest_io_ctx.set_namespace(dest_ns);
  return ret;
}

// assumes that destination io ctx is current io_ctx;
int RadosStorageImpl::copy(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                           std::list<RadosMetadata> &to_update) {
  if (!cluster->is_connected() || !io_ctx_created) {
    return -1;
  }

  librados::ObjectWriteOperation write_op;
  librados::IoCtx src_io_ctx, dest_io_ctx;

  // destination io_ctx is current io_ctx
  dest_io_ctx = io_ctx;

  if (strcmp(src_ns, dest_ns) != 0) {
    src_io_ctx.dup(dest_io_ctx);
    src_io_ctx.set_namespace(src_ns);
    dest_io_ctx.set_namespace(dest_ns);
  } else {
    src_io_ctx = dest_io_ctx;
  }

#if LIBRADOS_VERSION_CODE >= 30000
  write_op.copy_from(src_oid, src_io_ctx, 0, 0);
#else
  write_op.copy_from(src_oid, src_io_ctx, 0);
#endif

  // because we create a copy, save date needs to be updated
  // as an alternative we could use &ctx->data.save_date here if we save it to xattribute in write_metadata
  // and restore it in read_metadata function. => save_date of copy/move will be same as source.
  // write_op.mtime(&ctx->data.save_date);
  time_t save_time = time(NULL);
  write_op.mtime(&save_time);

  // update metadata
  for (std::list<RadosMetadata>::iterator it = to_update.begin(); it != to_update.end(); ++it) {
    write_op.setxattr((*it).key.c_str(), (*it).bl);
  }
  int ret = 0;
  librados::AioCompletion *completion = librados::Rados::aio_create_completion();
  ret = aio_operate(&dest_io_ctx, dest_oid, completion, &write_op);
  if (ret >= 0) {
    ret = completion->wait_for_complete();
    // cppcheck-suppress redundantAssignment
    ret = completion->get_return_value();
  }
  completion->release();
  // reset io_ctx
  dest_io_ctx.set_namespace(dest_ns);
  return ret;
}

// DEPRECATED => MOVED to rbox-save.cpp
// if save_async = true, don't forget to call wait_for_rados_operations e.g. wait_for_write_operations_complete
// to wait for completion and free resources.
bool RadosStorageImpl::save_mail(librados::ObjectWriteOperation *write_op_xattr, 
                                 RadosMail *mail) {
                                   
  if (!cluster->is_connected() || !io_ctx_created) {
    return false;
  }
  if (write_op_xattr == nullptr || mail == nullptr) {
    return false;
  }
  time_t save_date = mail->get_rados_save_date();
  write_op_xattr->mtime(&save_date);  
  uint32_t max_op_size = get_max_write_size_bytes() - 1024;
  //TODO: make this configurable
  int ret = split_buffer_and_exec_op(mail, write_op_xattr, 10240);
  if (ret != 0) {
    mail->set_active_op(0);
  } 
  return ret == 0;
}
// if save_async = true, don't forget to call wait_for_rados_operations e.g. wait_for_write_operations_complete
// to wait for completion and free resources.
// bool RadosStorageImpl::save_mail(RadosMail *mail) {
//   if (!cluster->is_connected() || !io_ctx_created) {
//     return false;
//   }
//   // delete write_op_xattr is called after operation completes (wait_for_rados_operations)
//   librados::ObjectWriteOperation write_op_xattr;  // = new librados::ObjectWriteOperation();

//   // set metadata
//   for (std::map<std::string, librados::bufferlist>::iterator it = mail->get_metadata()->begin();
//        it != mail->get_metadata()->end(); ++it) {
//     write_op_xattr.setxattr(it->first.c_str(), it->second);
//   }

//   if (mail->get_extended_metadata()->size() > 0) {
//     write_op_xattr.omap_set(*mail->get_extended_metadata());
//   }
//   return save_mail(&write_op_xattr, mail);
// }


/***SARA:this method is invoked by rbox_save_finish from rbox_save from Plugin part
 * 1-compare email size with Max allowed object size
 * 2-consider whether the email can be write in one chunk or it must be splited
 * 3-save metadat***/

bool  RadosStorageImpl::save_mail(RadosMail *mail){
  bool ret_val=false;
  /*1-compare email size with Max allowed object size*/
   int object_size = mail->get_mail_size();
   int max_object_size = this->get_max_object_size();
   if( max_object_size < object_size ||object_size<0||max_object_size==0){
    return false;
   }
  /*2-cosider whether the email can be write in one chunk or it must be splited*/
  int max_write=get_max_write_size_bytes();
  int buff_size=mail->get_mail_buffer()->length();
  if(buff_size<0||max_write==0){
    return false;
  }

  int rest = buff_size % max_write;
  int div =  buff_size / max_write + (rest > 0 ? 1 : 0);

  for (int i = 0; i <div; i++) {

    librados::bufferlist tmp_buffer;
    
    int offset = i * max_write;
    int length = max_write;
    
    if (buff_size < ((i+1) * length)) {
      length = rest;
    }
    if(i==0){
      /*start step3:save metadata*/
      librados::ObjectWriteOperation write_op;
      librmb::RadosMetadataStorageImpl* ms=new librmb::RadosMetadataStorageImpl();   
      ms->get_storage()->save_metadata(&write_op,mail);
      time_t save_date = mail->get_rados_save_date();
      write_op.mtime(&save_date);  
      /*End step3;*/

      /*start create first chunk*/
      tmp_buffer.substr_of(mail->get_mail_buffer(),offset,length);
      write_op.write(0, mail->get_mail_buffer());
      /*End*/

      /*Save metadata and first chunk together in a common write_op
      If (div==1) so after this step quick this loop. */
      ret_val =execute_operation(*mail->get_oid(), &write_op);
    }
    else {      
      if(offset + length > buff_size){
        return false;
      }else{
        tmp_buffer.substr_of(mail->get_mail_buffer(),offset,length);
        ret_val =append_to_object(*mail->get_oid(),tmp_buffer,length);
      }
    }      
  }
  return ret_val;
}

librmb::RadosMail *RadosStorageImpl::alloc_rados_mail() { return new librmb::RadosMail(); }

void RadosStorageImpl::free_rados_mail(librmb::RadosMail *mail) {
  if (mail != nullptr) {
    delete mail;
    mail = nullptr;
  }
}

uint64_t RadosStorageImpl::ceph_index_size(){
  uint64_t psize;
  time_t pmtime;
  get_recovery_io_ctx().stat(get_namespace(), &psize, &pmtime);
  return psize;
}

int RadosStorageImpl::ceph_index_append(const std::string &oid) {  
  librados::bufferlist bl;
  bl.append(RadosUtils::convert_to_ceph_index(oid));
  return get_recovery_io_ctx().append( get_namespace(),bl, bl.length());
}

int RadosStorageImpl::ceph_index_append(const std::set<std::string> &oids) {
  librados::bufferlist bl;
  bl.append(RadosUtils::convert_to_ceph_index(oids));
  return get_recovery_io_ctx().append( get_namespace(),bl, bl.length());
}
int RadosStorageImpl::ceph_index_overwrite(const std::set<std::string> &oids) {
  librados::bufferlist bl;
  bl.append(RadosUtils::convert_to_ceph_index(oids));
  return get_recovery_io_ctx().write_full( get_namespace(),bl);
}
std::set<std::string> RadosStorageImpl::ceph_index_read() {
  std::set<std::string> index;
  librados::bufferlist bl;
  size_t max = INT_MAX;
  int64_t psize;
  time_t pmtime;
  get_recovery_io_ctx().stat(get_namespace(), &psize, &pmtime);
  if(psize <=0){
    return index;
  }
  //std::cout << " NAMESPACE: " << get_namespace() << " exist? " << exist << " size : " << psize << std::endl;
  int ret = get_recovery_io_ctx().read(get_namespace(),bl, max,0);


  if(ret < 0){
    return index;
  }
  index = RadosUtils::ceph_index_to_set(bl.c_str());
  return index;
}
int RadosStorageImpl::ceph_index_delete() {
  return get_recovery_io_ctx().remove(get_namespace());
}

