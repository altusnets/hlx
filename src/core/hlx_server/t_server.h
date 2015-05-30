//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_server.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#ifndef _T_SERVER_H
#define _T_SERVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx_server.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"
#include "ndebug.h"
#include "evr.h"

// signal
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------


namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;

        // run options
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;

        // fd
        int m_server_fd;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_num_parallel(64),

                m_server_fd(-1)
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;

//: ----------------------------------------------------------------------------
//: t_server
//: ----------------------------------------------------------------------------
class t_server
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_server(const settings_struct_t &a_settings);
        ~t_server();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        int accept_tcp_connection(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        settings_struct_t m_settings;

        // -----------------------------
        // Summary info
        // -----------------------------

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        std::string m_next_header;
        std::string m_body;
        http_request m_request;

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int hp_on_message_begin(http_parser* a_parser);
        static int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_headers_complete(http_parser* a_parser);
        static int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_message_complete(http_parser* a_parser);

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static void *evr_loop_file_writeable_cb(void *a_data);
        static void *evr_loop_file_readable_cb(void *a_data);
        static void *evr_loop_file_error_cb(void *a_data);
        static void *evr_loop_file_timeout_cb(void *a_data);
        static void *evr_loop_timer_cb(void *a_data);
        static void *evr_loop_timer_completion_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_server)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_server *>(a_context)->t_run(NULL);
        }

        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true, int32_t a_status = 0);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config
        sig_atomic_t m_stopped;

        int32_t m_start_time_s;

        // Get evr_loop
        evr_loop *m_evr_loop;


        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;

        // -------------------------------------------------
        // State
        // -------------------------------------------------
        nconn::scheme_t m_scheme;
        bool m_cur_msg_complete;

};


} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H


