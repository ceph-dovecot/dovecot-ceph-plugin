/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */
/*
 * Encoding implementation is copy of ceph/src/include/encoding.h
 *
 * we only took the code to encode an decode ceph_string documented in:
 * http://docs.ceph.com/docs/jewel/dev/network-encoding/
 *
 * As byte encoding LITTLE_ENDIAN is set as it is defined for
 *
 struct ceph_string {
 u32le size;
 u8    data[size];
 }

 *
 */

#ifndef ENCODING_H_
#define ENCODING_H_

#include <rados/librados.h>
#include <rados/librados.hpp>
#include <iostream>
#include <string>

typedef __u32 __bitwise__ __be32;

#define CEPH_LITTLE_ENDIAN
static __inline__ __u16 swab16(__u16 val) { return (val >> 8) | (val << 8); }
static __inline__ __u32 swab32(__u32 val) {
  return ((val >> 24) | ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000) | ((val << 24)));
}
static __inline__ uint64_t swab64(uint64_t val) {
  return ((val >> 56) | ((val >> 40) & 0xff00ull) | ((val >> 24) & 0xff0000ull) | ((val >> 8) & 0xff000000ull) |
          ((val << 8) & 0xff00000000ull) | ((val << 24) & 0xff0000000000ull) | ((val << 40) & 0xff000000000000ull) |
          ((val << 56)));
}

// mswab == maybe swab (if not LE)
#ifdef CEPH_BIG_ENDIAN
#define mswab64(a) swab64(a)
#define mswab32(a) swab32(a)
#define mswab16(a) swab16(a)
#elif defined(CEPH_LITTLE_ENDIAN)
#define mswab64(a) (a)
#define mswab32(a) (a)
#define mswab16(a) (a)

#endif

#define MAKE_LE_CLASS(bits)                               \
  struct ceph_le##bits {                                  \
    __u##bits v;                                          \
    ceph_le##bits &operator=(__u##bits nv) {              \
      v = mswab##bits(nv);                                \
      return *this;                                       \
    }                                                     \
    operator __u##bits() const { return mswab##bits(v); } \
  } __attribute__((packed));                              \
  static inline bool operator==(ceph_le##bits a, ceph_le##bits b) { return a.v == b.v; }
MAKE_LE_CLASS(64)
MAKE_LE_CLASS(32)
MAKE_LE_CLASS(16)
#undef MAKE_LE_CLASS
// --------------------------------------
// base types
template <class T>
inline void encode_raw(const T &t, ceph::bufferlist &bl) {
  bl.append((char *)&t, sizeof(t));
}
template <class T>
inline void decode_raw(T &t, ceph::bufferlist::iterator &p) {
  p.copy(sizeof(t), (char *)&t);
}

#define WRITE_RAW_ENCODER(type)                                                    \
  inline void encode(const type &v, ceph::bufferlist &bl, uint64_t features = 0) { \
    (void)features;                                                                \
    encode_raw(v, bl);                                                             \
  }                                                                                \
  inline void decode(type &v, ceph::bufferlist::iterator &p) { decode_raw(v, p); }

WRITE_RAW_ENCODER(__u8)
#ifndef _CHAR_IS_SIGNED
WRITE_RAW_ENCODER(__s8)
#endif
WRITE_RAW_ENCODER(char)
WRITE_RAW_ENCODER(ceph_le64)
WRITE_RAW_ENCODER(ceph_le32)
WRITE_RAW_ENCODER(ceph_le16)

// FIXME: we need to choose some portable floating point encoding here
WRITE_RAW_ENCODER(float)
WRITE_RAW_ENCODER(double)

inline void encode(const bool &v, ceph::bufferlist &bl) {
  __u8 vv = v;
  encode_raw(vv, bl);
  std::cout << " alled " << std::endl;
}
inline void decode(bool &v, ceph::bufferlist::iterator &p) {
  __u8 vv;
  decode_raw(vv, p);
  v = vv;
}

// -----------------------------------
// int types
#define WRITE_INTTYPE_ENCODER(type, etype)                                  \
  inline void encode(type v, ceph::bufferlist &bl, uint64_t features = 0) { \
    (void)features;                                                         \
    ceph_##etype e;                                                         \
    e = v;                                                                  \
    encode_raw(e, bl);                                                      \
  }                                                                         \
  inline void decode(type &v, ceph::bufferlist::iterator &p) {              \
    ceph_##etype e;                                                         \
    decode_raw(e, p);                                                       \
    v = e;                                                                  \
  }

WRITE_INTTYPE_ENCODER(uint64_t, le64)
WRITE_INTTYPE_ENCODER(int64_t, le64)
WRITE_INTTYPE_ENCODER(uint32_t, le32)
WRITE_INTTYPE_ENCODER(int32_t, le32)
WRITE_INTTYPE_ENCODER(uint16_t, le16)
WRITE_INTTYPE_ENCODER(int16_t, le16)

#ifdef ENCODE_DUMP
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define ENCODE_STR(x) #x
#define ENCODE_STRINGIFY(x) ENCODE_STR(x)
#define ENCODE_DUMP_PRE() unsigned pre_off = bl.length()
// NOTE: This is almost an exponential backoff, but because we count
// bits we get a better sample of things we encode later on.
#define ENCODE_DUMP_POST(cl)                                                                  \
  do {                                                                                        \
    static int i = 0;                                                                         \
    i++;                                                                                      \
    int bits = 0;                                                                             \
    for (unsigned t = i; t; bits++)                                                           \
      t &= t - 1;                                                                             \
    if (bits > 2)                                                                             \
      break;                                                                                  \
    char fn[200];                                                                             \
    snprintf(fn, sizeof(fn), ENCODE_STRINGIFY(ENCODE_DUMP) "/%s__%d.%x", #cl, getpid(), i++); \
    int fd = ::open(fn, O_WRONLY | O_TRUNC | O_CREAT, 0644);                                  \
    if (fd >= 0) {                                                                            \
      bufferlist sub;                                                                         \
      sub.substr_of(bl, pre_off, bl.length() - pre_off);                                      \
      sub.write_fd(fd);                                                                       \
      ::close(fd);                                                                            \
    }                                                                                         \
  } while (0)
#else
#define ENCODE_DUMP_PRE()
#define ENCODE_DUMP_POST(cl)
#endif

#define WRITE_CLASS_ENCODER(cl)                                                  \
  inline void encode(const cl &c, ceph::bufferlist &bl, uint64_t features = 0) { \
    ENCODE_DUMP_PRE();                                                           \
    c.encode(bl);                                                                \
    ENCODE_DUMP_POST(cl);                                                        \
  }                                                                              \
  inline void decode(cl &c, bufferlist::iterator &p) { c.decode(p); }

#define WRITE_CLASS_MEMBER_ENCODER(cl)                          \
  inline void encode(const cl &c, ceph::bufferlist &bl) const { \
    ENCODE_DUMP_PRE();                                          \
    c.encode(bl);                                               \
    ENCODE_DUMP_POST(cl);                                       \
  }                                                             \
  inline void decode(cl &c, bufferlist::iterator &p) { c.decode(p); }

#define WRITE_CLASS_ENCODER_FEATURES(cl)                                     \
  inline void encode(const cl &c, ceph::bufferlist &bl, uint64_t features) { \
    ENCODE_DUMP_PRE();                                                       \
    c.encode(bl, features);                                                  \
    ENCODE_DUMP_POST(cl);                                                    \
  }                                                                          \
  inline void decode(cl &c, bufferlist::iterator &p) { c.decode(p); }
#define WRITE_CLASS_ENCODER_OPTIONAL_FEATURES(cl)                                \
  inline void encode(const cl &c, ceph::bufferlist &bl, uint64_t features = 0) { \
    ENCODE_DUMP_PRE();                                                           \
    c.encode(bl, features);                                                      \
    ENCODE_DUMP_POST(cl);                                                        \
  }                                                                              \
  inline void decode(cl &c, bufferlist::iterator &p) { c.decode(p); }

// string
inline void encode(const std::string &s, ceph::bufferlist &bl, uint64_t features = 0) {
  (void)features;
  __u32 len = s.length();
  encode(len, bl);
  if (len)
    bl.append(s.data(), len);
}
// const char* (encode only, string compatible)
inline void encode(const char *s, ceph::bufferlist &bl) {
  __u32 len = strlen(s);
  encode(len, bl);
  if (len)
    bl.append(s, len);
}

#define PLAIN_FILTER_NAME "plain"

#endif /* ENCODING_H_ */
