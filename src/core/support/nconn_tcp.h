//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_tcp.h
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
#ifndef _NCONN_TCP_H
#define _NCONN_TCP_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn.h"
#include "ndebug.h"

#if 0
#include "http_cb.h"
#include "host_info.h"
#include "req_stat.h"
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#if 0
#define MAX_READ_BUF (16*1024)
#define MAX_REQ_BUF (2048)
#endif


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
#if 0
class evr_loop;
class parsed_url;
#endif

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn_tcp: public nconn
{
public:
        // ---------------------------------------
        // Options
        // ---------------------------------------
        typedef enum tcp_opt_enum
        {
                OPT_TCP_GET_REQ_BUF = 0,
                OPT_TCP_GET_REQ_BUF_LEN,
                OPT_TCP_GET_GLOBAL_REQ_BUF,
                OPT_TCP_GET_GLOBAL_REQ_BUF_LEN
        } tcp_opt_t;

#if 0
        void set_host(const std::string &a_host) {m_host = a_host;};
        int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info);
        int32_t send_request(bool is_reuse = false);
        int32_t cleanup(evr_loop *a_evr_loop);
        bool can_reuse(void)
        {
                //NDBG_PRINT("CONN[%u] num / max %ld / %ld \n", m_connection_id, m_num_reqs, m_max_reqs_per_conn);
                if(m_server_response_supports_keep_alives &&
                   ((m_max_reqs_per_conn == -1) || (m_num_reqs < m_max_reqs_per_conn)))
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_ssl_ctx = a_ssl_ctx;};
        void reset_stats(void);
        const req_stat_t &get_stats(void) const { return m_stat;};
        void set_scheme(scheme_t a_scheme) {m_scheme = a_scheme;};
        bool is_done(void) { return (m_state == CONN_STATE_DONE);}
        void set_id(uint64_t a_id) {m_id = a_id;}
        uint64_t get_id(void) {return m_id;}
        void set_data1(void * a_data) {m_data1 = a_data;}
        void *get_data1(void) {return m_data1;}
#endif

        // Destructor
        ~nconn_tcp()
        {
        };


        // TODO FINISH!!!
        void set_state_done(void) { /*m_state = CONN_STATE_DONE;*/ };

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static const scheme_t m_scheme = SCHEME_TCP;
        static const uint32_t m_max_req_buf = 2048;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:

        // ---------------------------------------
        // Connection state
        // ---------------------------------------
#if 0
        typedef enum conn_state
        {
                CONN_STATE_FREE = 0,
                CONN_STATE_CONNECTING,

                // SSL
                CONN_STATE_SSL_CONNECTING,
                CONN_STATE_SSL_CONNECTING_WANT_READ,
                CONN_STATE_SSL_CONNECTING_WANT_WRITE,

                CONN_STATE_CONNECTED,
                CONN_STATE_READING,
                CONN_STATE_DONE
        } conn_state_t;
#endif

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_tcp)

#if 0
        int32_t setup_socket(const host_info_t &a_host_info);
        int32_t ssl_connect(const host_info_t &a_host_info);
        int32_t receive_response(void);
#endif

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        char m_req_buf[m_max_req_buf];
        uint32_t m_req_buf_len;

#if 0
        int m_fd;

        // ssl
        SSL_CTX * m_ssl_ctx;
        SSL *m_ssl;

        conn_state_t m_state;
        req_stat_t m_stat;
        uint64_t m_id;
        void *m_data1;
        bool m_save_response_in_reqlet;

        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;
        bool m_server_response_supports_keep_alives;
#endif

#if 0
        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        char m_read_buf[MAX_READ_BUF];
        uint32_t m_read_buf_idx;

        int64_t m_max_reqs_per_conn;
        int64_t m_num_reqs;

        uint64_t m_connect_start_time_us;
        uint64_t m_request_start_time_us;
        uint64_t m_last_connect_time_us;

        scheme_t m_scheme;
        std::string m_host;
        bool m_collect_stats_flag;
        uint32_t m_timeout_s;
#endif

};



#endif



