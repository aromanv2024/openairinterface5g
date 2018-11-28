/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*!\file nrLDPC_decoder.c
 * \brief Defines the LDPC decoder
 * \author Sebastian Wagner (TCL Communications) Email: <mailto:sebastian.wagner@tcl.com>
 * \date 27-03-2018
 * \version 1.0
 * \note
 * \warning
 */


#include <stdint.h>
#include <immintrin.h>
#include "nrLDPC_defs.h"
#include "nrLDPC_types.h"
#include "nrLDPC_init.h"
#include "nrLDPC_mPass.h"
#include "nrLDPC_cnProc.h"
#include "nrLDPC_bnProc.h"

#define NR_LDPC_ENABLE_PARITY_CHECK
//#define NR_LDPC_PROFILER_DETAIL

#ifdef NR_LDPC_DEBUG_MODE
#include "nrLDPC_tools/nrLDPC_debug.h"
#endif

static inline uint32_t nrLDPC_decoder_core(int8_t* p_llr, int8_t* p_out, uint32_t numLLR, t_nrLDPC_lut* p_lut, t_nrLDPC_dec_params* p_decParams, t_nrLDPC_time_stats* p_profiler);

int32_t nrLDPC_decoder(t_nrLDPC_dec_params* p_decParams, int8_t* p_llr, int8_t* p_out, t_nrLDPC_time_stats* p_profiler)
{
    uint32_t numLLR;
    uint32_t numIter = 0;
    t_nrLDPC_lut lut;
    t_nrLDPC_lut* p_lut = &lut;

    // Initialize decoder core(s) with correct LUTs
    numLLR = nrLDPC_init(p_decParams, p_lut);

    // Launch LDPC decoder core for one segment
    numIter = nrLDPC_decoder_core(p_llr, p_out, numLLR, p_lut, p_decParams, p_profiler);

    return numIter;
}

/**
   \brief Performs LDPC decoding of one code block
   \param p_llr Input LLRs
   \param p_out Output vector
   \param numLLR Number of LLRs
   \param p_lut Pointer to decoder LUTs
   \param p_decParams LDPC decoder parameters
   \param p_profiler LDPC profiler statistics
*/
static inline uint32_t nrLDPC_decoder_core(int8_t* p_llr, int8_t* p_out, uint32_t numLLR, t_nrLDPC_lut* p_lut, t_nrLDPC_dec_params* p_decParams, t_nrLDPC_time_stats* p_profiler)
{
    uint16_t Z          = p_decParams->Z;
    uint8_t  BG         = p_decParams->BG;
    uint8_t  numMaxIter = p_decParams->numMaxIter;
    e_nrLDPC_outMode outMode = p_decParams->outMode;

    // Minimum number of iterations is 1
    // 0 iterations means hard-decision on input LLRs
    uint32_t i = 1;
    // Initialize with parity check fail != 0
    int32_t pcRes = 1;
    int8_t* p_llrOut;

    if (outMode == nrLDPC_outMode_LLRINT8)
    {
        p_llrOut = p_out;
    }
    else
    {
        // Use LLR processing buffer as temporary output buffer
        p_llrOut = (int8_t*) llrProcBuf;
    }


    // Initialization
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->llr2llrProcBuf);
#endif
    nrLDPC_llr2llrProcBuf(p_lut, p_llr, Z, BG);
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->llr2llrProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_LLR_PROC);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_PROC);
#endif

#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->llr2CnProcBuf);
#endif
    nrLDPC_llr2CnProcBuf(p_lut, p_llr, numLLR, Z, BG);
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->llr2CnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_CN_PROC);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC);
#endif

    // First iteration

    // CN processing
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->cnProc);
#endif
    if (BG == 1)
    {
        nrLDPC_cnProc_BG1(p_lut, Z);
    }
    else
    {
        nrLDPC_cnProc_BG2(p_lut, Z);
    }
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->cnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_CN_PROC_RES);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC_RES);
#endif

#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->cn2bnProcBuf);
#endif
    if (BG == 1)
    {
        nrLDPC_cn2bnProcBuf_BG1(p_lut, Z);
    }
    else
    {
        nrLDPC_cn2bnProcBuf(p_lut, Z);
    }
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->cn2bnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_BN_PROC);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC);
#endif

    // BN processing
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->bnProcPc);
#endif
    nrLDPC_bnProcPc(p_lut, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->bnProcPc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_LLR_RES);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_RES);
#endif

#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->bnProc);
#endif
    nrLDPC_bnProc(p_lut, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->bnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_BN_PROC_RES);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC_RES);
#endif

    // BN results to CN processing buffer
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->bn2cnProcBuf);
#endif
    if (BG == 1)
    {
        nrLDPC_bn2cnProcBuf_BG1(p_lut, Z);
    }
    else
    {
        nrLDPC_bn2cnProcBuf(p_lut, Z);
    }
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->bn2cnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC);
#endif

    // Parity Check not necessary here since it will fail
    // because first 2 cols/BNs in BG are punctured and cannot be
    // estimated after only one iteration

    // First iteration finished

    while ( (i < (numMaxIter-1)) && (pcRes != 0) )
    {
        // Increase iteration counter
        i++;

        // CN processing
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cnProc);
#endif
        if (BG == 1)
        {
            nrLDPC_cnProc_BG1(p_lut, Z);
        }
        else
        {
            nrLDPC_cnProc_BG2(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC_RES);
#endif

        // Send CN results back to BNs
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cn2bnProcBuf);
#endif
        if (BG == 1)
        {
            nrLDPC_cn2bnProcBuf_BG1(p_lut, Z);
        }
        else
        {
            nrLDPC_cn2bnProcBuf(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cn2bnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC);
#endif

        // BN Processing
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->bnProcPc);
#endif
        nrLDPC_bnProcPc(p_lut, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->bnProcPc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_RES);
#endif

#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->bnProc);
#endif
        nrLDPC_bnProc(p_lut, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->bnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC_RES);
#endif

        // BN results to CN processing buffer
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->bn2cnProcBuf);
#endif
        if (BG == 1)
        {
            nrLDPC_bn2cnProcBuf_BG1(p_lut, Z);
        }
        else
        {
            nrLDPC_bn2cnProcBuf(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->bn2cnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC);
#endif

        // Parity Check
#ifdef NR_LDPC_ENABLE_PARITY_CHECK
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cnProcPc);
#endif
        if (BG == 1)
        {
            pcRes = nrLDPC_cnProcPc_BG1(p_lut, Z);
        }
        else
        {
            pcRes = nrLDPC_cnProcPc_BG2(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cnProcPc);
#endif
#endif

    }

    // Last iteration
    if ( (i < numMaxIter) && (pcRes != 0) )
    {
        // Increase iteration counter
        i++;

        // CN processing
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cnProc);
#endif
        if (BG == 1)
        {
            nrLDPC_cnProc_BG1(p_lut, Z);
        }
        else
        {
            nrLDPC_cnProc_BG2(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC_RES);
#endif

        // Send CN results back to BNs
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cn2bnProcBuf);
#endif
        if (BG == 1)
        {
            nrLDPC_cn2bnProcBuf_BG1(p_lut, Z);
        }
        else
        {
            nrLDPC_cn2bnProcBuf(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cn2bnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC);
#endif

        // BN Processing
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->bnProcPc);
#endif
        nrLDPC_bnProcPc(p_lut, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->bnProcPc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_RES);
#endif

        // If parity check not enabled, no need to send the BN proc results
        // back to CNs
#ifdef NR_LDPC_ENABLE_PARITY_CHECK
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->bnProc);
#endif
        nrLDPC_bnProc(p_lut, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->bnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC_RES);
#endif

        // BN results to CN processing buffer
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->bn2cnProcBuf);
#endif
        if (BG == 1)
        {
            nrLDPC_bn2cnProcBuf_BG1(p_lut, Z);
        }
        else
        {
            nrLDPC_bn2cnProcBuf(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->bn2cnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC);
#endif

        // Parity Check
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cnProcPc);
#endif
        if (BG == 1)
        {
            pcRes = nrLDPC_cnProcPc_BG1(p_lut, Z);
        }
        else
        {
            pcRes = nrLDPC_cnProcPc_BG2(p_lut, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cnProcPc);
#endif
#endif
    }


    // If maximum number of iterations reached an PC still fails increase number of iterations
    // Thus, i > numMaxIter indicates that PC has failed
    if (pcRes != 0)
    {
        i++;
    }

    // Assign results from processing buffer to output
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->llrRes2llrOut);
#endif
    nrLDPC_llrRes2llrOut(p_lut, p_llrOut, numLLR);
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->llrRes2llrOut);
#endif

    // Hard-decision
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->llr2bit);
#endif
    if (outMode == nrLDPC_outMode_BIT)
    {
        nrLDPC_llr2bitPacked(p_out, p_llrOut, numLLR);
    }
    else if (outMode == nrLDPC_outMode_BITINT8)
    {
        nrLDPC_llr2bit(p_out, p_llrOut, numLLR);
    }

#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->llr2bit);
#endif

    return i;
}