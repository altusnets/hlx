//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_ssl.cc
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
#include "nconn_ssl.h"
#include "util.h"
#include "evr.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#if 0
#include "reqlet.h"
#include "ndebug.h"
#include "parsed_url.h"

#include <string>

// Fcntl and friends
#include <fcntl.h>

#include <netinet/tcp.h>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>


//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ssl_connect(const host_info_t &a_host_info)
{
        // -------------------------------------------
        // HTTPSf
        // -------------------------------------------

        int l_status;
        m_ssl_state = SSL_STATE_SSL_CONNECTING;

        l_status = SSL_connect(m_ssl);
        // TODO REMOVE
        //NDBG_PRINT("%sSSL_CON%s[%3d]: Status %3d. Reason: %s\n",
        //                ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));
        //NDBG_PRINT_BT();
        if (l_status <= 0)
        {
                //fprintf(stderr, "%s: SSL connection failed - %d\n", "hlo", l_status);
                //NDBG_PRINT("Showing error.\n");

                int l_ssl_error = SSL_get_error(m_ssl, l_status);
                switch(l_ssl_error) {
                case SSL_ERROR_SSL:
                {
                        NDBG_PRINT("SSL_ERROR_SSL %lu: %s\n", ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                        break;
                }
                case SSL_ERROR_WANT_READ:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_CONNECTING_WANT_READ;
                        return EAGAIN;

                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_STATE_SSL_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_CONNECTING_WANT_WRITE;
                        return EAGAIN;
                }

                case SSL_ERROR_WANT_X509_LOOKUP:
                {
                        NDBG_PRINT("SSL_ERROR_WANT_X509_LOSTATUS_OKUP\n");
                        break;
                }

                // look at error stack/return value/errno
                case SSL_ERROR_SYSCALL:
                {
                        NDBG_PRINT("SSL_ERROR_SYSCALL %lu: %s\n", ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                        if(l_status == 0) {
                                NDBG_PRINT("An EOF was observed that violates the protocol\n");
                        } else if(l_status == -1) {
                                NDBG_PRINT("%s\n", strerror(errno));
                        }
                        break;
                }
                case SSL_ERROR_ZERO_RETURN:
                {
                        NDBG_PRINT("SSL_ERROR_ZERO_RETURN\n");
                        break;
                }
                case SSL_ERROR_WANT_CONNECT:
                {
                        NDBG_PRINT("SSL_ERROR_WANT_CONNECT\n");
                        break;
                }
                case SSL_ERROR_WANT_ACCEPT:
                {
                        NDBG_PRINT("SSL_ERROR_WANT_ACCEPT\n");
                        break;
                }
                }


                ERR_print_errors_fp(stderr);
                return STATUS_ERROR;
        }
        else if(1 == l_status)
        {
                m_ssl_state = SSL_STATE_CONNECTED;
        }

        // Stats
        if(m_collect_stats_flag)
        {
                m_stat.m_tt_ssl_connect_us = get_delta_time_us(m_connect_start_time_us);
        }

#if 0
        static bool s_first = true;
        if (s_first)
        {
                s_first = false;
                const char* cipher_name = SSL_get_cipher_name(m_ssl);
                const char* cipher_version = SSL_get_cipher_version(m_ssl);
                if(m_verbose)
                {
                        NDBG_PRINT("got ssl m_cipher %s %s\n", cipher_name, cipher_version);
                }
        }
#endif

        //did_connect = 1;

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::send_request(bool is_reuse)
{

        //NDBG_OUTPUT("%s: REQUEST-->\n%s%.*s%s\n", m_host.c_str(), ANSI_COLOR_BG_MAGENTA, (int)m_req_buf_len, m_req_buf, ANSI_COLOR_OFF);

        // Bump number requested
        ++m_num_reqs;

        // Save last connect time for reuse
        if(is_reuse)
        {
                if(m_collect_stats_flag)
                {
                        m_stat.m_tt_connect_us = m_last_connect_time_us;
                }
        }

        // Reset to support reusing connection
        m_read_buf_idx = 0;

        if(m_verbose)
        {
                if(m_color)
                        NDBG_OUTPUT("%s: REQUEST-->\n%s%.*s%s\n", m_host.c_str(), ANSI_COLOR_FG_MAGENTA, (int)m_req_buf_len, m_req_buf, ANSI_COLOR_OFF);
                else
                        NDBG_OUTPUT("%s: REQUEST-->\n%.*s\n", m_host.c_str(), (int)m_req_buf_len, m_req_buf);

        }

        // TODO Wrap with while and check for errors
        uint32_t l_bytes_written = 0;
        int l_status;
        while(l_bytes_written < m_req_buf_len)
        {
                l_status = SSL_write(m_ssl, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                if(l_status < 0)
                {
                        NDBG_PRINT("Error: performing SSL_write.\n");
                        return STATUS_ERROR;
                }
                l_bytes_written += l_status;
        }

        // TODO REMOVE
        //NDBG_OUTPUT("%sWRITE  %s[%3d]: Status %3d. Reason: %s\n",
        //                ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));

        // Get request time
        if(m_collect_stats_flag)
        {
                m_request_start_time_us = get_time_us();
        }

        m_ssl_state = SSL_STATE_READING;
        //header_state = HDST_LINE1_PROTOCOL;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::receive_response(void)
{

        uint32_t l_total_bytes_read = 0;
        int32_t l_bytes_read = 0;
        int32_t l_max_read = m_max_read_buf - m_read_buf_idx;
        bool l_should_give_up = false;

        do {
                l_bytes_read = SSL_read(m_ssl, m_read_buf + m_read_buf_idx, l_max_read);
                if(l_bytes_read < 0)
                {
                        int l_ssl_error = SSL_get_error(m_ssl, l_bytes_read);
                        //NDBG_PRINT("%sSSL_READ%s[%3d] l_bytes_read: %d error: %d\n",
                        //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        //                SSL_get_fd(m_ssl), l_bytes_read,
                        //                l_ssl_error);
                        if(l_ssl_error == SSL_ERROR_WANT_READ)
                        {
                                return STATUS_OK;
                        }
                }

                //NDBG_PRINT("%sHOST%s: %s fd[%3d]\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_host.c_str(), m_fd);
                //ns_hlo::mem_display((uint8_t *)(m_read_buf + m_read_buf_idx), l_bytes_read);

                // TODO Handle EOF -close connection...
                if ((l_bytes_read <= 0) && (errno != EAGAIN))
                {
                        if(m_verbose)
                        {
                                NDBG_PRINT("%s: %sl_bytes_read%s[%d] <= 0 total = %u--error: %s\n",
                                                m_host.c_str(),
                                                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_bytes_read, l_total_bytes_read, strerror(errno));
                        }
                        //close_connection(nowP);
                        return STATUS_ERROR;
                }

                if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                {
                        l_should_give_up = true;
                }

                l_total_bytes_read += l_bytes_read;

                // Stats
                m_stat.m_total_bytes += l_bytes_read;
                if(m_collect_stats_flag)
                {
                        if((m_read_buf_idx == 0) && (m_stat.m_tt_first_read_us == 0))
                        {
                                m_stat.m_tt_first_read_us = get_delta_time_us(m_request_start_time_us);
                        }
                }

                // -----------------------------------------
                // Parse result
                // -----------------------------------------
                size_t l_parse_status = 0;
                //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
                //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
                //                m_read_buf, (int)m_read_buf_idx, (int)l_bytes_read);
                l_parse_status = http_parser_execute(&m_http_parser, &m_http_parser_settings, m_read_buf + m_read_buf_idx, l_bytes_read);
                if(l_parse_status < (size_t)l_bytes_read)
                {
                        if(m_verbose)
                        {
                                NDBG_PRINT("%s: Error: parse error.  Reason: %s: %s\n",
                                                m_host.c_str(),
                                                //"","");
                                                http_errno_name((enum http_errno)m_http_parser.http_errno),
                                                http_errno_description((enum http_errno)m_http_parser.http_errno));
                                //NDBG_PRINT("%s: %sl_bytes_read%s[%d] <= 0 total = %u idx = %u\n",
                                //		m_host.c_str(),
                                //		ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_bytes_read, l_total_bytes_read, m_read_buf_idx);
                                //ns_hlo::mem_display((const uint8_t *)m_read_buf + m_read_buf_idx, l_bytes_read);

                        }

                        m_read_buf_idx += l_bytes_read;
                        return STATUS_ERROR;

                }

                m_read_buf_idx += l_bytes_read;
                if(l_bytes_read < l_max_read)
                {
                        break;
                }
                // Wrap the index and read again if was too large...
                else
                {
                        m_read_buf_idx = 0;
                }

                if(l_should_give_up)
                        break;

        } while(l_bytes_read > 0);

        return l_total_bytes_read;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::cleanup(evr_loop *a_evr_loop)
{
        // Shut down connection
        if(m_ssl)
        {
                SSL_free(m_ssl);
                m_ssl = NULL;
        }

        //NDBG_PRINT("CLOSE[%lu--%d] %s--CONN--%s\n", m_id, m_fd, ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);

        // Reset all the values
        // TODO Make init function...
        // Set num use back to zero -we need reset method here?
        m_ssl_state = SSL_STATE_FREE;

        // Super
        nconn_tcp::cleanup(a_evr_loop);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info)
{

        uint32_t l_retry_connect_count = 0;

        // Cancel last timer
        a_evr_loop->cancel_timer(&(m_timer_obj));

        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d] --START\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, m_ssl_state);
state_top:
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d]\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, m_ssl_state);
        switch (m_ssl_state)
        {

        // -------------------------------------------------
        // STATE: FREE
        // -------------------------------------------------
        case SSL_STATE_FREE:
        {
                int32_t l_status;
                l_status = setup_socket(a_host_info);
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }

                //NDBG_PRINT("m_ssl_ctx: %p\n", m_ssl_ctx);

                // Create SSL Context
                m_ssl = SSL_new(m_ssl_ctx);
                // TODO Check for NULL

                SSL_set_fd(m_ssl, m_fd);
                // TODO Check for Errors

                // Get start time
                // Stats
                if(m_collect_stats_flag)
                {
                        m_connect_start_time_us = get_time_us();
                }

                if (0 != a_evr_loop->add_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        return STATUS_ERROR;
                }

                m_ssl_state = SSL_STATE_CONNECTING;
                goto state_top;

        }

        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case SSL_STATE_CONNECTING:
        {
                //int l_status;
                //NDBG_PRINT("%sCNST_CONNECTING%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                int l_connect_status = 0;
                l_connect_status = connect(m_fd,
                                           (struct sockaddr*) &(a_host_info.m_sa),
                                           (a_host_info.m_sa_len));

                // TODO REMOVE
                //++l_retry;
                //NDBG_PRINT_BT();
                //NDBG_PRINT("%sCONNECT%s[%3d]: Retry: %d Status %3d. Reason[%d]: %s\n",
                //           ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
                //           m_fd, l_retry_connect_count, l_connect_status,
                //           errno,
                //           strerror(errno));

                if (l_connect_status < 0)
                {
                        switch (errno)
                        {
                        case EISCONN:
                        {
                                //NDBG_PRINT("%sCONNECT%s[%3d]: SET CONNECTED\n",
                                //                ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF,
                                //                m_fd);
                                // Ok!
                                m_ssl_state = SSL_STATE_CONNECTED;
                                break;
                        }
                        case EINVAL:
                        {
                                int l_err;
                                socklen_t l_errlen;
                                l_errlen = sizeof(l_err);
                                if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (void*) &l_err, &l_errlen) < 0)
                                {
                                        NDBG_PRINT("%s: unknown connect error\n",
                                                        m_host.c_str());
                                }
                                else
                                {
                                        NDBG_PRINT("%s: %s\n", m_host.c_str(),
                                                        strerror(l_err));
                                }
                                return STATUS_ERROR;
                        }
                        case ECONNREFUSED:
                        {
                                NDBG_PRINT("Error Connection refused for host: %s. Reason: %s\n", m_host.c_str(), strerror(errno));
                                return STATUS_ERROR;
                        }
                        case EAGAIN:
                        case EINPROGRESS:
                        {
                                //NDBG_PRINT("Error Connection in progress. Reason: %s\n", strerror(errno));
                                m_ssl_state = SSL_STATE_CONNECTING;
                                //l_in_progress = true;

                                // Set to writeable and try again
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                        return STATUS_ERROR;
                                }
                                return STATUS_OK;
                        }
                        case EADDRNOTAVAIL:
                        {
                                // TODO -bad to spin like this???
                                // Retry connect
                                //NDBG_PRINT("%sRETRY CONNECT%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
                                if(++l_retry_connect_count < 1024)
                                {
                                        usleep(1000);
                                        goto state_top;
                                }
                                else
                                {
                                        NDBG_PRINT("Error connect() for host: %s.  Reason: %s\n", m_host.c_str(), strerror(errno));
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        default:
                        {
                                NDBG_PRINT("Error Unkown. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                        }
                }

                m_ssl_state = SSL_STATE_CONNECTED;

                // Stats
                if(m_collect_stats_flag)
                {
                        m_stat.m_tt_connect_us = get_delta_time_us(m_connect_start_time_us);

                        // Save last connect time for reuse
                        m_last_connect_time_us = m_stat.m_tt_connect_us;
                }

                //NDBG_PRINT("%s connect: %" PRIu64 " -- start: %" PRIu64 " %s.\n",
                //              ANSI_COLOR_BG_RED,
                //              m_stat.m_tt_connect_us,
                //              m_connect_start_time_us,
                //              ANSI_COLOR_OFF);

                m_ssl_state = SSL_STATE_SSL_CONNECTING;

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: SSL_CONNECTING
        // -------------------------------------------------
        case SSL_STATE_SSL_CONNECTING:
        case SSL_STATE_SSL_CONNECTING_WANT_WRITE:
        case SSL_STATE_SSL_CONNECTING_WANT_READ:
        {
                int l_status;
                //NDBG_PRINT("%sSSL_CONNECTING%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                l_status = ssl_connect(a_host_info);
                if(EAGAIN == l_status)
                {
                        if(SSL_STATE_SSL_CONNECTING_WANT_READ == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                        return STATUS_ERROR;
                                }
                        }
                        else if(SSL_STATE_SSL_CONNECTING_WANT_WRITE == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                        return STATUS_ERROR;
                                }
                        }
                        return STATUS_OK;
                }
                else if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error: performing connect_cb\n");
                        return STATUS_ERROR;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case SSL_STATE_CONNECTED:
        {
                // -------------------------------------------
                // Send request
                // -------------------------------------------
                int32_t l_request_status = STATUS_OK;
                //NDBG_PRINT("%sSEND_REQUEST%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
                l_request_status = send_request(false);
                if(l_request_status != STATUS_OK)
                {
                        NDBG_PRINT("Error: performing send_request\n");
                        return STATUS_ERROR;
                }
                break;
        }

        // -------------------------------------------------
        // STATE: READING
        // -------------------------------------------------
        case SSL_STATE_READING:
        {
                int l_read_status = 0;
                //NDBG_PRINT("%sCNST_READING%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
                l_read_status = receive_response();
                //NDBG_PRINT("%sCNST_READING%s: receive_response(): total: %d\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, (int)m_stat.m_total_bytes);
                if(l_read_status < 0)
                {
                        NDBG_PRINT("Error: performing receive_response\n");
                        return STATUS_ERROR;
                }
                return l_read_status;
        }
        default:
                break;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len)
{

        //NDBG_PRINT("HERE: a_opt: %d a_buf: %p\n", a_opt, a_buf);

        // TODO RUN SUPER
        int32_t l_status;
        l_status = nconn_tcp::set_opt(a_opt, a_buf, a_len);
        if((l_status != STATUS_OK) &&
           (l_status != m_opt_unhandled))
        {
                return STATUS_ERROR;
        }
        if(l_status == STATUS_OK)
        {
                return STATUS_OK;
        }

        switch(a_opt)
        {
        case OPT_SSL_CIPHER_STR:
        {
                //m_cipher_str = a_len;
                break;
        }
        case OPT_SSL_CTX:
        {
                m_ssl_ctx = (SSL_CTX *)a_buf;
                break;
        }
        default:
        {
                //NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return m_opt_unhandled;
        }
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len)
{

        // TODO RUN SUPER
        int32_t l_status;
        l_status = nconn_tcp::get_opt(a_opt, a_buf, a_len);
        if((l_status != STATUS_OK) &&
           (l_status != m_opt_unhandled))
        {
                return STATUS_ERROR;
        }
        if(l_status == STATUS_OK)
        {
                return STATUS_OK;
        }

        switch(a_opt)
        {
        default:
        {
                //NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return m_opt_unhandled;
        }
        }

        return STATUS_OK;
}
