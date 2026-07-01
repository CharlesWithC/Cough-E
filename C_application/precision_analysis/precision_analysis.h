#ifndef _PRECISION_ANALYSIS_H_
#define _PRECISION_ANALYSIS_H_

#include <type_traits>
#include <vector>

#include <audio_model.h>
#include <frequency_features.h>
#include <types.h>

template <typename Target, typename Src, typename Enable = void> struct VariableConverter;

template <typename Target, typename Src>
struct VariableConverter<Target, Src, std::enable_if_t<!std::is_pointer_v<Src>>> {
    Src &original;
    Target storage;
    VariableConverter(Src &orig, size_t) : original(orig), storage(static_cast<Target>(orig)) {}
    ~VariableConverter() {
        if constexpr (!std::is_const_v<Src>) {
            original = static_cast<Src>(storage);
        }
    }
    Target &get() { return storage; }
};

template <typename Target, typename Src>
struct VariableConverter<Target, Src, std::enable_if_t<std::is_pointer_v<Src>>> {
    using ElementType = std::remove_cv_t<std::remove_pointer_t<Src>>;
    Src original;
    size_t size;
    std::vector<Target> storage;
    VariableConverter(Src orig, size_t sz) : original(orig), size(sz), storage(sz) {
        for (size_t i = 0; i < size; ++i) {
            storage[i] = static_cast<Target>(orig[i]);
        }
    }
    ~VariableConverter() {
        if constexpr (!std::is_const_v<std::remove_pointer_t<Src>>) {
            for (size_t i = 0; i < size; ++i) {
                original[i] = static_cast<ElementType>(storage[i]);
            }
        }
    }
    Target *get() { return storage.data(); }
};

#define CONV_P1(var, size) VariableConverter<_TARGET_TYPE, decltype(var)> _conv_##var(var, size);
#define CONV_P2(var, size) auto &&var = _conv_##var.get();

#define DEFINE_WRAPPER(ctrl, mask, func_name, template_suffix, src_type, target_type, param_list, call_args, var_list) \
    template <int _dummy = 0, std::enable_if_t<(_dummy == 0) && (((ctrl) & (mask)) != 0), int> = 0>                    \
    static inline src_type wrapper_##func_name param_list {                                                            \
        using _TARGET_TYPE = target_type;                                                                              \
        var_list(CONV_P1) {                                                                                            \
            var_list(CONV_P2) { return static_cast<src_type>(func_name template_suffix call_args); }                   \
        }                                                                                                              \
    }

#define DEFINE_PASSTHROUGH(ctrl, mask, func, T, ret_t, params, args)                                                   \
    template <int _dummy = 0, std::enable_if_t<(_dummy == 0) && (((ctrl) & (mask)) == 0), int> = 0>                    \
    inline ret_t wrapper_##func params {                                                                               \
        return func<T> args;                                                                                           \
    }

#define DEFINE_VOID_WRAPPER(ctrl, mask, func_name, template_suffix, target_type, param_list, call_args, var_list)      \
    template <int _dummy = 0, std::enable_if_t<(_dummy == 0) && (((ctrl) & (mask)) != 0), int> = 0>                    \
    static inline void wrapper_##func_name param_list {                                                                \
        using _TARGET_TYPE = target_type;                                                                              \
        var_list(CONV_P1) { var_list(CONV_P2) func_name template_suffix call_args; }                                   \
    }

#define DEFINE_VOID_PASSTHROUGH(ctrl, mask, func, T, params, args)                                                     \
    template <int _dummy = 0, std::enable_if_t<(_dummy == 0) && (((ctrl) & (mask)) == 0), int> = 0>                    \
    inline void wrapper_##func params {                                                                                \
        func<T> args;                                                                                                  \
    }

#ifndef PRECISION_CTRL1
#define PRECISION_CTRL1 0
#endif
#ifndef PRECISION_CTRL2
#define PRECISION_CTRL2 0
#endif

#if defined(PRECISION_CTRL1) || defined(PRECISION_CTRL2)
#define PA_LOG(category, name, value)                                                                                  \
    printf("PRECISION|%04X%04X|%s|%s|%f\n", (unsigned int)(PRECISION_CTRL1), (unsigned int)(PRECISION_CTRL2),          \
           (category), (name), (float)(value))
#else
#define PA_LOG(category, name, value) ((void)0)
#endif

// 00 = no conversion / 01 = use sreal / 10 = use mreal / 11 = not used (reserved for possible custom type mix)
// 0000 0000 0000 0000 PRECISION_CTRL1 (FFT)
// []                  compute_spec_decrease
//   []                compute_spectral_slope
//      []             compute_rolloff
//        []           compute_centroid
//           []        compute_spread
//             []      compute_kurt
//                []   compute_skew
//                  [] compute_rfft
// 0000 0000 0000 0000 PRECISION_CTRL2 (Periodogram + MFCC + MEL)
// []                  compute_periodogram
//   []                compute_flatness
//      []             compute_std
//        []           compute_spectral_entropy
//           []        get_domiant_freq
//             []      normalized_bandpowers
//                []   get_mfcc_features
//                  [] get_mel_spectrogram_features

#define RFFT(op) op(sig, len) op(mags, (len / 2) + 1) op(freqs, (len / 2) + 1) op(sum_mags, 1)
#define MFLS(op) op(mags, len) op(freqs, len) op(sum_mags, 0)
#define SPRD(op) op(mags, len) op(freqs, len) op(sum_mags, 0) op(centroid, 0)
#define KTSK(op) op(mags, len) op(freqs, len) op(sum_mags, 0) op(centroid, 0) op(spread, 0)

DEFINE_WRAPPER(PRECISION_CTRL1, 0x8000, compute_spec_decrease, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x4000, compute_spec_decrease, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0xC000, compute_spec_decrease, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags))

DEFINE_WRAPPER(PRECISION_CTRL1, 0x2000, compute_spectral_slope, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x1000, compute_spectral_slope, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0x3000, compute_spectral_slope, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags))

DEFINE_WRAPPER(PRECISION_CTRL1, 0x0800, compute_rolloff, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x0400, compute_rolloff, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0x0C00, compute_rolloff, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags))

DEFINE_WRAPPER(PRECISION_CTRL1, 0x0200, compute_centroid, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x0100, compute_centroid, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags), MFLS)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0x0300, compute_centroid, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags), (mags, freqs, len, sum_mags))

DEFINE_WRAPPER(PRECISION_CTRL1, 0x0080, compute_spread, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid),
               (mags, freqs, len, sum_mags, centroid), SPRD)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x0040, compute_spread, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid),
               (mags, freqs, len, sum_mags, centroid), SPRD)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0x00C0, compute_spread, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid),
                   (mags, freqs, len, sum_mags, centroid))

DEFINE_WRAPPER(PRECISION_CTRL1, 0x0020, compute_kurt, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid, real_t spread),
               (mags, freqs, len, sum_mags, centroid, spread), KTSK)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x0010, compute_kurt, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid, real_t spread),
               (mags, freqs, len, sum_mags, centroid, spread), KTSK)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0x0030, compute_kurt, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid, real_t spread),
                   (mags, freqs, len, sum_mags, centroid, spread))

DEFINE_WRAPPER(PRECISION_CTRL1, 0x0008, compute_skew, <mreal_t>, real_t, mreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid, real_t spread),
               (mags, freqs, len, sum_mags, centroid, spread), KTSK)
DEFINE_WRAPPER(PRECISION_CTRL1, 0x0004, compute_skew, <sreal_t>, real_t, sreal_t,
               (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid, real_t spread),
               (mags, freqs, len, sum_mags, centroid, spread), KTSK)
DEFINE_PASSTHROUGH(PRECISION_CTRL1, 0x000C, compute_skew, real_t, real_t,
                   (real_t * mags, real_t *freqs, int16_t len, real_t sum_mags, real_t centroid, real_t spread),
                   (mags, freqs, len, sum_mags, centroid, spread))

DEFINE_VOID_WRAPPER(PRECISION_CTRL1, 0x0002, compute_rfft, <mreal_t>, mreal_t,
                    (const real_t *sig, int16_t len, int16_t fs, real_t *mags, real_t *freqs, real_t *sum_mags),
                    (sig, len, fs, mags, freqs, sum_mags), RFFT)
DEFINE_VOID_WRAPPER(PRECISION_CTRL1, 0x0001, compute_rfft, <sreal_t>, sreal_t,
                    (const real_t *sig, int16_t len, int16_t fs, real_t *mags, real_t *freqs, real_t *sum_mags),
                    (sig, len, fs, mags, freqs, sum_mags), RFFT)
DEFINE_VOID_PASSTHROUGH(PRECISION_CTRL1, 0x0003, compute_rfft, real_t,
                        (const real_t *sig, int16_t len, int16_t fs, real_t *mags, real_t *freqs, real_t *sum_mags),
                        (sig, len, fs, mags, freqs, sum_mags))

#define PERIO(op) op(sig, len) op(psd, (NPERSEG / 2) + 1) op(freqs, (NPERSEG / 2) + 1)
#define XLEN(op) op(x, len)
#define DOMF(op) op(psd, len) op(freqs, len)
#define NBPW(op) op(psd, len) op(freqs, len) op(band_powers, N_PSD)
#define MFCF(op) op(x, len) op(mean_mfcc, N_MFCC) op(std_mfcc, N_MFCC)
#define MELS(op)                                                                                                       \
    op(x, len) op(mean_mel_spectr, n_mels_needed) op(std_mel_spectr, n_mels_needed) op(max_mel_spectr, n_mels_needed)  \
        op(entropy_mel_spectr, n_mels_needed)

DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x8000, compute_periodogram, <mreal_t>, mreal_t,
                    (const real_t *sig, int16_t len, int16_t fs, real_t *psd, real_t *freqs),
                    (sig, len, fs, psd, freqs), PERIO)
DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x4000, compute_periodogram, <sreal_t>, sreal_t,
                    (const real_t *sig, int16_t len, int16_t fs, real_t *psd, real_t *freqs),
                    (sig, len, fs, psd, freqs), PERIO)
DEFINE_VOID_PASSTHROUGH(PRECISION_CTRL2, 0xC000, compute_periodogram, real_t,
                        (const real_t *sig, int16_t len, int16_t fs, real_t *psd, real_t *freqs),
                        (sig, len, fs, psd, freqs))

DEFINE_WRAPPER(PRECISION_CTRL2, 0x2000, compute_flatness, <mreal_t>, real_t, mreal_t, (real_t * x, int16_t len),
               (x, len), XLEN)
DEFINE_WRAPPER(PRECISION_CTRL2, 0x1000, compute_flatness, <sreal_t>, real_t, sreal_t, (real_t * x, int16_t len),
               (x, len), XLEN)
DEFINE_PASSTHROUGH(PRECISION_CTRL2, 0x3000, compute_flatness, real_t, real_t, (real_t * x, int16_t len), (x, len))

DEFINE_WRAPPER(PRECISION_CTRL2, 0x0800, compute_std, <mreal_t>, real_t, mreal_t, (real_t * x, int16_t len), (x, len),
               XLEN)
DEFINE_WRAPPER(PRECISION_CTRL2, 0x0400, compute_std, <sreal_t>, real_t, sreal_t, (real_t * x, int16_t len), (x, len),
               XLEN)
DEFINE_PASSTHROUGH(PRECISION_CTRL2, 0x0C00, compute_std, real_t, real_t, (real_t * x, int16_t len), (x, len))

DEFINE_WRAPPER(PRECISION_CTRL2, 0x0200, compute_spectral_entropy, <mreal_t>, real_t, mreal_t, (real_t * x, int16_t len),
               (x, len), XLEN)
DEFINE_WRAPPER(PRECISION_CTRL2, 0x0100, compute_spectral_entropy, <sreal_t>, real_t, sreal_t, (real_t * x, int16_t len),
               (x, len), XLEN)
DEFINE_PASSTHROUGH(PRECISION_CTRL2, 0x0300, compute_spectral_entropy, real_t, real_t, (real_t * x, int16_t len),
                   (x, len))

DEFINE_WRAPPER(PRECISION_CTRL2, 0x0080, get_domiant_freq, <mreal_t>, real_t, mreal_t,
               (real_t * psd, real_t *freqs, int16_t len), (psd, freqs, len), DOMF)
DEFINE_WRAPPER(PRECISION_CTRL2, 0x0040, get_domiant_freq, <sreal_t>, real_t, sreal_t,
               (real_t * psd, real_t *freqs, int16_t len), (psd, freqs, len), DOMF)
DEFINE_PASSTHROUGH(PRECISION_CTRL2, 0x00C0, get_domiant_freq, real_t, real_t,
                   (real_t * psd, real_t *freqs, int16_t len), (psd, freqs, len))

DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x0020, normalized_bandpowers, <mreal_t>, mreal_t,
                    (real_t * psd, real_t *freqs, int16_t len, const int8_t *psd_selector, real_t *band_powers),
                    (psd, freqs, len, psd_selector, band_powers), NBPW)
DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x0010, normalized_bandpowers, <sreal_t>, sreal_t,
                    (real_t * psd, real_t *freqs, int16_t len, const int8_t *psd_selector, real_t *band_powers),
                    (psd, freqs, len, psd_selector, band_powers), NBPW)
DEFINE_VOID_PASSTHROUGH(PRECISION_CTRL2, 0x0030, normalized_bandpowers, real_t,
                        (real_t * psd, real_t *freqs, int16_t len, const int8_t *psd_selector, real_t *band_powers),
                        (psd, freqs, len, psd_selector, band_powers))

DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x0008, get_mfcc_features, <mreal_t>, mreal_t,
                    (const real_t *x, int16_t len, real_t *mean_mfcc, real_t *std_mfcc), (x, len, mean_mfcc, std_mfcc),
                    MFCF)
DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x0004, get_mfcc_features, <sreal_t>, sreal_t,
                    (const real_t *x, int16_t len, real_t *mean_mfcc, real_t *std_mfcc), (x, len, mean_mfcc, std_mfcc),
                    MFCF)
DEFINE_VOID_PASSTHROUGH(PRECISION_CTRL2, 0x000C, get_mfcc_features, real_t,
                        (const real_t *x, int16_t len, real_t *mean_mfcc, real_t *std_mfcc),
                        (x, len, mean_mfcc, std_mfcc))

DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x0002, get_mel_spectrogram_features, <mreal_t>, mreal_t,
                    (const real_t *x, int16_t len, uint8_t *idx_needed, uint8_t n_mels_needed, real_t *mean_mel_spectr,
                     real_t *std_mel_spectr, real_t *max_mel_spectr, real_t *entropy_mel_spectr),
                    (x, len, idx_needed, n_mels_needed, mean_mel_spectr, std_mel_spectr, max_mel_spectr,
                     entropy_mel_spectr),
                    MELS)
DEFINE_VOID_WRAPPER(PRECISION_CTRL2, 0x0001, get_mel_spectrogram_features, <sreal_t>, sreal_t,
                    (const real_t *x, int16_t len, uint8_t *idx_needed, uint8_t n_mels_needed, real_t *mean_mel_spectr,
                     real_t *std_mel_spectr, real_t *max_mel_spectr, real_t *entropy_mel_spectr),
                    (x, len, idx_needed, n_mels_needed, mean_mel_spectr, std_mel_spectr, max_mel_spectr,
                     entropy_mel_spectr),
                    MELS)
DEFINE_VOID_PASSTHROUGH(PRECISION_CTRL2, 0x0003, get_mel_spectrogram_features, real_t,
                        (const real_t *x, int16_t len, uint8_t *idx_needed, uint8_t n_mels_needed,
                         real_t *mean_mel_spectr, real_t *std_mel_spectr, real_t *max_mel_spectr,
                         real_t *entropy_mel_spectr),
                        (x, len, idx_needed, n_mels_needed, mean_mel_spectr, std_mel_spectr, max_mel_spectr,
                         entropy_mel_spectr))

#endif
