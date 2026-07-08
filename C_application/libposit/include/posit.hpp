// posit.hpp

#ifndef POSIT_HPP
#define POSIT_HPP

#include "posit.h"

#ifdef USE_CONSTEVAL // force compile-time evaluation
#define CONSTEXPR consteval
#else
#define CONSTEXPR constexpr
#endif

// NOTE: It seems there is hardware latency issue on pmvxw.
//
// Whether we let LLVM auto decide when to use pmvxw, or we manually call pmvxw
// on .value(), we observe similar latency-related issues, where raw posit data
// is not always immediately available, and may be overwritten when accessed
// later. The current workaround is to use psw to route data through memory
// when accessing raw posit values, which prevents LLVM from using pmvxw
// shortcut to move data to GPR. The hardware should be investigated to
// determine the exact cause of this issue.
//
// Side note:
// Using psw also avoids sign bit extensions in posit16 when dealing with GPR.

class Posit {
    posit_t v; // private field, use `value` method

public:
    CONSTEXPR Posit() : v(0) { }

    CONSTEXPR Posit(float x) : v(float2posit(x)) { }
    template<typename T>
    CONSTEXPR Posit(T x) : v(float2posit(static_cast<float>(x))) { }

    explicit operator float() const { return posit2float(v); }
    template<typename T>
    explicit operator T() const { return static_cast<T>(posit2float(v)); }

    inline posit_t value() {
        return psw(v);
    }

    inline posit_t value_unsafe() {
        // this should only be used by library functions
        // this should not be used by application programmers
        return v;
    }

    // call `from_bits` to directly set posit numbers
    static CONSTEXPR inline Posit from_bits(posit_t x){
        Posit p;
        p.v = x;
        return p;
    }

    static inline Posit min(Posit x, Posit y){
        Posit p;
        p.v = pmin(x.v, y.v);
        return p;
    }

    static inline Posit max(Posit x, Posit y){
        Posit p;
        p.v = pmax(x.v, y.v);
        return p;
    }

    static inline Posit sqrt(Posit x){
        Posit p;
        p.v = psqrt(x.v);
        return p;
    }

    Posit& operator+=(Posit y) {
        v = padd(v, y.v);
        return *this;
    }

    Posit& operator-=(Posit y) {
        v = psub(v, y.v);
        return *this;
    }

    Posit& operator*=(Posit y) {
        v = pmul(v, y.v);
        return *this;
    }

    Posit& operator/=(Posit y) {
        v = pdiv(v, y.v);
        return *this;
    }

    friend inline Posit operator+(Posit x, Posit y) { return x += y; }
    friend inline Posit operator-(Posit x) { return Posit(0) -= x; }
    friend inline Posit operator-(Posit x, Posit y) { return x -= y; }
    friend inline Posit operator*(Posit x, Posit y) { return x *= y; }
    friend inline Posit operator/(Posit x, Posit y) { return x /= y; }

    friend inline bool operator==(Posit x, Posit y) { return peq(x.v, y.v); }
    friend inline bool operator< (Posit x, Posit y) { return plt(x.v, y.v); }
    friend inline bool operator<=(Posit x, Posit y) { return ple(x.v, y.v); }
    friend inline bool operator> (Posit x, Posit y) { return !ple(x.v, y.v); }
    friend inline bool operator>=(Posit x, Posit y) { return !plt(x.v, y.v); }

    template<typename T>
    friend inline Posit operator+(Posit x, T y) { return x += Posit(y); }
    template<typename T>
    friend inline Posit operator+(T x, Posit y) { return Posit(x) += y; }

    template<typename T>
    friend inline Posit operator-(Posit x, T y) { return x -= Posit(y); }
    template<typename T>
    friend inline Posit operator-(T x, Posit y) { return Posit(x) -= y; }

    template<typename T>
    friend inline Posit operator*(Posit x, T y) { return x *= Posit(y); }
    template<typename T>
    friend inline Posit operator*(T x, Posit y) { return Posit(x) *= y; }

    template<typename T>
    friend inline Posit operator/(Posit x, T y) { return x /= Posit(y); }
    template<typename T>
    friend inline Posit operator/(T x, Posit y) { return Posit(x) /= y; }

    template<typename T>
    friend inline bool operator==(Posit x, T y) { return peq(x.v, Posit(y).v); }
    template<typename T>
    friend inline bool operator==(T x, Posit y) { return peq(Posit(x).v, y.v); }

    template<typename T>
    friend inline bool operator< (Posit x, T y) { return plt(x.v, Posit(y).v); }
    template<typename T>
    friend inline bool operator< (T x, Posit y) { return plt(Posit(x).v, y.v); }

    template<typename T>
    friend inline bool operator<=(Posit x, T y) { return ple(x.v, Posit(y).v); }
    template<typename T>
    friend inline bool operator<=(T x, Posit y) { return ple(Posit(x).v, y.v); }

    template<typename T>
    friend inline bool operator> (Posit x, T y) { return !ple(x.v, Posit(y).v); }
    template<typename T>
    friend inline bool operator> (T x, Posit y) { return !ple(Posit(x).v, y.v); }

    template<typename T>
    friend inline bool operator>=(Posit x, T y) { return !plt(x.v, Posit(y).v); }
    template<typename T>
    friend inline bool operator>=(T x, Posit y) { return !plt(Posit(x).v, y.v); }
};

namespace libposit {
    static inline Posit abs(Posit x) {
        return Posit::from_bits(psgnjxs(x.value_unsafe(), x.value_unsafe()));
    }
    static inline Posit fabs(Posit x) { return abs(x); }

    static inline Posit sqrt(Posit x) { return Posit::sqrt(x); }
    static inline Posit sqrtf(Posit x) { return sqrt(x); }
}

#endif
