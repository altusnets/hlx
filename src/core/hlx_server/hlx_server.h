//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_server.h
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
#ifndef _HLX_SERVER_H
#define _HLX_SERVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "http_parser.h"
#include "http_request.h"

#include <pthread.h>
#include <signal.h>

#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_SERVER_STATUS_OK 0
#define HLX_SERVER_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define HLX_SERVER_DISALLOW_ASSIGN(class_name)\
    class_name& operator=(const class_name &);
#define HLX_SERVER_DISALLOW_COPY(class_name)\
    class_name(const class_name &);
#define HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(class_name)\
    HLX_SERVER_DISALLOW_COPY(class_name)\
    HLX_SERVER_DISALLOW_ASSIGN(class_name)

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------
class http_request;

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: hlx_server
//: ----------------------------------------------------------------------------
class hlx_server
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        ~hlx_server();

        // Settings...
        void set_verbose(bool a_val) { m_verbose = a_val;}
        void set_color(bool a_val) { m_color = a_val;}
        void set_stats(bool a_val) { m_stats = a_val;}
        void set_port(uint16_t a_port) {m_port = a_port;}

        // Running...
        int32_t run(void);
        void *t_run(void *a_nothing);

        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void) { return !m_stopped;};

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
        // Get the singleton instance
        static hlx_server *get(void);

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

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(hlx_server)
        hlx_server();

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<hlx_server *>(a_context)->t_run(NULL);
        }

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

        // -------------------------------------------------
        // Settings
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        bool m_stats;

        uint16_t m_port;

        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;

        // -------------------------------------------------
        // State
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        int32_t m_server_fd;
        bool m_cur_msg_complete;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance
        static hlx_server *m_singleton_ptr;
};


} //namespace ns_hlx {

#endif // #ifndef _HLX_SERVER_H



