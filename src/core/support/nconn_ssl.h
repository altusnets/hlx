//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_ssl.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------
#ifndef _NCONN_SSL_H
#define _NCONN_SSL_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn_tcp.h"
#include "ndebug.h"
#include <openssl/ssl.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn_ssl: public nconn_tcp
{
public:
        // ---------------------------------------
        // Options
        // ---------------------------------------
        typedef enum ssl_opt_enum
        {
                OPT_SSL_GET_REQ_BUF = 0,
                OPT_SSL_GET_GLOBAL_REQ_BUF
        } ssl_opt_t;


        nconn_ssl(bool a_verbose,
                  bool a_color,
                  int64_t a_max_reqs_per_conn = -1,
                  bool a_save_response_in_reqlet = false,
                  bool a_collect_stats = false,
                  void *a_rand_ptr = NULL):
                          nconn_tcp(a_verbose, a_color, a_max_reqs_per_conn, a_save_response_in_reqlet, a_collect_stats, a_rand_ptr),
                          m_ssl_ctx(NULL),
                          m_ssl(NULL),
                          m_ssl_state(SSL_STATE_FREE)
          {

          };

        // Destructor
        ~nconn_ssl()
        {
        };

        int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info);
        int32_t send_request(bool is_reuse = false);
        int32_t cleanup(evr_loop *a_evr_loop);
        int32_t set_opt(uint32_t a_opt, void *a_buf, uint32_t a_len);
        int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len);

        bool is_done(void) { return (m_ssl_state == SSL_STATE_DONE);}
        void set_state_done(void) { m_ssl_state = SSL_STATE_DONE; };

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static const scheme_t m_scheme = SCHEME_SSL;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:

        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum ssl_state
        {
                SSL_STATE_FREE = 0,
                SSL_STATE_CONNECTING,

                // SSL
                SSL_STATE_SSL_CONNECTING,
                SSL_STATE_SSL_CONNECTING_WANT_READ,
                SSL_STATE_SSL_CONNECTING_WANT_WRITE,

                SSL_STATE_CONNECTED,
                SSL_STATE_READING,
                SSL_STATE_DONE
        } ssl_state_t;

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_ssl)

        int32_t ssl_connect(const host_info_t &a_host_info);
        int32_t receive_response(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // ssl
        SSL_CTX * m_ssl_ctx;
        SSL *m_ssl;

        ssl_state_t m_ssl_state;

};



#endif



