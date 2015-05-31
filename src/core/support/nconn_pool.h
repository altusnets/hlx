//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.h
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
#ifndef _NCONN_POOL_H
#define _NCONN_POOL_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"
#include "reqlet.h"
#include "ncache.h"
#include "settings.h"

#include <list>
#include <unordered_set>
#include <unordered_map>
#include <vector>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;
typedef ncache <nconn *> idle_conn_ncache_t;

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn_pool
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nconn_pool(uint32_t a_size);
        ~nconn_pool();
        // TODO passing settings struct -readonly reference
        nconn *get(reqlet *a_reqlet,
                   const settings_struct_t &a_settings);
        int32_t add_idle(nconn *a_nconn);
        int32_t release(nconn *a_nconn);

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int delete_cb(void* o_1, void *a_2);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_pool);
        void init(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_free_list;
        conn_id_set_t m_conn_used_set;
        idle_conn_ncache_t m_idle_conn_ncache;
        bool m_initd;

protected:
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------

};

}


#endif






