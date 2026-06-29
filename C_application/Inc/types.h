#pragma once

#ifdef FXP_MODE
#include <FxP/core/fxp_core.h>
#endif

#ifdef UPOS_MODE // universal posit
#ifndef __cplusplus
#error "UPOS requires compiling with g++"
#endif
#include <universal/number/posit/posit.hpp>
using namespace sw::universal;
typedef posit<24, 2> real_t;
typedef posit<16, 2> sreal_t;
#else
#ifdef LPOS_MODE // libposit
#ifndef __cplusplus
#error "LPOS requires compiling with g++"
#endif
#define POSIT_SIZE 32
#include <posit.hpp>
using namespace libposit;
typedef Posit real_t;
typedef Posit sreal_t;
#define POS Posit::from_bits
#else
typedef float real_t;
typedef float sreal_t;
#endif
#endif

#ifndef FXP_MODE
typedef real_t cough_audio_sample_t;
typedef real_t cough_imu_sample_t;
typedef real_t cough_feat_t;
typedef real_t cough_audio_feat_t;
typedef real_t cough_imu_feat_t;

typedef cough_feat_t feat_t;
typedef cough_audio_feat_t audio_feat_t;
typedef cough_imu_feat_t imu_feat_t;
typedef cough_audio_sample_t audio_sample_t;
typedef cough_imu_sample_t imu_sample_t;

typedef sreal_t audio_score_t;
typedef sreal_t imu_score_t;
#endif // for FXP_MODE, these are defined in fxp_core.h

#ifdef UPOS_MODE
#include <concepts>
template <typename T>
concept RealType =
    std::same_as<T, real_t> || std::same_as<T, audio_feat_t> ||
    std::same_as<T, imu_feat_t> || std::same_as<T, audio_sample_t> ||
    std::same_as<T, imu_sample_t>;
#else
#define RealType                                                               \
    typename // use generic typename for non-upos (including lpos) because
             // concept is c++20
#endif

#ifdef HEEPATIA_MODE
#define DINTL0                                                                 \
    __attribute__((section(".xheep_data_interleaved_sec0")))                   \
    __attribute__((aligned(4)))
#define DINTL1                                                                 \
    __attribute__((section(".xheep_data_interleaved_sec1")))                   \
    __attribute__((aligned(4)))
#define FLASH0                                                                 \
    __attribute__((section(".xheep_data_flash_only_sec0")))                    \
    __attribute__((aligned(4)))
#define FLASH1                                                                 \
    __attribute__((section(".xheep_data_flash_only_sec1")))                    \
    __attribute__((aligned(4)))
#define FLASH2                                                                 \
    __attribute__((section(".xheep_data_flash_only_sec2")))                    \
    __attribute__((aligned(4)))
#define FLASH3                                                                 \
    __attribute__((section(".xheep_data_flash_only_sec3")))                    \
    __attribute__((aligned(4)))
#define CARUS00                                                                \
    __attribute__((section(".xheep_data_carus0_sec0")))                        \
    __attribute__((aligned(4)))
#define CARUS01                                                                \
    __attribute__((section(".xheep_data_carus0_sec1")))                        \
    __attribute__((aligned(4)))
#define CARUS10                                                                \
    __attribute__((section(".xheep_data_carus1_sec0")))                        \
    __attribute__((aligned(4)))
#define CARUS11                                                                \
    __attribute__((section(".xheep_data_carus1_sec1")))                        \
    __attribute__((aligned(4)))
#else
#define DINTL0
#define DINTL1
#define FLASH0
#define FLASH1
#define FLASH2
#define FLASH3
#define CARUS00
#define CARUS01
#define CARUS10
#define CARUS11
#endif
