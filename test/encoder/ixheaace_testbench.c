/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
 */
/*****************************************************************************/
/* File includes                                                             */
/*****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ixheaac_type_def.h"
#include "ixheaace_aac_constants.h"
#include "ixheaac_error_standards.h"
#include "ixheaace_error_handler.h"

#include "ixheaace_api.h"
#include "ixheaace_memory_standards.h"
#include "ixheaace_config_params.h"
#include "iusace_cnst.h"
#include "ixheaac_constants.h"
#include "ixheaac_basic_ops32.h"
#include "ixheaac_basic_ops40.h"
#include "ixheaac_basic_ops.h"

VOID ia_enhaacplus_enc_error_handler_init();
VOID ia_testbench_error_handler_init();

extern ia_error_info_struct ia_testbench_error_info;
extern ia_error_info_struct ia_enhaacplus_enc_error_info;

/*****************************************************************************/
/* Process select hash defines                                               */
/*****************************************************************************/
#define WAV_READER
#define DISPLAY_MESSAGE

/*****************************************************************************/
/* Constant hash defines                                                     */
/*****************************************************************************/
#define MAX_MEM_ALLOCS 100
#define IA_MAX_CMD_LINE_LENGTH 300
#define IA_MAX_ARGS 20
#define IA_SCREEN_WIDTH 80

#define PARAMFILE "paramfilesimple.txt"

/*****************************************************************************/
/* Error codes for the testbench                                             */
/*****************************************************************************/
#define IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED 0xFFFFA001

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/
pVOID g_pv_arr_alloc_memory[MAX_MEM_ALLOCS];
WORD g_w_malloc_count;
FILE *g_pf_inp, *g_pf_out;
FILE *g_drc_inp = NULL;
FILE *g_pf_meta;
WORD32 eld_ga_hdr;
WORD8 pb_drc_file_path[IA_MAX_CMD_LINE_LENGTH] = "";

int ia_enhaacplus_enc_fread(void *buf, int size, int bytes, FILE *fp) {
  return (int)fread(buf, size, bytes, fp);
}

int ia_enhaacplus_enc_fwrite(void *pb_buf, FILE *pf_out, WORD32 i_out_bytes) {
  fwrite(pb_buf, sizeof(char), i_out_bytes, pf_out);
  return 1;
}

IA_ERRORCODE ia_enhaacplus_enc_wav_header_decode(FILE *in_file, UWORD32 *n_channels,
                                                 UWORD32 *i_channel_mask, UWORD32 *sample_rate,
                                                 UWORD32 *pcm_sz, WORD32 *length) {
  WORD8 wav_hdr[40 + 36];
  WORD8 data_start[4];
  WORD16 num_ch;
  UWORD32 f_samp;
  WORD16 output_format;
  WORD32 check, count = 0;
  FLAG wav_format_pcm = 0, wav_format_extensible = 0;
  UWORD16 cbSize = 0;
  WORD32 curr_pos, size;

  *i_channel_mask = 0;

  if (fread(wav_hdr, 1, 40, in_file) != 40) return 1;

  if (wav_hdr[0] != 'R' && wav_hdr[1] != 'I' && wav_hdr[2] != 'F' && wav_hdr[3] != 'F') {
    return 1;
  }

  if (wav_hdr[20] == 01 && wav_hdr[21] == 00) {
    wav_format_pcm = 1;
  } else if (wav_hdr[20] == ((WORD8)-2) && wav_hdr[21] == ((WORD8)-1)) {
    wav_format_extensible = 1;
  } else {
    return 1;
  }

  num_ch = (WORD16)((UWORD8)wav_hdr[23] * 256 + (UWORD8)wav_hdr[22]);
  f_samp = ((UWORD8)wav_hdr[27] * 256 * 256 * 256);
  f_samp += ((UWORD8)wav_hdr[26] * 256 * 256);
  f_samp += ((UWORD8)wav_hdr[25] * 256);
  f_samp += ((UWORD8)wav_hdr[24]);

  output_format = ((UWORD8)wav_hdr[35] * 256);
  output_format += ((UWORD8)wav_hdr[34]);

  *n_channels = num_ch;
  *sample_rate = f_samp;
  *pcm_sz = output_format;

  if (wav_format_pcm) {
    data_start[0] = wav_hdr[36];
    data_start[1] = wav_hdr[37];
    data_start[2] = wav_hdr[38];
    data_start[3] = wav_hdr[39];
  } else if (wav_format_extensible) {
    cbSize |= ((UWORD8)wav_hdr[37] << 8);
    cbSize |= ((UWORD8)wav_hdr[36]);

    if (fread(&(wav_hdr[40]), 1, (UWORD16)(cbSize - 2 + 4), in_file) != (UWORD16)(cbSize - 2 + 4))
      return 1;

    if (cbSize > 34) {
      return 1;
    }

    *i_channel_mask = 0;
    *i_channel_mask |= (UWORD8)wav_hdr[43] << 24;
    *i_channel_mask |= (UWORD8)wav_hdr[42] << 16;
    *i_channel_mask |= (UWORD8)wav_hdr[41] << 8;
    *i_channel_mask |= (UWORD8)wav_hdr[40];

    data_start[0] = wav_hdr[40 + cbSize - 2 + 0];
    data_start[1] = wav_hdr[40 + cbSize - 2 + 1];
    data_start[2] = wav_hdr[40 + cbSize - 2 + 2];
    data_start[3] = wav_hdr[40 + cbSize - 2 + 3];
  }

  check = 1;
  while (check) {
    if (data_start[0] == 'd' && data_start[1] == 'a' && data_start[2] == 't' &&
        data_start[3] == 'a') {
      if (1 != fread(length, 4, 1, in_file)) return 1;
      check = 0;

      curr_pos = ftell(in_file);
      fseek(in_file, 0L, SEEK_END);
      size = ftell(in_file) - curr_pos;
      if (*length > size) {
        printf("\n Inconsitent file size \n");
      }
      *length = MIN(*length, size);
      fseek(in_file, curr_pos, SEEK_SET);
    } else {
      data_start[0] = data_start[1];
      data_start[1] = data_start[2];
      data_start[2] = data_start[3];
      if (1 != fread(&data_start[3], 1, 1, in_file)) return 1;
    }
    count++;
    if (count > 40) {
      *length = 0xffffffff;
      return (1);
    }
  }
  return IA_NO_ERROR;
}

void ia_enhaacplus_enc_print_usage() {
  printf("\nUsage:\n");
  printf("\n<executable> -ifile:<inputfile> -ofile:<outputfile> [options]\n");
  printf("\nor\n");
  printf("\n<executable> -paramfile:<paramfile>\n");
  printf("\n[options] can be,");
  printf("\n[-br:<bitrate>]");
  printf("\n[-mps:<use_mps>]");
  printf("\n[-adts:<use_adts_flag (0/1)>]");
  printf("\n[-tns:<use_tns_flag>]");
  printf("\n[-framesize:<framesize_to_be_used>]");
  printf("\n[-aot:<audio_object_type>]");
  printf("\n[-esbr:<esbr_flag (0/1)>]");
  printf("\n[-full_bandwidth:<Enable use of full bandwidth of input>]");
  printf("\n[-max_out_buffer_per_ch:<bitreservoir_size>]");
  printf("\n[-tree_cfg:<tree_config>]");
  printf("\n\nwhere, \n  <paramfile> is the parameter file with multiple commands");
  printf("\n  <inputfile> is the input 16-bit WAV or PCM file name");
  printf("\n  <outputfile> is the output ADTS/ES file name");
  printf("\n  <bitrate> is the bit-rate in bits per second. Valid values are ");
  printf("\n    Plain AAC: 8000-576000 bps per channel");
  printf("\n  <use_mps> When set to 1 MPS is enable. Default 0.");
  printf("\n  <use_adts_flag> when set to 1 ADTS header generated.");
  printf("\n                  Default is 0");
  printf("\n  <use_tns_flag> controls usage of TNS in encoding. Default is 1");
  printf("\n  <framesize_to_be_used> is the framesize to be used.");
  printf(
      "\n    For AOT 23, 39 (LD core coder profiles) valid values are 480 and 512 .Default is "
      "512");
  printf(
      "\n    For AOT 2, 5, 29 (LC core coder profiles) valid values are 960 and 1024 .Default "
      "is 1024");
  printf("\n  <audio_object_type> is the Audio object type");
  printf("\n    2 for AAC LC");
  printf("\n    5 for HEAACv1(Legacy SBR)");
  printf("\n    23 for AAC LD");
  printf("\n    29 for HEAACv2");
  printf("\n    39 for AAC ELD");
  printf("\n    Default is 2 for AAC LC");
  printf("\n  <esbr_flag> when set to 1 enables eSBR in HEAACv1 encoding");
  printf("\n                  Default is 0");
  printf(
      "\n  <full_bandwidth> Enable use of full bandwidth of input. Valid values are 0(disable) "
      "and 1(enable). Default is 0.");
  printf("\n  <bitreservoir_size> is the maximum size of bit reservoir to be used.");
  printf("\n    Valid values are from -1 to 6144. -1 to omit use of bit reservoir.");
  printf("\n    Default is 384.");
  printf(
      "\n  <tree_config> MPS tree config"
      "0 for '212'"
      "1 for '5151'"
      "2 for '5152'"
      "3 for '525'"
      "Default '212' for stereo input '515' for 6ch input");
  exit(1);
}

static VOID ixheaace_parse_config_param(WORD32 argc, pWORD8 argv[], pVOID ptr_enc_api) {
  LOOPIDX i;
  ixheaace_user_config_struct *pstr_enc_api = (ixheaace_user_config_struct *)ptr_enc_api;

  for (i = 0; i < argc; i++) {
    /* Stream bit rate */
    if (!strncmp((const char *)argv[i], "-br:", 4)) {
      char *pb_arg_val = (char *)argv[i] + 4;
      pstr_enc_api->input_config.i_bitrate = atoi(pb_arg_val);
      fprintf(stdout, "Stream bit rate = %d\n", pstr_enc_api->input_config.i_bitrate);
    }
    /* MPS */
    if (!strncmp((const char *)argv[i], "-mps:", 5)) {
      char *pb_arg_val = (char *)argv[i] + 5;
      pstr_enc_api->input_config.i_use_mps = atoi(pb_arg_val);
    }
    /* Use TNS */
    if (!strncmp((const char *)argv[i], "-tns:", 5)) {
      char *pb_arg_val = (char *)argv[i] + 5;
      pstr_enc_api->input_config.aac_config.use_tns = atoi(pb_arg_val);
      pstr_enc_api->input_config.user_tns_flag = 1;
    }
    /*Use full bandwidth*/
    if (!strncmp((const char *)argv[i], "-full_bandwidth:", 16)) {
      char *pb_arg_val = (char *)argv[i] + 16;
      pstr_enc_api->input_config.aac_config.full_bandwidth = atoi(pb_arg_val);
    }
    /* frame size */
    if (!strncmp((const char *)argv[i], "-framesize:", 11)) {
      char *pb_arg_val = (char *)argv[i] + 11;
      pstr_enc_api->input_config.frame_length = atoi(pb_arg_val);
      pstr_enc_api->input_config.frame_cmd_flag = 1;
    }
    if (!strncmp((const char *)argv[i], "-aot:", 5)) {
      char *pb_arg_val = (char *)argv[i] + 5;
      pstr_enc_api->input_config.aot = atoi(pb_arg_val);
    }
    if (!strncmp((const char *)argv[i], "-esbr:", 6)) {
      char *pb_arg_val = (char *)argv[i] + 6;
      pstr_enc_api->input_config.esbr_flag = atoi(pb_arg_val);
    }

    if (!strncmp((const char *)argv[i], "-max_out_buffer_per_ch:", 23)) {
      char *pb_arg_val = (char *)argv[i] + 23;
      pstr_enc_api->input_config.aac_config.bitreservoir_size = atoi(pb_arg_val);
      pstr_enc_api->input_config.out_bytes_flag = 1;
    }
    if (!strncmp((const char *)argv[i], "-tree_cfg:", 10)) {
      pCHAR8 pb_arg_val = (pCHAR8)(argv[i] + 10);
      pstr_enc_api->input_config.i_mps_tree_config = atoi(pb_arg_val);
    }
    if (!strncmp((const char *)argv[i], "-adts:", 6)) {
      pCHAR8 pb_arg_val = (pCHAR8)(argv[i] + 6);
      pstr_enc_api->input_config.i_use_adts = atoi(pb_arg_val);
    }
  }

  return;
}

VOID ia_enhaacplus_enc_display_id_message(WORD8 lib_name[], WORD8 lib_version[]) {
  WORD8 str[4][IA_SCREEN_WIDTH] = {"ITTIAM SYSTEMS PVT LTD, BANGALORE\n",
                                   "http:\\\\www.ittiam.com\n", "", ""};
  WORD8 spaces[IA_SCREEN_WIDTH / 2 + 1];
  WORD32 i, spclen;

  strcpy((pCHAR8)str[2], (pCHAR8)lib_name);
  strcat((pCHAR8)str[2], (pCHAR8)lib_version);
  strcat((pCHAR8)str[2], "\n");
  strcat((pCHAR8)str[4 - 1], "\n");

  for (i = 0; i < IA_SCREEN_WIDTH / 2 + 1; i++) {
    spaces[i] = ' ';
  }

  for (i = 0; i < 4; i++) {
    spclen = IA_SCREEN_WIDTH / 2 - (WORD32)(strlen((const char *)str[i]) / 2);
    spaces[spclen] = '\0';
    printf("%s", (const char *)spaces);
    spaces[spclen] = ' ';
    printf("%s", (const char *)str[i]);
  }
}

pVOID malloc_global(UWORD32 size, UWORD32 alignment) {
#ifdef WIN32
  return _aligned_malloc(size, alignment);
#else
  pVOID ptr = NULL;
  if (posix_memalign((VOID **)&ptr, alignment, size)) {
    ptr = NULL;
  }
  return ptr;
#endif
}

VOID free_global(pVOID ptr) {
#ifdef WIN32
  _aligned_free(ptr);
#else
  free(ptr);
#endif
  ptr = NULL;
}

static VOID iaace_aac_set_default_config(ixheaace_aac_enc_config *config) {
  /* make the pre initialization of the structs flexible */
  memset(config, 0, sizeof(*config));

  /* default configurations */
  config->bitrate = 48000;
  config->bandwidth = 0;
  config->inv_quant = 2;
  config->use_tns = 0;
  config->noise_filling = 0;
  config->bitreservoir_size = BITRESERVOIR_SIZE_CONFIG_PARAM_DEFAULT_VALUE_LC;
}

static VOID ixheaace_print_config_params(ixheaace_input_config *pstr_input_config,
                                         ixheaace_input_config *pstr_input_config_user) {
  printf(
      "\n*************************************************************************************"
      "***********\n");
  printf("\nParameters Taken:\n");
  if (pstr_input_config_user->aot == pstr_input_config->aot) {
    printf("\nAOT : %d", pstr_input_config->aot);
  } else {
    printf("\nAOT (Invalid config value, setting to default) : %d", pstr_input_config->aot);
  }
  if (pstr_input_config->aot == AOT_AAC_LC) {
    printf(" - AAC LC ");
  } else if (pstr_input_config->aot == AOT_SBR) {
    if (pstr_input_config->esbr_flag == 1) {
      printf(" - HEAACv1 (eSBR) ");
    } else {
      printf(" - HEAACv1 (Legacy SBR) ");
    }
  } else if (pstr_input_config->aot == AOT_AAC_ELD) {
    printf(" - AAC ELD");
    if (pstr_input_config->i_use_mps == 1) {
      printf("v2");
    }
  } else if (pstr_input_config->aot == AOT_AAC_LD) {
    printf(" - AAC LD ");
  } else if (pstr_input_config->aot == AOT_PS) {
    printf(" - HEAACv2 ");
  }
  if (pstr_input_config->aot == AOT_PS || pstr_input_config->aot == AOT_SBR) {
    if (pstr_input_config_user->esbr_flag == pstr_input_config->esbr_flag) {
      printf("\nESBR Flag : %d", pstr_input_config->esbr_flag);
    } else {
      printf("\nESBR Flag (Invalid config value, setting to default) : %d",
             pstr_input_config->esbr_flag);
    }
  }

  if (pstr_input_config_user->aac_config.noise_filling ==
      pstr_input_config->aac_config.noise_filling) {
    printf("\nNoise Filling Flag : %d", pstr_input_config->aac_config.noise_filling);
  } else {
    printf("\nNoise Filling Flag (Invalid config value, setting to default) : %d",
           pstr_input_config->aac_config.noise_filling);
  }
  if (pstr_input_config_user->i_use_adts == pstr_input_config->i_use_adts) {
    printf("\nUse ADTS Flag : %d", pstr_input_config->i_use_adts);
  } else {
    printf("\nUse ADTS Flag (Invalid config value, setting to default) : %d",
           pstr_input_config->i_use_adts);
  }

  if (pstr_input_config->aot != AOT_USAC) {
    if (pstr_input_config_user->aac_config.full_bandwidth ==
        pstr_input_config->aac_config.full_bandwidth) {
      printf("\nFull Bandwidth Flag : %d", pstr_input_config->aac_config.full_bandwidth);
    } else {
      printf("\nFull Bandwidth Flag (Invalid config value, setting to default) : %d",
             pstr_input_config->aac_config.full_bandwidth);
    }
  }
  if (pstr_input_config_user->aac_config.use_tns == pstr_input_config->aac_config.use_tns) {
    printf("\nUse TNS Flag : %d", pstr_input_config->aac_config.use_tns);
  } else {
    printf("\nUse TNS Flag (Invalid config value, setting to default) : %d",
           pstr_input_config->aac_config.use_tns);
  }
  if (pstr_input_config->aot == AOT_AAC_LD || pstr_input_config->aot == AOT_AAC_ELD) {
    if (pstr_input_config_user->aac_config.bitreservoir_size ==
        pstr_input_config->aac_config.bitreservoir_size) {
      printf("\nBitreservoir Size : %d", pstr_input_config->aac_config.bitreservoir_size);
    } else {
      printf("\nBitreservoir Size (Invalid config value, setting to default) : %d",
             pstr_input_config->aac_config.bitreservoir_size);
    }
  }
  if (pstr_input_config_user->i_bitrate == pstr_input_config->i_bitrate) {
    printf("\nBitrate : %d bps", pstr_input_config->i_bitrate);
  } else {
    printf("\nBitrate (Invalid config value, setting to default) : %d bps",
           pstr_input_config->i_bitrate);
  }
  if (pstr_input_config_user->i_use_mps != pstr_input_config->i_use_mps) {
    printf("\nMPS (Invalid config value, setting to default) : %d ",
           pstr_input_config->i_use_mps);
  }
  if (pstr_input_config->i_use_mps) {
    if (pstr_input_config_user->i_mps_tree_config == pstr_input_config->i_mps_tree_config) {
      printf("\nTree config : %d ", pstr_input_config->i_mps_tree_config);
    } else {
      printf("\nTree config (Invalid tree config value, setting to default) : %d ",
             pstr_input_config->i_mps_tree_config);
    }
  } else if (!(pstr_input_config->i_use_mps) && (pstr_input_config_user->i_mps_tree_config !=
                                                 pstr_input_config->i_mps_tree_config)) {
    printf("\nTree config (Invalid tree config value ) : %d ",
           pstr_input_config->i_mps_tree_config);
  }
  if (pstr_input_config->frame_cmd_flag) {
    if (pstr_input_config_user->frame_length == pstr_input_config->frame_length) {
      printf("\nFrame Length : %d", pstr_input_config->frame_length);
    } else {
      printf("\nFrame Length (Invalid config value, setting to default) : %d",
             pstr_input_config->frame_length);
    }
  } else {
    printf("\nFrame Length : %d", pstr_input_config->frame_length);
  }
  if ((pstr_input_config_user->i_samp_freq != pstr_input_config->i_samp_freq) &&
      (pstr_input_config->i_use_adts == 1)) {
    printf(
        "\nSampling Frequency (With adts:1, Setting non-standard Sampling Frequency to mapped "
        "standard Sampling Frequency) : %d Hz",
        pstr_input_config->i_samp_freq);
  } else {
    printf("\nSampling Frequency : %d Hz", pstr_input_config_user->i_samp_freq);
  }
  printf(
      "\n*************************************************************************************"
      "***********\n\n");
}
IA_ERRORCODE ia_enhaacplus_enc_main_process(WORD32 argc, pWORD8 argv[]) {
  LOOPIDX frame_count = 0;

  /* Error code */
  IA_ERRORCODE err_code = IA_NO_ERROR;

  /* API obj */
  pVOID pv_ia_process_api_obj;
  /* First part                                        */
  /* Error Handler Init                                */
  /* Get Library Name, Library Version and API Version */
  /* Initialize API structure + Default config set     */
  /* Set config params from user                       */
  /* Initialize memory tables                          */
  /* Get memory information and allocate memory        */

  pWORD8 pb_inp_buf = NULL, pb_out_buf = NULL;
  WORD32 i_bytes_read = 0;
  WORD32 input_size = 0;
  WORD32 samp_freq;
  FLOAT32 down_sampling_ratio = 1;
  WORD32 i_out_bytes = 0;
  WORD32 i_total_length = 0;
  WORD32 start_offset_samples = 0, i_dec_len = 0;
  UWORD32 *ia_stsz_size = NULL;
  UWORD32 ui_samp_freq, ui_num_chan, ui_pcm_wd_sz, ui_pcm = 0, ui_channel_mask,
                                                   ui_num_coupling_chans = 0;
  WORD32 max_frame_size = 0;
  WORD32 expected_frame_count = 0;

  /* The error init function */
  VOID (*p_error_init)();

  /* The process error info structure */
  ia_error_info_struct *p_proc_err_info;

  /* ******************************************************************/
  /* The API config structure                                         */
  /* ******************************************************************/
  ixheaace_user_config_struct *pstr_enc_api = (ixheaace_user_config_struct *)malloc_global(
      sizeof(ixheaace_user_config_struct), BYTE_ALIGN_8);
  memset(pstr_enc_api, 0, sizeof(ixheaace_user_config_struct));
  ixheaace_input_config *pstr_in_cfg = &pstr_enc_api->input_config;
  ixheaace_output_config *pstr_out_cfg = &pstr_enc_api->output_config;
  ixheaace_input_config *pstr_in_cfg_user =
      (ixheaace_input_config *)malloc_global(sizeof(ixheaace_input_config), BYTE_ALIGN_8);

  /* Stack process struct initing */
  p_error_init = ia_enhaacplus_enc_error_handler_init;
  p_proc_err_info = &ia_enhaacplus_enc_error_info;
  /* Stack process struct initing end */

  /* ******************************************************************/
  /* Initialize the error handler                                     */
  /* ******************************************************************/
  (*p_error_init)();

  /* ******************************************************************/
  /* Parse input configuration parameters                             */
  /* ******************************************************************/

  iaace_aac_set_default_config(&pstr_in_cfg->aac_config);

  pstr_in_cfg->i_bitrate = pstr_in_cfg->aac_config.bitrate;
  pstr_in_cfg->aot = AOT_AAC_LC;
  pstr_in_cfg->i_channels = 2;
  pstr_in_cfg->i_samp_freq = 44100;
  pstr_in_cfg->i_use_mps = 0;
  pstr_in_cfg->i_mps_tree_config = -1;
  pstr_in_cfg->i_use_adts = 0;
  pstr_in_cfg->esbr_flag = 0;
  pstr_in_cfg->i_use_es = 1;

  pstr_in_cfg->i_channels_mask = 0;
  pstr_in_cfg->i_num_coupling_chan = 0;
  pstr_in_cfg->aac_config.full_bandwidth = 0;
  pstr_out_cfg->malloc_xheaace = &malloc_global;
  pstr_out_cfg->free_xheaace = &free_global;
  pstr_in_cfg->frame_cmd_flag = 0;
  pstr_in_cfg->out_bytes_flag = 0;
  pstr_in_cfg->user_tns_flag = 0;
  pstr_in_cfg->i_use_adts = !eld_ga_hdr;
  /* ******************************************************************/
  /* Parse input configuration parameters                             */
  /* ******************************************************************/
  ixheaace_parse_config_param(argc, argv, pstr_enc_api);

  {
    if (pstr_in_cfg->aot == AOT_AAC_LC || pstr_in_cfg->aot == AOT_SBR ||
        pstr_in_cfg->aot == AOT_PS) {
      if (pstr_in_cfg->frame_cmd_flag == 0) {
        pstr_in_cfg->frame_length = 1024;
      }
      if (pstr_in_cfg->out_bytes_flag == 0) {
        pstr_in_cfg->aac_config.bitreservoir_size =
            BITRESERVOIR_SIZE_CONFIG_PARAM_DEFAULT_VALUE_LC;
      }
      if (pstr_in_cfg->user_tns_flag == 0) {
        pstr_in_cfg->aac_config.use_tns = 1;
      }
    } else if (pstr_in_cfg->aot == AOT_AAC_LD || pstr_in_cfg->aot == AOT_AAC_ELD) {
      if (pstr_in_cfg->frame_cmd_flag == 0) {
        pstr_in_cfg->frame_length = 512;
      }
      if (pstr_in_cfg->out_bytes_flag == 0) {
        pstr_in_cfg->aac_config.bitreservoir_size =
            BITRESERVOIR_SIZE_CONFIG_PARAM_DEFAULT_VALUE_LD;
      }

      if (pstr_in_cfg->user_tns_flag == 0) {
        pstr_in_cfg->aac_config.use_tns = 1;
      }
    }
  }

  ui_samp_freq = pstr_in_cfg->i_samp_freq;
  ui_num_chan = pstr_in_cfg->i_channels;
  ui_channel_mask = pstr_in_cfg->i_channels_mask;
  ui_num_coupling_chans = pstr_in_cfg->i_num_coupling_chan;
  ui_pcm_wd_sz = pstr_in_cfg->ui_pcm_wd_sz;
  if (!(pstr_in_cfg->i_use_adts)) {
    pstr_in_cfg->i_use_es = 1;
  } else {
    pstr_in_cfg->i_use_es = 0;
  }

  if (!ui_pcm) {
    /* Decode WAV header */
    if (ia_enhaacplus_enc_wav_header_decode(g_pf_inp, &ui_num_chan, &ui_channel_mask,
                                            &ui_samp_freq, &ui_pcm_wd_sz, &i_total_length) == 1) {
      fprintf(stdout, "Unable to Read Input WAV File\n");
      exit(1);
    }

    /* PCM Word Size (For single input file) */
    pstr_in_cfg->ui_pcm_wd_sz = ui_pcm_wd_sz;
    /* Sampling Frequency */
    pstr_in_cfg->i_samp_freq = ui_samp_freq;
    /* Total Number of Channels */
    pstr_in_cfg->i_channels = ui_num_chan;
    /* Number of coupling channels*/
    pstr_in_cfg->i_num_coupling_chan = ui_num_coupling_chans;
    /* Channels Mask */
    pstr_in_cfg->i_channels_mask = ui_channel_mask;

    pstr_in_cfg->aac_config.length = i_total_length;
  }

  /* Get library id and version number and display it */
  ixheaace_get_lib_id_strings((pVOID)&pstr_out_cfg->version);
  ia_enhaacplus_enc_display_id_message(pstr_out_cfg->version.p_lib_name,
                                       pstr_out_cfg->version.p_version_num);

  /* ******************************************************************/
  /* Initialize API structure and set config params to default        */
  /* ******************************************************************/

  memcpy(pstr_in_cfg_user, pstr_in_cfg, sizeof(ixheaace_input_config));

  err_code = ixheaace_create((pVOID)pstr_in_cfg, (pVOID)pstr_out_cfg);
  _IA_HANDLE_ERROR(p_proc_err_info, (pWORD8) "", err_code, pstr_out_cfg);

  pv_ia_process_api_obj = pstr_out_cfg->pv_ia_process_api_obj;

  pb_inp_buf = (pWORD8)pstr_out_cfg->mem_info_table[IA_MEMTYPE_INPUT].mem_ptr;
  pb_out_buf = (pWORD8)pstr_out_cfg->mem_info_table[IA_MEMTYPE_OUTPUT].mem_ptr;

  ixheaace_print_config_params(pstr_in_cfg, pstr_in_cfg_user);
  start_offset_samples = 0;
  input_size = pstr_out_cfg->input_size;

  if (input_size) {
    expected_frame_count = (pstr_in_cfg->aac_config.length + (input_size - 1)) / input_size;
  }

  if (NULL == ia_stsz_size) {
    ia_stsz_size = (UWORD32 *)malloc_global((expected_frame_count + 2) * sizeof(*ia_stsz_size),
                                            BYTE_ALIGN_8);
    memset(ia_stsz_size, 0, (expected_frame_count + 2) * sizeof(*ia_stsz_size));
  }
  down_sampling_ratio = pstr_out_cfg->down_sampling_ratio;
  samp_freq = (WORD32)(pstr_out_cfg->samp_freq / down_sampling_ratio);

  { ia_enhaacplus_enc_fwrite(pb_out_buf, g_pf_out, 0); }

  if (pstr_in_cfg->i_use_es) {
    i_dec_len = pstr_out_cfg->i_out_bytes;
    ia_enhaacplus_enc_fwrite(pb_out_buf, g_pf_out, pstr_out_cfg->i_out_bytes);
    fflush(g_pf_out);
  }

  i_bytes_read = ia_enhaacplus_enc_fread((pVOID)pb_inp_buf, sizeof(WORD8), input_size, g_pf_inp);

  while (i_bytes_read) {
    /*****************************************************************************/
    /* Print frame number */
    /*****************************************************************************/
    frame_count++;
    fprintf(stdout, "Frames Processed [%d]\r", frame_count);
    fflush(stdout);

    if (i_bytes_read != input_size) {
      memset((pb_inp_buf + i_bytes_read), 0, (input_size - i_bytes_read));
    }

    /*****************************************************************************/
    /* Perform Encoding of frame data */
    /*****************************************************************************/

    err_code = ixheaace_process(pv_ia_process_api_obj, (pVOID)pstr_in_cfg, (pVOID)pstr_out_cfg);

    _IA_HANDLE_ERROR(p_proc_err_info, (pWORD8) "", err_code, pstr_out_cfg);

    /* Get the output bytes */
    i_out_bytes = pstr_out_cfg->i_out_bytes;

    if (max_frame_size < i_out_bytes) max_frame_size = i_out_bytes;

    if (pstr_in_cfg->i_use_es || !(pstr_in_cfg->i_use_adts)) {
      if (i_out_bytes)
        ia_stsz_size[frame_count - 1] = i_out_bytes;
      else
        frame_count--;
    }

    ia_enhaacplus_enc_fwrite(pb_out_buf, g_pf_out, i_out_bytes);
    fflush(g_pf_out);

    i_bytes_read =
        ia_enhaacplus_enc_fread((pVOID)pb_inp_buf, sizeof(WORD8), input_size, g_pf_inp);

    if (frame_count == expected_frame_count) break;
  }

  fprintf(stdout, "\n");
  fflush(stdout);

  // Error handler is not invoked here to avoid invoking ixheaace_delete() twice.
  err_code = ixheaace_delete((pVOID)pstr_out_cfg);
  if ((err_code)&IA_FATAL_ERROR) {
    if (pstr_in_cfg_user) {
      free_global(pstr_in_cfg_user);
    }
    if (pstr_enc_api) {
      free_global(pstr_enc_api);
    }
    if (ia_stsz_size != NULL) {
      free_global(ia_stsz_size);
    }
    return (err_code);
  }

  if ((pstr_in_cfg->i_use_es) && (g_pf_meta)) {
    fprintf(g_pf_meta, "-dec_info_init:%d\n", i_dec_len);
    fprintf(g_pf_meta, "-g_track_count:%d\n", 1);
    fprintf(g_pf_meta, "-ia_mp4_stsz_entries:%d\n", frame_count);
    fprintf(g_pf_meta, "-movie_time_scale:%d\n", samp_freq);
    fprintf(g_pf_meta, "-media_time_scale:%d\n", samp_freq);
    fprintf(g_pf_meta, "-playTimeInSamples:%d\n",
            i_total_length / ((ui_pcm_wd_sz >> 3) * ui_num_chan));
    fprintf(g_pf_meta, "-startOffsetInSamples:%d\n-useEditlist:%d\n", start_offset_samples, 1);
    for (WORD32 i = 0; i < frame_count; i++)
      fprintf(g_pf_meta, "-ia_mp4_stsz_size:%d\n", ia_stsz_size[i]);
  }
  if (pstr_in_cfg_user) {
    free_global(pstr_in_cfg_user);
  }
  if (pstr_enc_api) {
    free_global(pstr_enc_api);
  }
  if (ia_stsz_size != NULL) {
    free_global(ia_stsz_size);
  }
  return IA_NO_ERROR;
}

int main(WORD32 argc, pCHAR8 argv[]) {
  FILE *param_file_id = NULL;

  WORD8 curr_cmd[IA_MAX_CMD_LINE_LENGTH];
  WORD32 fargc, curpos;
  WORD32 processcmd = 0;
  WORD8 fargv[IA_MAX_ARGS][IA_MAX_CMD_LINE_LENGTH];

  pWORD8 pargv[IA_MAX_ARGS];

  WORD8 pb_input_file_path[IA_MAX_CMD_LINE_LENGTH] = "";
  WORD8 pb_output_file_path[IA_MAX_CMD_LINE_LENGTH] = "";
  eld_ga_hdr = 1;
  ia_testbench_error_handler_init();

  ixheaace_version instance = {0};
  if (argc == 1 || argc == 2) {
    if (argc == 2 && (!strncmp((const char *)argv[1], "-paramfile:", 11))) {
      pWORD8 paramfile = (pWORD8)argv[1] + 11;

      param_file_id = fopen((const char *)paramfile, "r");
      if (param_file_id == NULL) {
        ixheaace_get_lib_id_strings(&instance);
        ia_enhaacplus_enc_display_id_message(instance.p_lib_name, instance.p_version_num);
        ia_enhaacplus_enc_print_usage();
        return IA_NO_ERROR;
      }
    } else {
      param_file_id = fopen(PARAMFILE, "r");
      if (param_file_id == NULL) {
        ixheaace_get_lib_id_strings(&instance);
        ia_enhaacplus_enc_display_id_message(instance.p_lib_name, instance.p_version_num);
        ia_enhaacplus_enc_print_usage();
        return IA_NO_ERROR;
      }
    }

    /* Process one line at a time */
    while (fgets((char *)curr_cmd, IA_MAX_CMD_LINE_LENGTH, param_file_id)) {
      WORD8 pb_meta_file_name[IA_MAX_CMD_LINE_LENGTH] = "";
      curpos = 0;
      fargc = 0;
      /* if it is not a param_file command and if */
      /* CLP processing is not enabled */
      if (curr_cmd[0] != '@' && !processcmd) { /* skip it */
        continue;
      }

      while (sscanf((char *)curr_cmd + curpos, "%s", fargv[fargc]) != EOF) {
        if (fargv[0][0] == '/' && fargv[0][1] == '/') break;
        if (strcmp((const char *)fargv[0], "@echo") == 0) break;
        if (strcmp((const char *)fargv[fargc], "@New_line") == 0) {
          if (NULL == fgets((char *)curr_cmd + curpos, IA_MAX_CMD_LINE_LENGTH, param_file_id))
            break;
          continue;
        }
        curpos += (WORD32)strlen((const char *)fargv[fargc]);
        while (*(curr_cmd + curpos) == ' ' || *(curr_cmd + curpos) == '\t') curpos++;
        fargc++;
      }

      if (fargc < 1) /* for blank lines etc. */
        continue;

      if (strcmp((const char *)fargv[0], "@Output_path") == 0) {
        if (fargc > 1)
          strcpy((char *)pb_output_file_path, (const char *)fargv[1]);
        else
          strcpy((char *)pb_output_file_path, "");
        continue;
      }

      if (strcmp((const char *)fargv[0], "@Input_path") == 0) {
        if (fargc > 1)
          strcpy((char *)pb_input_file_path, (const char *)fargv[1]);
        else
          strcpy((char *)pb_input_file_path, "");
        strcpy((char *)pb_drc_file_path, (const char *)pb_input_file_path);
        continue;
      }

      if (strcmp((const char *)fargv[0], "@Start") == 0) {
        processcmd = 1;
        continue;
      }

      if (strcmp((const char *)fargv[0], "@Stop") == 0) {
        processcmd = 0;
        continue;
      }

      /* otherwise if this a normal command and its enabled for execution */
      if (processcmd) {
        int i;
        int err_code = IA_NO_ERROR;
        WORD8 pb_input_file_name[IA_MAX_CMD_LINE_LENGTH] = "";
        WORD8 pb_output_file_name[IA_MAX_CMD_LINE_LENGTH] = "";
        WORD32 aot_value = 0;
        WORD32 is_ld_eld = 0;  // If set to 1, it denotes AOT 23 or AOT 39

        int file_count = 0;
        for (i = 0; i < fargc; i++) {
          printf("%s ", fargv[i]);
          pargv[i] = fargv[i];

          if (!strncmp((const char *)fargv[i], "-ifile:", 7)) {
            pWORD8 pb_arg_val = fargv[i] + 7;

            strcat((char *)pb_input_file_name, (const char *)pb_input_file_path);
            strcat((char *)pb_input_file_name, (const char *)pb_arg_val);

            g_pf_inp = NULL;
            g_pf_inp = fopen((const char *)pb_input_file_name, "rb");
            if (g_pf_inp == NULL) {
              err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
              ia_error_handler(&ia_testbench_error_info, (pWORD8) "Input File", err_code);
            }
            file_count++;
          }

          if (!strncmp((const char *)fargv[i], "-ofile:", 7)) {
            pWORD8 pb_arg_val = fargv[i] + 7;

            strcat((char *)pb_output_file_name, (const char *)pb_output_file_path);
            strcat((char *)pb_output_file_name, (const char *)pb_arg_val);

            g_pf_out = NULL;
            g_pf_out = fopen((const char *)pb_output_file_name, "wb");
            if (g_pf_out == NULL) {
              err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
              ia_error_handler(&ia_testbench_error_info, (pWORD8) "Output File", err_code);
            }
            file_count++;
          }

          if (!strncmp((const char *)fargv[i], "-aot:", 5)) {
            pWORD8 pb_arg_val = fargv[i] + 5;
            aot_value = atoi((const char *)(pb_arg_val));
            if (aot_value == 23 || aot_value == 39) {
              is_ld_eld = 1;
            }
          }
          if (!strncmp((const char *)fargv[i], "-adts:", 6)) {
            pWORD8 pb_arg_val = fargv[i] + 6;

            if ((atoi((const char *)pb_arg_val))) eld_ga_hdr = 0;
          }
        }
        g_w_malloc_count = 0;

        printf("\n");

        if (file_count != 2) {
          err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
          ia_error_handler(&ia_testbench_error_info, (pWORD8) "Input or Output File", err_code);
        }
        if (is_ld_eld) {
          eld_ga_hdr = 1;
        }

        if ((strcmp((const char *)pb_output_file_name, "")) && (eld_ga_hdr)) {
          char *file_name = strrchr((const char *)pb_output_file_name, '.');
          SIZE_T idx = file_name - (char *)pb_output_file_name;
          memcpy(pb_meta_file_name, pb_output_file_name, idx);
          strcat((char *)pb_meta_file_name, ".txt");
          g_pf_meta = NULL;
          g_pf_meta = fopen((const char *)pb_meta_file_name, "wt");
          if (g_pf_meta == NULL) {
            err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
            ia_error_handler(&ia_testbench_error_info, (pWORD8) "Meta File", err_code);
          }
        }
        if (err_code == IA_NO_ERROR) ia_enhaacplus_enc_main_process(fargc, pargv);

        eld_ga_hdr = 1;

        if (g_pf_inp) fclose(g_pf_inp);
        if (g_pf_out) fclose(g_pf_out);
        if (g_pf_meta != NULL) {
          fclose(g_pf_meta);
          g_pf_meta = NULL;
        }
      }
    }
  } else {
    int i;
    int err_code = IA_NO_ERROR;
    int file_count = 0;
    WORD32 aot_value = 0;
    WORD32 is_ld_eld = 0;  // If set to 1, it denotes AOT 23 or AOT 39

    WORD8 pb_input_file_name[IA_MAX_CMD_LINE_LENGTH] = "";
    WORD8 pb_output_file_name[IA_MAX_CMD_LINE_LENGTH] = "";
    WORD8 pb_meta_file_name[IA_MAX_CMD_LINE_LENGTH] = "";
    for (i = 1; i < argc; i++) {
      printf("%s ", argv[i]);

      if (!strncmp((const char *)argv[i], "-ifile:", 7)) {
        pWORD8 pb_arg_val = (pWORD8)argv[i] + 7;
        strcat((char *)pb_input_file_name, (const char *)pb_input_file_path);
        strcat((char *)pb_input_file_name, (const char *)pb_arg_val);

        g_pf_inp = NULL;
        g_pf_inp = fopen((const char *)pb_input_file_name, "rb");
        if (g_pf_inp == NULL) {
          err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
          ia_error_handler(&ia_testbench_error_info, (pWORD8) "Input File", err_code);
        }
        file_count++;
      }

      if (!strncmp((const char *)argv[i], "-ofile:", 7)) {
        pWORD8 pb_arg_val = (pWORD8)argv[i] + 7;

        strcat((char *)pb_output_file_name, (const char *)pb_output_file_path);
        strcat((char *)pb_output_file_name, (const char *)pb_arg_val);

        g_pf_out = NULL;
        g_pf_out = fopen((const char *)pb_output_file_name, "wb");
        if (g_pf_out == NULL) {
          err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
          ia_error_handler(&ia_testbench_error_info, (pWORD8) "Output File", err_code);
        }
        file_count++;
      }

      if (!strncmp((const char *)argv[i], "-aot:", 5)) {
        pCHAR8 pb_arg_val = argv[i] + 5;
        aot_value = atoi((const char *)(pb_arg_val));
        if (aot_value == 23 || aot_value == 39) {
          is_ld_eld = 1;
        }
      }

      if (!strncmp((const char *)argv[i], "-adts:", 6)) {
        pCHAR8 pb_arg_val = argv[i] + 6;
        if (atoi((const char *)pb_arg_val)) eld_ga_hdr = 0;
      }

      if (!strncmp((const char *)argv[i], "-help", 5)) {
        ia_enhaacplus_enc_print_usage();
      }
    }
    g_w_malloc_count = 0;

    printf("\n");
    if (file_count != 2) {
      err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
      ia_error_handler(&ia_testbench_error_info, (pWORD8) "Input or Output File", err_code);
    }
    if (is_ld_eld) {
      eld_ga_hdr = 1;
    }
#ifdef _WIN32
#pragma warning(suppress : 6001)
#endif
    if ((strcmp((const char *)pb_output_file_name, "")) && (eld_ga_hdr)) {
      char *file_name = strrchr((const char *)pb_output_file_name, '.');
      SIZE_T idx = file_name - (char *)pb_output_file_name;
      memcpy(pb_meta_file_name, pb_output_file_name, idx);
      strcat((char *)pb_meta_file_name, ".txt");
      g_pf_meta = NULL;
      g_pf_meta = fopen((const char *)pb_meta_file_name, "wt");
      if (g_pf_meta == NULL) {
        err_code = IA_TESTBENCH_MFMAN_FATAL_FILE_OPEN_FAILED;
        ia_error_handler(&ia_testbench_error_info, (pWORD8) "Meta File", err_code);
      }
    }
    if (err_code == IA_NO_ERROR) ia_enhaacplus_enc_main_process(argc - 1, (pWORD8 *)&argv[1]);

    eld_ga_hdr = 1;
    if (g_pf_inp) fclose(g_pf_inp);
    if (g_pf_out) fclose(g_pf_out);
    if (g_pf_meta != NULL) {
      fclose(g_pf_meta);
      g_pf_meta = NULL;
    }
  }
  if (param_file_id != NULL) {
    fclose(param_file_id);
  }

  return IA_NO_ERROR;
}
