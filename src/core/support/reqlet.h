//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    reqlet.h
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
#ifndef _REQLET_H
#define _REQLET_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx_client.h"
#include "parsed_url.h"
#include "evr.h"
#include "ndebug.h"
#include "host_info.h"
#include "req_stat.h"

#include <math.h>

#include <map>
#include <vector>

// Sockets...
#include <netinet/in.h>

//: ----------------------------------------------------------------------------
//: Fwd' Decls
//: ----------------------------------------------------------------------------
namespace ns_hlx {

class conn;
class resolver;

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
// Wildcard support
typedef enum path_order_enum {

        EXPLODED_PATH_ORDER_SEQUENTIAL = 0,
        EXPLODED_PATH_ORDER_RANDOM

} path_order_t;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector <std::string> path_vector_t;
typedef std::map<std::string, std::string> header_map_t;
typedef std::list <std::string> value_list_t;
typedef std::map<std::string, value_list_t> kv_list_map_t;

typedef struct range_struct {
        uint32_t m_start;
        uint32_t m_end;
} range_t;

typedef std::vector <std::string> path_substr_vector_t;
typedef std::vector <range_t> range_vector_t;

//: ----------------------------------------------------------------------------
//: \details: reqlet
//: ----------------------------------------------------------------------------
class reqlet
{
public:
        // -------------------------------------------
        // Public methods
        // -------------------------------------------
        reqlet(uint64_t a_id, int64_t a_num_to_req = 1):
                m_url(),
                m_host_info(),
                m_stat_agg(),
                m_headers(),
                m_next_value(m_headers.begin()),
                m_body(""),
                m_status(0),
                m_conn_info(),
                m_multipath(false),
                m_id(a_id),
                m_is_resolved_flag(false),
                m_num_to_req(a_num_to_req),
                m_num_reqd(0),
                m_repeat_path_flag(false),
                m_path_vector(),
                m_path_order(EXPLODED_PATH_ORDER_SEQUENTIAL),
                m_path_vector_last_idx(0),
                m_label("UNDEFINED"),
                m_label_hash(0)
        {};
        reqlet(const reqlet &a_reqlet);
        ~reqlet() {};
        int32_t init_with_url(const std::string &a_url, bool a_wildcarding=false);
        uint64_t get_id(void) {return m_id;}
        bool is_resolved(void) {return m_is_resolved_flag;}
        int32_t resolve(resolver &a_resolver);
        void show_host_info(void);
        bool is_done(void) {
                if((m_num_to_req == -1)|| m_num_reqd < (uint64_t)m_num_to_req) return false;
                else return true;
        }
        void reset(void) {
                m_num_reqd = 0;
        }
        void bump_num_requested(void) {++m_num_reqd;}
        const std::string &get_path(void *a_rand);
        const std::string &get_label(void);
        uint64_t get_label_hash(void);
        void set_host(const std::string &a_host);
        void set_port(uint16_t &a_port) { m_url.m_port = a_port;}
        void set_response(uint16_t a_response_status, const char *a_response);
        void set_id(uint64_t a_id) {m_id = a_id;};

        // TODO Cookies!!!
        void set_uri(const std::string &a_uri);
        const kv_list_map_t &get_uri_decoded_query(void);

        // -------------------------------------------
        // Public members
        // -------------------------------------------
        parsed_url m_url;
        host_info_t m_host_info;
        t_stat_t m_stat_agg;

        // TODO Move headers to kv list type...
        header_map_t m_headers;
        header_map_t::iterator m_next_value;
        std::string m_body;
        uint16_t m_status;
        header_map_t m_conn_info;
        bool m_multipath;

        // -------------------------------------------
        // Class methods
        // -------------------------------------------

private:
        // -------------------------------------------
        // Private methods
        // -------------------------------------------
        //DISALLOW_COPY_AND_ASSIGN(reqlet)


        int32_t special_effects_parse(void);
        int32_t add_option(const char *a_key, const char *a_val);
        int32_t parse_path(const char *a_path, path_substr_vector_t &ao_substr_vector, range_vector_t &ao_range_vector);
        int32_t path_exploder(std::string a_path_part,
                                                  const path_substr_vector_t &a_substr_vector, uint32_t a_substr_idx,
                                                  const range_vector_t &a_range_vector, uint32_t a_range_idx);

        // -------------------------------------------
        // Private members
        // -------------------------------------------
        // Unique id
        uint64_t m_id;
        bool m_is_resolved_flag;

        // State
        int64_t m_num_to_req;
        uint64_t m_num_reqd;

        // -------------------------------------------
        // Special effects...
        // Supports urls like:
        // http://127.0.0.1:80/[0X000001-0X00000A]/100K/[1-1000]/[1-100].gif;order=random
        // -------------------------------------------
        // Wildcards
        bool m_repeat_path_flag;
        path_vector_t m_path_vector;

        // Meta
        // order=random
        path_order_t m_path_order;

        uint32_t m_path_vector_last_idx;

        std::string m_label;
        uint64_t m_label_hash;


#if 0
        std::string m_uri;
        std::string m_path;
        kv_list_map_t m_query;
        kv_list_map_t m_headers;
        std::string m_body;
        kv_list_map_t m_query_uri_decoded;
#endif

        // -------------------------------------------
        // Class members
        // -------------------------------------------

};

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
#if 0
std::string uri_decode(const std::string & a_src);
std::string uri_encode(const std::string & a_src);
#endif

} // ns_hlx

#endif
