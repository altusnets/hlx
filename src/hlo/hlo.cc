//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlo.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "hlo.h"
#include "util.h"
#include "ssl_util.h"
#include "tinymt64.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"

#include <unistd.h>
#include <signal.h>

// inet_aton
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <unordered_set>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;
typedef std::map <uint64_t, reqlet *> reqlet_map_t;
typedef std::vector <reqlet *> reqlet_list_t;
typedef std::map <std::string, std::string> header_map_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class t_client
{
public:


        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_client(
                bool a_verbose,
                bool a_color,
                uint32_t a_sock_opt_recv_buf_size,
                uint32_t a_sock_opt_send_buf_size,
                bool a_sock_opt_no_delay,
                const std::string & a_cipher_str,
                SSL_CTX *a_ctx,
                evr_type_t a_evr_type,
                uint32_t a_max_parallel_connections,
                int32_t a_run_time_s,
                int32_t a_timeout_s,
                int64_t a_max_reqs_per_conn = -1,
                const std::string &a_url = "",
                const std::string &a_url_file = "",
                bool a_wildcarding = true
                );

        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);
        void get_stats_copy(tag_stat_map_t &ao_tag_stat_map);
        int32_t add_url(std::string &a_url);
        int32_t add_url_file(std::string &a_url_file);
        uint32_t get_timeout_s(void) { return m_timeout_s;};

        bool is_done(void) const
        {
                return (m_num_fetched == m_num_fetches);
        }

        bool has_available_fetches(void) const
        {
                return ((m_num_fetches == -1) || (m_num_pending < m_num_fetches));
        }

        reqlet *reqlet_take(void);
        bool reqlet_give_and_can_reuse_conn(reqlet *a_reqlet);

        void set_rate(int32_t a_rate)
        {
                if(a_rate != -1)
                {
                        m_rate_limit = true;
                        m_rate_delta_us = 1000000 / a_rate;
                }
                else
                {
                        m_rate_limit = false;
                }
        }

        void limit_rate()
        {
                if(m_rate_limit)
                {
                        uint64_t l_cur_time_us = get_time_us();
                        if((l_cur_time_us - m_last_get_req_us) < m_rate_delta_us)
                        {
                                usleep(m_rate_delta_us - (l_cur_time_us - m_last_get_req_us));
                        }
                        m_last_get_req_us = get_time_us();
                }
        }

        void set_end_fetches(int64_t a_num_fetches) { m_num_fetches = a_num_fetches;}
        void set_reqlet_mode(reqlet_mode_t a_mode) {m_reqlet_mode = a_mode;}
        int64_t get_num_fetches(void) {return m_num_fetches;}
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_ssl_ctx = a_ssl_ctx;}

#if 0
        //int32_t start_async_resolver();
        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
        //static int32_t resolve_cb(void* a_cb_obj);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
#endif

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        int32_t m_timeout_s;

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static void *evr_loop_file_writeable_cb(void *a_data);
        static void *evr_loop_file_readable_cb(void *a_data);
        static void *evr_loop_file_error_cb(void *a_data);
        static void *evr_loop_file_timeout_cb(void *a_data);
        static void *evr_loop_timer_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_client)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_client *>(a_context)->t_run(NULL);
        }

        int32_t start_connections(void);
        int32_t add_avail(reqlet *a_reqlet);
        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true);
        int32_t create_request(nconn &ao_conn, reqlet &a_reqlet);
        nconn *create_new_nconn(uint32_t a_id, const reqlet &a_reqlet);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config
        bool m_verbose;
        bool m_color;

        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL support
        std::string m_cipher_str;
        SSL_CTX *m_ssl_ctx;

        evr_type_t m_evr_type;

        sig_atomic_t m_stopped;

        uint32_t m_max_parallel_connections;
        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_free_list;
        conn_id_set_t m_conn_used_set;

        int64_t m_max_reqs_per_conn;
        int32_t m_run_time_s;
        int32_t m_start_time_s;

        std::string m_url;
        std::string m_url_file;
        bool m_wilcarding;

        reqlet_map_t m_reqlet_map;

        int64_t m_num_fetches;
        int64_t m_num_fetched;
        int64_t m_num_pending;

        bool m_rate_limit;

        uint64_t m_rate_delta_us;

        reqlet_mode_t m_reqlet_mode;
        reqlet_list_t m_reqlet_avail_list;

        uint32_t m_last_reqlet_index;
        uint64_t m_last_get_req_us;

        void *m_rand_ptr;

        header_map_t m_header_map;

        //uint64_t m_unresolved_count;

        // Get evr_loop
        evr_loop *m_evr_loop;

};

//: ----------------------------------------------------------------------------
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_client *g_t_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::t_client(bool a_verbose,
                bool a_color,
                uint32_t a_sock_opt_recv_buf_size,
                uint32_t a_sock_opt_send_buf_size,
                bool a_sock_opt_no_delay,
                const std::string & a_cipher_str,
                SSL_CTX *a_ssl_ctx,
                evr_type_t a_evr_type,
                uint32_t a_max_parallel_connections,
                int32_t a_run_time_s,
                int32_t a_timeout_s,
                int64_t a_max_reqs_per_conn,
                const std::string &a_url,
                const std::string &a_url_file,
                bool a_wildcarding):

                        m_t_run_thread(),
                        m_timeout_s(a_timeout_s),
                        m_verbose(a_verbose),
                        m_color(a_color),
                        m_sock_opt_recv_buf_size(a_sock_opt_recv_buf_size),
                        m_sock_opt_send_buf_size(a_sock_opt_send_buf_size),
                        m_sock_opt_no_delay(a_sock_opt_no_delay),
                        m_cipher_str(a_cipher_str),
                        m_ssl_ctx(a_ssl_ctx),
                        m_evr_type(a_evr_type),
                        m_stopped(false),
                        m_max_parallel_connections(a_max_parallel_connections),
                        m_nconn_vector(a_max_parallel_connections),
                        m_conn_free_list(),
                        m_conn_used_set(),
                        m_max_reqs_per_conn(a_max_reqs_per_conn),
                        m_run_time_s(a_run_time_s),
                        m_start_time_s(0),
                        m_url(a_url),
                        m_url_file(a_url_file),
                        m_wilcarding(a_wildcarding),
                        m_reqlet_map(),
                        m_num_fetches(-1),
                        m_num_fetched(0),
                        m_num_pending(0),
                        m_rate_limit(false),
                        m_rate_delta_us(0),
                        m_reqlet_mode(REQLET_MODE_ROUND_ROBIN),
                        m_reqlet_avail_list(),
                        m_last_reqlet_index(0),
                        m_last_get_req_us(0),
                        m_rand_ptr(NULL),
                        m_header_map(),
                        m_evr_loop(NULL)
{

        // Initialize rand...
        m_rand_ptr = malloc(sizeof(tinymt64_t));
        tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
        tinymt64_init(l_rand_ptr, get_time_us());

        for(uint32_t i_conn = 0; i_conn < a_max_parallel_connections; ++i_conn)
        {
                m_nconn_vector[i_conn] = NULL;
                m_conn_free_list.push_back(i_conn);
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::~t_client()
{
        for(uint32_t i_conn = 0; i_conn < m_nconn_vector.size(); ++i_conn)
        {
                if(m_nconn_vector[i_conn])
                {
                        delete m_nconn_vector[i_conn];
                        m_nconn_vector[i_conn] = NULL;
                }
        }

        if(m_evr_loop)
        {
                delete m_evr_loop;
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int t_client::run(void)
{

        int32_t l_pthread_error = 0;

        l_pthread_error = pthread_create(&m_t_run_thread,
                        NULL,
                        t_run_static,
                        this);
        if (l_pthread_error != 0)
        {
                // failed to create thread

                NDBG_PRINT("Error: creating thread.  Reason: %s\n.", strerror(l_pthread_error));
                return STATUS_ERROR;

        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::stop(void)
{
        m_stopped = true;
        int32_t l_status;
        if(m_evr_loop)
        {
                l_status = m_evr_loop->stop();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error performing stop.\n");
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_writeable_cb(void *a_data)
{

        if(!a_data)
        {
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        if (false == l_t_client->has_available_fetches())
                return NULL;

        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn);

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                // TODO FIX!!!
                //T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Error performing connect_cb");
                l_t_client->cleanup_connection(l_nconn);
                return NULL;
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);

        if(!a_data)
        {
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn);

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                // TODO FIX!!!
                //T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Error performing connect_cb");
                l_t_client->cleanup_connection(l_nconn);
                return NULL;
        }

        if(l_status >= 0)
        {
                l_reqlet->m_stat_agg.m_num_bytes_read += l_status;
        }

        // Check for done...
        if((l_nconn->is_done()) ||
           (l_status == STATUS_ERROR))
        {
                // Add stats
                add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
                l_nconn->reset_stats();

                l_reqlet->m_stat_agg.m_num_conn_completed++;
                l_t_client->m_num_fetched++;

                // Bump stats
                if(l_status == STATUS_ERROR)
                {
                        ++(l_reqlet->m_stat_agg.m_num_errors);
                }

                // Give back reqlet
                bool l_can_reuse = false;
                l_can_reuse = (l_nconn->can_reuse() && l_t_client->reqlet_give_and_can_reuse_conn(l_reqlet));

                //NDBG_PRINT("CONN %sREUSE%s: %d -- l_nconn->can_reuse(): %d  --l_t_client->reqlet_give_and_can_reuse_conn(l_reqlet): %d\n",
                //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //              l_can_reuse,
                //              l_nconn->can_reuse(),
                //              l_t_client->reqlet_give_and_can_reuse_conn(l_reqlet)
                //              );

                if(l_can_reuse)
                {
                        // Send request again...
                        l_t_client->create_request(*l_nconn, *l_reqlet);
                        l_nconn->send_request(true);
                        l_t_client->m_num_pending++;
                }
                // You complete me...
                else
                {
                        //NDBG_PRINT("DONE: l_reqlet: %sHOST%s: %d / %d\n",
                        //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        //              l_can_reuse,
                        //              l_nconn->can_reuse());

                        l_t_client->cleanup_connection(l_nconn, false);
                        return NULL;
                }
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sSTATUS_ERRORS%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_timeout_cb(void *a_data)
{

        if(!a_data)
        {
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;
        uint64_t l_connection_id = l_nconn->get_id();

        //printf("%sT_O%s: %p\n",ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn);

        // Add stats
        add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
        l_nconn->reset_stats();

        if(l_t_client->m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_connection_id,
                                l_reqlet->m_url.m_host.c_str());
        }

        // Stats
        l_t_client->m_num_fetched++;
        l_reqlet->m_stat_agg.m_num_conn_completed++;
        ++(l_reqlet->m_stat_agg.m_num_idle_killed);

        // Connections
        l_t_client->cleanup_connection(l_nconn);

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_timer_cb(void *a_data)
{
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::t_run(void *a_nothing)
{

        // Set thread local
        g_t_client = this;
        int32_t l_status;

        // Add the urls
        if(!m_url.empty())
        {
                l_status = add_url(m_url);
                if(l_status != STATUS_OK)
                {
                        m_stopped = true;
                        return NULL;
                }
        }
        if(!m_url_file.empty())
        {
                l_status = add_url_file(m_url_file);
                if(l_status != STATUS_OK)
                {
                        m_stopped = true;
                        return NULL;
                }
        }

        // Set start time
        m_start_time_s = get_time_s();

        evr_loop_type_t l_evr_loop_type = EVR_LOOP_EPOLL;
        // -------------------------------------------
        // Get the event handler...
        // -------------------------------------------
        if (m_evr_type == EV_SELECT)
        {
                l_evr_loop_type = EVR_LOOP_SELECT;
                //NDBG_PRINT("Using evr_select\n");
        }
        // Default to epoll
        else
        {
                l_evr_loop_type = EVR_LOOP_EPOLL;

                //NDBG_PRINT("Using evr_epoll\n");
        }

        // Create loop
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                        evr_loop_file_writeable_cb,
                        evr_loop_file_error_cb,
                        l_evr_loop_type,
                        m_max_parallel_connections);

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop: m_run_time_s: %d start_time_s: %" PRIu64 "\n", m_run_time_s, get_time_s() - m_start_time_s);
        while(!m_stopped &&
                        //has_available_fetches() &&
                        !is_done() &&
                        ((m_run_time_s == -1) || (m_run_time_s > (int32_t)(get_time_s() - m_start_time_s))))
        {

                //NDBG_PRINT("TIME: %d\n", (int32_t)get_time_s() - m_start_time_s);
                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                l_status = start_connections();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                        return NULL;
                }

                // Run loop
                //NDBG_PRINT("%sRUN%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                m_evr_loop->run();
                //NDBG_PRINT("%sDONE%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);

        }

        m_stopped = true;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::create_new_nconn(uint32_t a_id, const reqlet &a_reqlet)
{
        nconn *l_nconn = NULL;


        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_TCP)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_tcp(m_verbose, m_color, m_max_reqs_per_conn, false, true);
        }
        else if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_ssl(m_verbose, m_color, m_max_reqs_per_conn, false, true);
        }

        return l_nconn;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::start_connections(void)
{
        // Find an empty connection slot.
        //NDBG_PRINT("m_conn_free_list.size(): %Zu\n", m_conn_free_list.size());
        for (conn_id_list_t::iterator i_conn = m_conn_free_list.begin(); i_conn != m_conn_free_list.end();)
        {
                if (false == has_available_fetches())
                        break;

                // Grab reqlet from repo
                reqlet *l_reqlet = NULL;
                // TODO Check for error???

                l_reqlet = reqlet_take();
                if(!l_reqlet)
                {
                        // No available reqlets
                        if(m_verbose)
                        {
                                NDBG_PRINT("Bailing out out.  Reason no reqlets available\n");
                        }
                        return STATUS_OK;

                }

                // Start connection for this reqlet
                //NDBG_PRINT("i_conn: %d\n", *i_conn);
                nconn *l_nconn = m_nconn_vector[*i_conn];
                // TODO Check for NULL

                if(l_nconn &&
                   (l_nconn->m_scheme != l_reqlet->m_url.m_scheme))
                {
                        // Destroy nconn and recreate
                        delete l_nconn;
                        l_nconn = NULL;
                }

                if(!l_nconn)
                {
                        // Create nconn
                        l_nconn = create_new_nconn(*i_conn, *l_reqlet);
                        if(!l_nconn)
                        {
                                NDBG_PRINT("Error performing create_new_nconn\n");
                                return STATUS_ERROR;
                        }

                        // -------------------------------------------
                        // Set options
                        // -------------------------------------------
                        // Set generic options
                        SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_sock_opt_recv_buf_size);
                        SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_sock_opt_send_buf_size);
                        SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_sock_opt_no_delay);

                        // Set ssl options
                        if(l_reqlet->m_url.m_scheme == nconn::SCHEME_SSL)
                        {
                                SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CIPHER_STR, m_cipher_str.c_str(), m_cipher_str.length());
                                SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CTX, m_ssl_ctx, sizeof(m_ssl_ctx));
                        }

                }

                int32_t l_status;

                // Assign the reqlet for this connection
                l_nconn->set_data1(l_reqlet);

                // Bump stats
                ++(l_reqlet->m_stat_agg.m_num_conn_started);

                // Create request
                create_request(*l_nconn, *l_reqlet);

                m_conn_used_set.insert(*i_conn);
                m_conn_free_list.erase(i_conn++);

                // TODO Make configurable
                m_evr_loop->add_timer(m_timeout_s*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

                //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                l_nconn->set_host(l_reqlet->m_url.m_host);
                l_status = l_nconn->run_state_machine(m_evr_loop, l_reqlet->m_host_info);
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error: Performing do_connect: status: %d\n", l_status);
                        cleanup_connection(l_nconn);
                        continue;
                }

        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::create_request(nconn &ao_conn,
                reqlet &a_reqlet)
{

        // Get connection
        char *l_req_buf = NULL;
        uint32_t l_req_buf_len = 0;
        uint32_t l_max_buf_len = nconn_tcp::m_max_req_buf;

        GET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF, (void **)(&l_req_buf), &l_req_buf_len);

        // -------------------------------------------
        // Request.
        // -------------------------------------------
        const std::string &l_path_ref = a_reqlet.get_path(m_rand_ptr);
        //NDBG_PRINT("PATH: %s\n", l_path_ref.c_str());
        if(l_path_ref.length())
        {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "GET %.500s HTTP/1.1\r\n", l_path_ref.c_str());
        } else {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "GET / HTTP/1.1\r\n");
        }

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(header_map_t::const_iterator i_header = m_header_map.begin();
                        i_header != m_header_map.end();
                        ++i_header)
        {
                //printf("Adding HEADER: %s: %s\n", i_header->first.c_str(), i_header->second.c_str());
                l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                "%s: %s\r\n", i_header->first.c_str(), i_header->second.c_str());

                if (strcasecmp(i_header->first.c_str(), "host") == 0)
                {
                        l_specd_host = true;
                }
        }

        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                "Host: %s\r\n", a_reqlet.m_url.m_host.c_str());
        }

        // -------------------------------------------
        // End of request terminator...
        // -------------------------------------------
        l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len, "\r\n");

        // Set len
        SET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF_LEN, NULL, l_req_buf_len);
        return STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::cleanup_connection(nconn *a_nconn, bool a_cancel_timer)
{

        uint64_t l_conn_id = a_nconn->get_id();

        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }
        a_nconn->reset_stats();
        a_nconn->cleanup(m_evr_loop);

        // Add back to free list
        m_conn_free_list.push_back(l_conn_id);
        m_conn_used_set.erase(l_conn_id);

#if 0
        // Reduce num pending
        ++m_num_fetched;
        --m_num_pending;
#endif

        return STATUS_OK;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
        int32_t l_retval = STATUS_OK;
        m_header_map[a_header_key] = a_header_val;
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::get_stats_copy(tag_stat_map_t &ao_tag_stat_map)
{
        // TODO Need to make this threadsafe -spinlock perhaps...
        for(reqlet_list_t::iterator i_reqlet = m_reqlet_avail_list.begin(); i_reqlet != m_reqlet_avail_list.end(); ++i_reqlet)
        {
                ao_tag_stat_map[(*i_reqlet)->get_label()] = (*i_reqlet)->m_stat_agg;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_avail(reqlet *a_reqlet)
{

        // Add to available list...
        //--m_unresolved_count;

        //if(a_reqlet)
        //      NDBG_PRINT("Add available: a_reqlet: %p %sHOST%s: %s\n", a_reqlet, ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_reqlet->m_url.m_host.c_str());

        m_reqlet_avail_list.push_back(a_reqlet);

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_url(std::string &a_url)
{

        // TODO
        // Make threadsafe...

        reqlet *l_reqlet = new reqlet((uint64_t)(m_reqlet_map.size()));

        // Initialize
        int32_t l_status = STATUS_OK;
        l_status = l_reqlet->init_with_url(a_url, m_wilcarding);
        if(STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing init_with_url: %s\n", a_url.c_str());
                return STATUS_ERROR;
        }

        //NDBG_PRINT("PARSE: | %d | %s | %d | %s |\n",
        //              l_reqlet->m_url.m_scheme,
        //              l_reqlet->m_url.m_host.c_str(),
        //              l_reqlet->m_url.m_port,
        //              l_reqlet->m_url.m_path.c_str());

        m_reqlet_map[l_reqlet->get_id()] = l_reqlet;
        //++m_unresolved_count;

        // Slow resolve
        l_status = l_reqlet->resolve();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing resolving host: %s\n", a_url.c_str());
                return STATUS_ERROR;
        }

        // Test connection

        //--m_unresolved_count;
        m_reqlet_avail_list.push_back(l_reqlet);

        //t_async_resolver::get()->add_lookup(l_reqlet, resolve_cb);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_url_file(std::string &a_url_file)
{

        FILE * l_file;
        int32_t l_status = STATUS_OK;

        l_file = fopen (a_url_file.c_str(),"r");
        if (NULL == l_file)
        {
                NDBG_PRINT("Error opening file.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        //NDBG_PRINT("ADD_FILE: ADDING: %s\n", a_url_file.c_str());
        //uint32_t l_num_added = 0;

        ssize_t l_file_line_size = 0;
        char *l_file_line = NULL;
        size_t l_unused;
        while((l_file_line_size = getline(&l_file_line,&l_unused,l_file)) != -1)
        {

                //NDBG_PRINT("LINE: %s", l_file_line);

                std::string l_line(l_file_line);

                if(!l_line.empty())
                {
                        //NDBG_PRINT("Add url: %s\n", l_line.c_str());

                        l_line.erase( std::remove_if( l_line.begin(), l_line.end(), ::isspace ), l_line.end() );
                        if(!l_line.empty())
                        {
                                l_status = add_url(l_line);
                                if(STATUS_OK != l_status)
                                {
                                        NDBG_PRINT("Error performing addurl for url: %s\n", l_line.c_str());

                                        if(l_file_line)
                                        {
                                                free(l_file_line);
                                                l_file_line = NULL;
                                        }
                                        return STATUS_ERROR;
                                }
                        }
                }

                if(l_file_line)
                {
                        free(l_file_line);
                        l_file_line = NULL;
                }

        }

        //NDBG_PRINT("ADD_FILE: DONE: %s -- last line len: %d\n", a_url_file.c_str(), (int)l_file_line_size);
        //if(l_file_line_size == -1)
        //{
        //        NDBG_PRINT("Error: getline errno: %d Reason: %s\n", errno, strerror(errno));
        //}


        l_status = fclose(l_file);
        if (0 != l_status)
        {
                NDBG_PRINT("Error closing file.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *t_client::reqlet_take(void)
{

        reqlet *l_reqlet = NULL;

        //NDBG_PRINT("m_rate_limit = %d m_rate_delta_us = %lu\n", m_rate_limit, m_rate_delta_us);
        //NDBG_PRINT("m_reqlet_avail_list.size(): %d\n", (int)m_reqlet_avail_list.size());

        if(0 == m_reqlet_avail_list.size())
        {
                return NULL;
        }

        limit_rate();

        // Based on mode
        switch(m_reqlet_mode)
        {
        case REQLET_MODE_ROUND_ROBIN:
        {
                uint32_t l_next_index = ((m_last_reqlet_index + 1) >= m_reqlet_avail_list.size()) ? 0 : m_last_reqlet_index + 1;
                //NDBG_PRINT("m_last_reqlet_index: %d\n", m_last_reqlet_index);
                m_last_reqlet_index = l_next_index;
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                break;
        }
        case REQLET_MODE_SEQUENTIAL:
        {
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                if(l_reqlet->is_done())
                {
                        uint32_t l_next_index = ((m_last_reqlet_index + 1) >= m_reqlet_avail_list.size()) ? 0 : m_last_reqlet_index + 1;
                        l_reqlet->reset();
                        m_last_reqlet_index = l_next_index;
                        l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                }
                break;
        }
        case REQLET_MODE_RANDOM:
        {
                tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
                uint32_t l_next_index = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % m_reqlet_avail_list.size());
                m_last_reqlet_index = l_next_index;
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                break;
        }
        default:
        {
                // Default to round robin
                uint32_t l_next_index = ((m_last_reqlet_index + 1) >= m_reqlet_avail_list.size()) ? 0 : m_last_reqlet_index + 1;
                m_last_reqlet_index = l_next_index;
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
        }
        }


        if(l_reqlet)
        {
                l_reqlet->bump_num_requested();
        }

        return l_reqlet;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool t_client::reqlet_give_and_can_reuse_conn(reqlet *a_reqlet)
{
        limit_rate();
        return has_available_fetches();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::set_url(const std::string &a_url)
{
	int32_t l_retval = STATUS_OK;

	m_url = a_url;

	return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::set_url_file(const std::string &a_url_file)
{
	int32_t l_retval = STATUS_OK;

	m_url_file = a_url_file;

	return l_retval;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
	int32_t l_retval = STATUS_OK;
	m_header_map[a_header_key] = a_header_val;
	return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_end_fetches(int32_t a_val)
{
	m_end_fetches = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_max_reqs_per_conn(int64_t a_val)
{
	m_max_reqs_per_conn = a_val;
	if((m_max_reqs_per_conn > 1) || (m_max_reqs_per_conn < 0))
	{
		set_header("Connection", "keep-alive");
	}
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_rate(int32_t a_val)
{
	m_rate = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::set_reqlet_mode(reqlet_mode_t a_mode)
{
	m_reqlet_mode = a_mode;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void show_total_agg_stat(std::string &a_tag,
        const total_stat_agg_t &a_stat,
        double a_time_elapsed_s,
        uint32_t a_max_parallel,
        bool a_color)
{
        if(a_color)
        printf("| %sRESULTS%s:             %s%s%s\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, ANSI_COLOR_FG_YELLOW, a_tag.c_str(), ANSI_COLOR_OFF);
        else
        printf("| RESULTS:             %s\n", a_tag.c_str());

        printf("| fetches:             %lu\n", a_stat.m_total_reqs);
        printf("| max parallel:        %u\n", a_max_parallel);
        printf("| bytes:               %e\n", (double)a_stat.m_total_bytes);
        printf("| seconds:             %f\n", a_time_elapsed_s);
        printf("| mean bytes/conn:     %f\n", ((double)a_stat.m_total_bytes)/((double)a_stat.m_total_reqs));
        printf("| fetches/sec:         %f\n", ((double)a_stat.m_total_reqs)/(a_time_elapsed_s));
        printf("| bytes/sec:           %e\n", ((double)a_stat.m_total_bytes)/a_time_elapsed_s);

#define SHOW_XSTAT_LINE(_tag, stat)\
        do {\
        printf("| %-16s %12.6f mean, %12.6f max, %12.6f min, %12.6f stdev, %12.6f var\n",\
               _tag,                                                    \
               stat.mean()/1000.0,                                      \
               stat.max()/1000.0,                                       \
               stat.min()/1000.0,                                       \
               stat.stdev()/1000.0,                                     \
               stat.var()/1000.0);                                      \
        } while(0)

        SHOW_XSTAT_LINE("ms/connect:", a_stat.m_stat_us_connect);
        SHOW_XSTAT_LINE("ms/1st-response:", a_stat.m_stat_us_first_response);
        SHOW_XSTAT_LINE("ms/download:", a_stat.m_stat_us_download);
        SHOW_XSTAT_LINE("ms/end2end:", a_stat.m_stat_us_end_to_end);

        if(a_color)
                printf("| %sHTTP response codes%s: \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        else
                printf("| HTTP response codes: \n");

        for(status_code_count_map_t::const_iterator i_status_code = a_stat.m_status_code_count_map.begin();
                        i_status_code != a_stat.m_status_code_count_map.end();
                ++i_status_code)
        {
                if(a_color)
                printf("| %s%3d%s -- %u\n", ANSI_COLOR_FG_MAGENTA, i_status_code->first, ANSI_COLOR_OFF, i_status_code->second);
                else
                printf("| %3d -- %u\n", i_status_code->first, i_status_code->second);
        }

        // Done flush...
        printf("\n");

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results(double a_elapsed_time,
                          bool a_show_breakdown_flag)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        // Get stats
        get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // TODO Fix elapse and max parallel
        l_tag = "ALL";
        show_total_agg_stat(l_tag, l_total, a_elapsed_time, m_start_parallel, m_color);

        // -------------------------------------------
        // Optional Breakdown
        // -------------------------------------------
        if(a_show_breakdown_flag)
        {
                for(tag_stat_map_t::iterator i_stat = l_tag_stat_map.begin();
                                i_stat != l_tag_stat_map.end();
                                ++i_stat)
                {
                        l_tag = i_stat->first;
                        show_total_agg_stat(l_tag, i_stat->second, a_elapsed_time, m_start_parallel, m_color);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void show_total_agg_stat_legacy(std::string &a_tag,
                                       const total_stat_agg_t &a_stat,
                                       std::string &a_sep,
                                       double a_time_elapsed_s,
                                       uint32_t a_max_parallel)
{
        printf("%s: ", a_tag.c_str());
        printf("%lu fetches, ", a_stat.m_total_reqs);
        printf("%u max parallel, ", a_max_parallel);
        printf("%e bytes, ", (double)a_stat.m_total_bytes);
        printf("in %f seconds, ", a_time_elapsed_s);
        printf("%s", a_sep.c_str());

        printf("%f mean bytes/connection, ", ((double)a_stat.m_total_bytes)/((double)a_stat.m_total_reqs));
        printf("%s", a_sep.c_str());

        printf("%f fetches/sec, %e bytes/sec", ((double)a_stat.m_total_reqs)/(a_time_elapsed_s), ((double)a_stat.m_total_bytes)/a_time_elapsed_s);
        printf("%s", a_sep.c_str());

#define SHOW_XSTAT_LINE_LEGACY(_tag, stat)\
        printf("%s %.6f mean, %.6f max, %.6f min, %.6f stdev",\
               _tag,                                          \
               stat.mean()/1000.0,                            \
               stat.max()/1000.0,                             \
               stat.min()/1000.0,                             \
               stat.stdev()/1000.0);                          \
        printf("%s", a_sep.c_str())

        SHOW_XSTAT_LINE_LEGACY("msecs/connect:", a_stat.m_stat_us_connect);
        SHOW_XSTAT_LINE_LEGACY("msecs/first-response:", a_stat.m_stat_us_first_response);
        SHOW_XSTAT_LINE_LEGACY("msecs/download:", a_stat.m_stat_us_download);
        SHOW_XSTAT_LINE_LEGACY("msecs/end2end:", a_stat.m_stat_us_end_to_end);

        printf("HTTP response codes: ");
        if(a_sep == "\n")
                printf("%s", a_sep.c_str());

        for(status_code_count_map_t::const_iterator i_status_code = a_stat.m_status_code_count_map.begin();
                        i_status_code != a_stat.m_status_code_count_map.end();
                ++i_status_code)
        {
                printf("code %d -- %u, ", i_status_code->first, i_status_code->second);
        }

        // Done flush...
        printf("\n");

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::get_stats(total_stat_agg_t &ao_all_stats,
                    bool a_get_breakdown,
                    tag_stat_map_t &ao_breakdown_stats)
{
        // -------------------------------------------
        // Aggregate
        // -------------------------------------------
        for(t_client_list_t::iterator i_c = m_t_client_list.begin();
                        i_c != m_t_client_list.end();
                        ++i_c)
        {

                // Grab a copy of the stats
                tag_stat_map_t l_copy;
                (*i_c)->get_stats_copy(l_copy);
                for(tag_stat_map_t::iterator i_reqlet = l_copy.begin(); i_reqlet != l_copy.end(); ++i_reqlet)
                {
                        if(a_get_breakdown)
                        {
                                std::string l_tag = i_reqlet->first;
                                tag_stat_map_t::iterator i_stat;
                                if((i_stat = ao_breakdown_stats.find(l_tag)) == ao_breakdown_stats.end())
                                {
                                        ao_breakdown_stats[l_tag] = i_reqlet->second;
                                }
                                else
                                {
                                        // Add to existing
                                        add_to_total_stat_agg(i_stat->second, i_reqlet->second);
                                }
                        }

                        // Add to total
                        add_to_total_stat_agg(ao_all_stats, i_reqlet->second);
                }
        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results_http_load_style(double a_elapsed_time,
                                          bool a_show_breakdown_flag,
                                          bool a_one_line_flag)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        // Get stats
        get_stats(l_total, a_show_breakdown_flag, l_tag_stat_map);

        std::string l_tag;
        // Separator
        std::string l_sep = "\n";
        if(a_one_line_flag) l_sep = "||";

        // TODO Fix elapse and max parallel
        l_tag = "State";
        show_total_agg_stat_legacy(l_tag, l_total, l_sep, a_elapsed_time, m_start_parallel);

        // -------------------------------------------
        // Optional Breakdown
        // -------------------------------------------
        if(a_show_breakdown_flag)
        {
                for(tag_stat_map_t::iterator i_stat = l_tag_stat_map.begin();
                                i_stat != l_tag_stat_map.end();
                                ++i_stat)
                {
                        l_tag = "[";
                        l_tag += i_stat->first;
                        l_tag += "]";
                        show_total_agg_stat_legacy(l_tag, i_stat->second, l_sep, a_elapsed_time, m_start_parallel);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
{

        tag_stat_map_t l_tag_stat_map;
        total_stat_agg_t l_total;

        uint64_t l_time_ms = get_time_ms();
        // Get stats
        get_stats(l_total, true, l_tag_stat_map);

        int l_cur_offset = 0;
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"{\"data\": [");
        bool l_first_stat = true;
        for(tag_stat_map_t::iterator i_agg_stat = l_tag_stat_map.begin();
                        i_agg_stat != l_tag_stat_map.end();
                        ++i_agg_stat)
        {

                if(l_first_stat) l_first_stat = false;
                else
                        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,",");

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                                "{\"key\": \"%s\", \"value\": ",
                                i_agg_stat->first.c_str());

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                                "{\"%s\": %" PRIu64 ", \"%s\": %" PRIu64 "}",
                                "count", (uint64_t)(i_agg_stat->second.m_num_conn_completed),
                                "time", (uint64_t)(l_time_ms));

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"}");
        }

        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"]}");


        return l_cur_offset;

}



//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results_line_desc(void)
{
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
        if(m_color)
        {
        printf("| %s%9s%s / %s%9s%s | %s%9s%s | %s%9s%s | %s%12s%s | %9s | %11s | %9s |\n",
                        ANSI_COLOR_FG_GREEN, "Cmpltd", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_BLUE, "Total", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_MAGENTA, "IdlKil", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_RED, "Errors", ANSI_COLOR_OFF,
                        ANSI_COLOR_FG_YELLOW, "kBytes Recvd", ANSI_COLOR_OFF,
                        "Elapsed",
                        "Req/s",
                        "MB/s");
        }
        else
        {
                printf("| %9s / %9s | %9s | %9s | %12s | %9s | %11s | %9s |\n",
                                "Cmpltd",
                                "Total",
                                "IdlKil",
                                "Errors",
                                "kBytes Recvd",
                                "Elapsed",
                                "Req/s",
                                "MB/s");
        }
        printf("+-----------/-----------+-----------+-----------+--------------+-----------+-------------+-----------+\n");
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlo::display_results_line(void)
{

        total_stat_agg_t l_total;
        tag_stat_map_t l_unused;
        uint64_t l_cur_time_ms = get_time_ms();

        // Get stats
        get_stats(l_total, false, l_unused);

        double l_reqs_per_s = ((double)(l_total.m_num_conn_completed - m_last_stat.m_num_conn_completed)*1000.0) /
                        ((double)(l_cur_time_ms - m_last_display_time_ms));
        double l_kb_per_s = ((double)(l_total.m_num_bytes_read - m_last_stat.m_num_bytes_read)*1000.0/1024) /
                        ((double)(l_cur_time_ms - m_last_display_time_ms));

        if(m_color)
        {
                        printf("| %s%9" PRIu64 "%s / %s%9" PRIi64 "%s | %s%9" PRIu64 "%s | %s%9" PRIu64 "%s | %s%12.2f%s | %8.2fs | %10.2fs | %8.2fs |\n",
                                        ANSI_COLOR_FG_GREEN, l_total.m_num_conn_completed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_BLUE, l_total.m_num_conn_completed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_MAGENTA, l_total.m_num_idle_killed, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_RED, l_total.m_num_errors, ANSI_COLOR_OFF,
                                        ANSI_COLOR_FG_YELLOW, ((double)(l_total.m_num_bytes_read))/(1024.0), ANSI_COLOR_OFF,
                                        ((double)(get_delta_time_ms(m_start_time_ms))) / 1000.0,
                                        l_reqs_per_s,
                                        l_kb_per_s/1024.0
                                        );
        }
        else
        {
                printf("| %9" PRIu64 " / %9" PRIi64 " | %9" PRIu64 " | %9" PRIu64 " | %12.2f | %8.2fs | %10.2fs | %8.2fs |\n",
                                l_total.m_num_conn_completed,
                                l_total.m_num_conn_completed,
                                l_total.m_num_idle_killed,
                                l_total.m_num_errors,
                                ((double)(l_total.m_num_bytes_read))/(1024.0),
                                ((double)(get_delta_time_ms(m_start_time_ms)) / 1000.0),
                                l_reqs_per_s,
                                l_kb_per_s/1024.0
                                );

        }

        m_last_display_time_ms = get_time_ms();
        m_last_stat = l_total;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::run(void)
{
	int32_t l_retval = STATUS_OK;

	// Check is initialized
	if(!m_is_initd)
	{
		l_retval = init();
		if(STATUS_OK != l_retval)
		{
			NDBG_PRINT("Error: performing init.\n");
			return STATUS_ERROR;
		}
	}


        set_start_time_ms(get_time_ms());

	// Caculate num parallel per thread
	uint32_t l_num_parallel_conn_per_thread = m_start_parallel / m_num_threads;
	if(l_num_parallel_conn_per_thread < 1) l_num_parallel_conn_per_thread = 1;

        uint32_t l_num_fetches_per_thread = m_end_fetches / m_num_threads;
        uint32_t l_remainder_fetches = m_end_fetches % m_num_threads;

	// -------------------------------------------
	// Create t_client list...
	// -------------------------------------------
	for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
	{

		if(m_verbose)
		{
			NDBG_PRINT("Creating...\n");
		}

		// Construct with settings...
		t_client *l_t_client = new t_client(
			m_verbose,
			m_color,
			m_sock_opt_recv_buf_size,
			m_sock_opt_send_buf_size,
			m_sock_opt_no_delay,
			m_cipher_list,
			m_ssl_ctx,
			m_evr_type,
			l_num_parallel_conn_per_thread,
			m_run_time_s,
			m_timeout_s,
			m_max_reqs_per_conn,
			m_url,
			m_url_file,
			m_wildcarding
			);

                if (i_client_idx == (m_num_threads - 1))
                        l_num_fetches_per_thread += l_remainder_fetches;

		l_t_client->set_rate(m_rate);
		l_t_client->set_end_fetches(l_num_fetches_per_thread);
		l_t_client->set_reqlet_mode(m_reqlet_mode);

		for(header_map_t::iterator i_header = m_header_map.begin();i_header != m_header_map.end(); ++i_header)
		{
			l_t_client->set_header(i_header->first, i_header->second);
		}

		m_t_client_list.push_back(l_t_client);

	}

	// Set start time...
        set_start_time_ms(get_time_ms());

	// -------------------------------------------
	// Run...
	// -------------------------------------------
	for(t_client_list_t::iterator i_client = m_t_client_list.begin();
		i_client != m_t_client_list.end();
		++i_client)
	{

		if(m_verbose)
		{
			NDBG_PRINT("Running...\n");
		}
		(*i_client)->run();

	}

	return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::stop(void)
{
	int32_t l_retval = STATUS_OK;

	for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
			i_t_client != m_t_client_list.end();
			++i_t_client)
	{
		(*i_t_client)->stop();
	}

	return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::wait_till_stopped(void)
{
	int32_t l_retval = STATUS_OK;

	// -------------------------------------------
	// Join all threads before exit
	// -------------------------------------------
	for(t_client_list_t::iterator i_client = m_t_client_list.begin();
			i_client != m_t_client_list.end(); ++i_client)
	{

		//if(l_settings.m_verbose)
		//{
		//	NDBG_PRINT("joining...\n");
		//}
		pthread_join(((*i_client)->m_t_run_thread), NULL);

	}

	return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlo::is_running(void)
{
	for (t_client_list_t::iterator i_client = m_t_client_list.begin();
			i_client != m_t_client_list.end(); ++i_client)
	{
		if((*i_client)->is_running())
			return true;
	}

	return false;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlo::init(void)
{
	// Check if already is initd
	if(m_is_initd)
		return STATUS_OK;

	// SSL init...
	m_ssl_ctx = ssl_init(m_cipher_list);
	if(NULL == m_ssl_ctx) {
		NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_cipher_list.c_str());
		return STATUS_ERROR;
	}

	// -------------------------------------------
	// Start async resolver
	// -------------------------------------------
	//t_async_resolver::get()->run();

	m_is_initd = true;
	return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlo::hlo(void) :
        m_t_client_list(),
	m_ssl_ctx(NULL),
	m_is_initd(false),
	m_cipher_list(""),
	m_verbose(false),
	m_color(false),
	m_quiet(false),
	m_sock_opt_recv_buf_size(0),
	m_sock_opt_send_buf_size(0),
	m_sock_opt_no_delay(false),
	m_evr_type(EV_EPOLL),
	m_start_parallel(64),
	m_end_fetches(-1),
	m_max_reqs_per_conn(1),
	m_run_time_s(-1),
	m_timeout_s(HLO_DEFAULT_CONN_TIMEOUT_S),
	m_num_threads(1),
	m_url(),
	m_url_file(),
	m_header_map(),
	m_rate(-1),
	m_reqlet_mode(REQLET_MODE_ROUND_ROBIN),
        m_wildcarding(true),
        m_start_time_ms(),
        m_last_display_time_ms(),
        m_last_stat()
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlo::~hlo()
{

	// -------------------------------------------
	// Delete t_client list...
	// -------------------------------------------
	for(t_client_list_t::iterator i_client = m_t_client_list.begin();
			i_client != m_t_client_list.end(); )
	{

		t_client *l_t_client_ptr = *i_client;
		delete l_t_client_ptr;
		m_t_client_list.erase(i_client++);

	}

	// SSL Cleanup
	ssl_kill_locks();

	// TODO Deprecated???
	//EVP_cleanup();

}


