//
// Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOCI_TYPE_HOLDER_H_INCLUDED
#define SOCI_TYPE_HOLDER_H_INCLUDED

#include "soci/fixed-size-ints.h"
#include "soci/soci-backend.h"
// std
#include <ctime>
#include <limits>
#include <type_traits>
#include <typeinfo>

namespace soci {
class type_holder_bad_cast : public std::bad_cast {
public:
  type_holder_bad_cast(soci::db_type dt, const std::type_info &target) {
    info = std::bad_cast::what();

    switch (dt) {
    case soci::db_string:
      info.append(": no cast defined from std::string to ");
      break;
    case soci::db_date:
      info.append(": no cast defined from std::tm to ");
      break;
    case soci::db_double:
      info.append(": no cast defined from double to ");
      break;
    case soci::db_int8:
      info.append(": no cast defined from int8 to ");
      break;
    case soci::db_int16:
      info.append(": no cast defined from int16 to ");
      break;
    case soci::db_int32:
      info.append(": no cast defined from int32 to ");
      break;
    case soci::db_int64:
      info.append(": no cast defined from int64 to ");
      break;
    case soci::db_uint8:
      info.append(": no cast defined from uint8 to ");
      break;
    case soci::db_uint16:
      info.append(": no cast defined from uint16 to ");
      break;
    case soci::db_uint32:
      info.append(": no cast defined from uint32 to ");
      break;
    case soci::db_uint64:
      info.append(": no cast defined from uint64 to ");
      break;
    case soci::db_blob:
      info.append(": no cast defined from std::string(blob) to ");
      break;
    case soci::db_xml:
      info.append(": no cast defined from std::string(xml) to ");
      break;
    default:
      info.append(": no cast defined from unknown db_type(")
          .append(std::to_string((int)dt))
          .append(") to ");
      break;
    }

    info.append(target.name());
  }

  type_holder_bad_cast(const std::type_info &source,
                       const std::type_info &target) {
    info = std::bad_cast::what();
    info.append(": no cast defined from ")
        .append(source.name())
        .append(" to ")
        .append(target.name());
  }

  type_holder_bad_cast(const std::string &message) {
    info = std::bad_cast::what();
    info.append(": ").append(message);
  }

  const char *what() const noexcept override { return info.c_str(); }

private:
  std::string info;
};

// Type safe conversion that fails at compilation if instantiated
template <typename T, typename U, typename Enable = void>
struct soci_cast {
  static inline T cast(U) { throw type_holder_bad_cast(typeid(T), typeid(U)); }
};

// Type safe conversion that is a noop
template <typename T, typename U>
struct soci_cast<T, U,
                 typename std::enable_if<std::is_same<T, U>::value>::type> {
  static inline T cast(U val) { return val; }
};

// Type safe conversion that is widening
template <typename T, typename U>
struct soci_cast<
    T, U,
    typename std::enable_if<(
        (!std::is_same<T, U>::value) && std::is_integral<T>::value &&
        std::is_integral<U>::value &&
        (uintmax_t)(std::numeric_limits<T>::max)() >=
            (uintmax_t)(std::numeric_limits<U>::max)() &&
        (intmax_t)(std::numeric_limits<T>::min)() <=
            (intmax_t)(std::numeric_limits<U>::min)())>::type> {
  static inline T cast(U val) { return static_cast<T>(val); }
};

// Type safe integral conversion that can narrow the min side
template <typename T, typename U>
struct soci_cast<
    T, U,
    typename std::enable_if<(
        std::is_integral<T>::value && std::is_integral<U>::value &&
        (uintmax_t)(std::numeric_limits<T>::max)() >=
            (uintmax_t)(std::numeric_limits<U>::max)() &&
        (intmax_t)(std::numeric_limits<T>::min)() >
            (intmax_t)(std::numeric_limits<U>::min)())>::type> {
  static inline T cast(U val) {
    if ((intmax_t)(std::numeric_limits<T>::min)() > (intmax_t)val)
      throw type_holder_bad_cast("narrowing cast on the min side");
    return static_cast<T>(val);
  }
};

// Type safe integral conversion that can narrow the max side
template <typename T, typename U>
struct soci_cast<
    T, U,
    typename std::enable_if<(
        std::is_integral<T>::value && std::is_integral<U>::value &&
        (uintmax_t)(std::numeric_limits<T>::max)() <
            (uintmax_t)(std::numeric_limits<U>::max)() &&
        (intmax_t)(std::numeric_limits<T>::min)() <=
            (intmax_t)(std::numeric_limits<U>::min)())>::type> {
  static inline T cast(U val) {
    if ((uintmax_t)(std::numeric_limits<T>::max)() < (uintmax_t)val)
      throw type_holder_bad_cast("narrowing cast on the max side");
    return static_cast<T>(val);
  }
};

// Type safe integral conversion that can narrow on both sides
template <typename T, typename U>
struct soci_cast<
    T, U,
    typename std::enable_if<(
        std::is_integral<T>::value && std::is_integral<U>::value &&
        (uintmax_t)(std::numeric_limits<T>::max)() <
            (uintmax_t)(std::numeric_limits<U>::max)() &&
        (intmax_t)(std::numeric_limits<T>::min)() >
            (intmax_t)(std::numeric_limits<U>::min)())>::type> {
  static inline T cast(U val) {
    T ret = static_cast<T>(val);
    if (static_cast<U>(ret) != val)
      throw type_holder_bad_cast("narrowing cast on the min or max side");
    return ret;
  }
};

// Type safe conversion involving at least one floating-point value
template <typename T, typename U>
struct soci_cast<
    T, U,
    typename std::enable_if<!std::is_same<T, U>::value &&
                            (std::is_floating_point<T>::value ||
                             std::is_floating_point<U>::value)>::type> {
  static inline T cast(U val) {
    T ret = static_cast<T>(val);
    if (static_cast<U>(ret) != val)
      throw type_holder_bad_cast("cast lost floating precision");
    return ret;
  }
};

namespace details {
// Returns U* as T*, if the dynamic type of the pointer is really T.
//
// This should be used instead of dynamic_cast<> because using it doesn't work
// when using libc++ and ELF visibility together. Luckily, when we don't need
// the full power of the cast, but only need to check if the types are the
// same, it can be done by comparing their type info objects.
//
// This function does _not_ replace dynamic_cast<> in all cases and notably
// doesn't allow the input pointer to be null.
template <typename T, typename U> T *checked_ptr_cast(U *ptr) {
  // Check if they're identical first, as an optimization, and then compare
  // their names to make it actually work with libc++.
  std::type_info const &ti_ptr = typeid(*ptr);
  std::type_info const &ti_ret = typeid(T);

  if (&ti_ptr != &ti_ret && std::strcmp(ti_ptr.name(), ti_ret.name()) != 0) {
    return NULL;
  }

  return static_cast<T *>(ptr);
}

// Base class holder + derived class type_holder for storing type data
// instances in a container of holder objects
union SOCI_DECL type_holder {
  type_holder(){};
  ~type_holder() {}
  std::string s;
  int8_t i8;
  int16_t i16;
  int32_t i32;
  int64_t i64;
  uint8_t ui8;
  uint16_t ui16;
  uint32_t ui32;
  uint64_t ui64;
  double d;
  std::tm t = {};
};

class SOCI_DECL holder {
public:
  template <typename T> static holder *make_holder(T *&val);
  ~holder();

  template <typename T> constexpr T get() const {
    switch (dt) {
    case soci::db_string:
      return soci_cast<T, std::string>::cast(val.s);
    case soci::db_date:
      return soci_cast<T, std::tm>::cast(val.t);
    case soci::db_double:
      return soci_cast<T, double>::cast(val.d);
    case soci::db_int8:
      return soci_cast<T, int8_t>::cast(val.i8);
    case soci::db_int16:
      return soci_cast<T, int16_t>::cast(val.i16);
    case soci::db_int32:
      return soci_cast<T, int32_t>::cast(val.i32);
    case soci::db_int64:
      return soci_cast<T, int64_t>::cast(val.i64);
    case soci::db_uint8:
      return soci_cast<T, uint8_t>::cast(val.ui8);
    case soci::db_uint16:
      return soci_cast<T, uint16_t>::cast(val.ui16);
    case soci::db_uint32:
      return soci_cast<T, uint32_t>::cast(val.ui32);
    case soci::db_uint64:
      return soci_cast<T, uint64_t>::cast(val.ui64);
    case soci::db_blob:
      return soci_cast<T, std::string>::cast(val.s);
    case soci::db_xml:
      return soci_cast<T, std::string>::cast(val.s);
    default:
      throw type_holder_bad_cast(dt, typeid(T));
    }
  }

private:
  holder(soci::db_type dt_);
  type_holder val;
  const soci::db_type dt;
};

template <typename T> struct type_holder_raw_get {
  static_assert(std::is_same<T, void>::value, "Unmatched raw type");
  static const db_type type = (soci::db_type)0;
  static inline T *get(type_holder &) {
    throw std::bad_cast(); // Unreachable: only provided for compilation
                           // corectness
  }
};

template <> struct type_holder_raw_get<std::string> {
  static const db_type type = soci::db_string;
  static inline std::string *get(type_holder &val) { return &val.s; }
};

template <> struct type_holder_raw_get<int8_t> {
  static const db_type type = soci::db_int8;
  static inline int8_t *get(type_holder &val) { return &val.i8; }
};

template <> struct type_holder_raw_get<int16_t> {
  static const db_type type = soci::db_int16;
  static inline int16_t *get(type_holder &val) { return &val.i16; }
};

template <> struct type_holder_raw_get<int32_t> {
  static const db_type type = soci::db_int32;
  static inline int32_t *get(type_holder &val) { return &val.i32; }
};

template <> struct type_holder_raw_get<int64_t> {
  static const db_type type = soci::db_int64;
  static inline int64_t *get(type_holder &val) { return &val.i64; }
};

template <> struct type_holder_raw_get<uint8_t> {
  static const db_type type = soci::db_uint8;
  static inline uint8_t *get(type_holder &val) { return &val.ui8; }
};

template <> struct type_holder_raw_get<uint16_t> {
  static const db_type type = soci::db_uint16;
  static inline uint16_t *get(type_holder &val) { return &val.ui16; }
};

template <> struct type_holder_raw_get<uint32_t> {
  static const db_type type = soci::db_uint32;
  static inline uint32_t *get(type_holder &val) { return &val.ui32; }
};

template <> struct type_holder_raw_get<uint64_t> {
  static const db_type type = soci::db_uint64;
  static inline uint64_t *get(type_holder &val) { return &val.ui64; }
};

#if defined(SOCI_INT64_IS_LONG)
template <> struct type_holder_raw_get<long long> {
  static const db_type type = soci::db_int64;
  static inline int64_t *get(type_holder &val) { return &val.i64; }
};

template <> struct type_holder_raw_get<unsigned long long> {
  static const db_type type = soci::db_uint64;
  static inline uint64_t *get(type_holder &val) { return &val.ui64; }
};
#elif defined(SOCI_LONG_IS_64_BIT)
template <> struct type_holder_raw_get<long> {
  static const db_type type = soci::db_int64;
  static inline int64_t *get(type_holder &val) { return &val.i64; }
};

template <> struct type_holder_raw_get<unsigned long> {
  static const db_type type = soci::db_uint64;
  static inline uint64_t *get(type_holder &val) { return &val.ui64; }
};
#else
template <> struct type_holder_raw_get<long> {
  static const db_type type = soci::db_int32;
  static inline int32_t *get(type_holder &val) { return &val.i32; }
};

template <> struct type_holder_raw_get<unsigned long> {
  static const db_type type = soci::db_uint32;
  static inline uint32_t *get(type_holder &val) { return &val.ui32; }
};
#endif

template <> struct type_holder_raw_get<double> {
  static const db_type type = soci::db_double;
  static inline double *get(type_holder &val) { return &val.d; }
};

template <> struct type_holder_raw_get<std::tm> {
  static const db_type type = soci::db_date;
  static inline std::tm *get(type_holder &val) { return &val.t; }
};

template <typename T> holder *holder::make_holder(T *&val) {
  holder *h = new holder(type_holder_raw_get<T>::type);
  val = type_holder_raw_get<T>::get(h->val);
  return h;
}

} // namespace details

} // namespace soci

#endif // SOCI_TYPE_HOLDER_H_INCLUDED
