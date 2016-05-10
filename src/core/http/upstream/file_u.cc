//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    file_u.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/29/2015
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
#include "nbq.h"
#include "ndebug.h"
#include "clnt_session.h"
#include "hlx/status.h"
#include "hlx/trace.h"
#include "hlx/file_u.h"
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_u::file_u(clnt_session &a_clnt_session):
        base_u(a_clnt_session),
        m_fd(-1),
        m_size(0),
        m_read(0)
{
}

//: ----------------------------------------------------------------------------
//: \details: Destructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
file_u::~file_u()
{
        if(m_fd > 0)
        {
                close(m_fd);
                m_fd = -1;
        }
}

//: ----------------------------------------------------------------------------
//: \details: Setup file for reading
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_u::fsinit(const char *a_filename)
{
        // ---------------------------------------
        // Check is a file
        // TODO
        // ---------------------------------------
        struct stat l_stat;
        int32_t l_status = HLX_STATUS_OK;
        l_status = stat(a_filename, &l_stat);
        if(l_status != 0)
        {
                TRC_ERROR("performing stat on file: %s.  Reason: %s\n", a_filename, strerror(errno));
                return HLX_STATUS_ERROR;
        }

        // Check if is regular file
        if(!(l_stat.st_mode & S_IFREG))
        {
                TRC_ERROR("opening file: %s.  Reason: is NOT a regular file\n", a_filename);
                return HLX_STATUS_ERROR;
        }

        // Set size
        m_size = l_stat.st_size;

        // Open the file
        m_fd = open(a_filename, O_RDONLY);
        if (m_fd < 0)
        {
                TRC_ERROR("performing open on file: %s.  Reason: %s\n", a_filename, strerror(errno));
                return HLX_STATUS_ERROR;
        }

        // Start sending it
        m_read = 0;
        m_state = UPS_STATE_SENDING;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Read part from file
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
ssize_t file_u::ups_read(char *ao_dst, size_t a_len)
{
        // Get one chunk of the file from disk
        ssize_t l_read = 0;
        l_read = read(m_fd, (void *)ao_dst, a_len);
        if (l_read == 0)
        {
                // All done; close the file.
                close(m_fd);
                m_fd = 0;
                m_state = DONE;
                return 0;
        }
        else if(l_read < 0)
        {
                //NDBG_PRINT("Error performing read. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        if(l_read > 0)
        {
                m_read += l_read;
        }
        return l_read;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: Continue sending the file started by sendFile().
//:           Call this periodically. Returns nonzero when done.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ssize_t file_u::ups_read(size_t a_len)
{
        if(m_fd < 0)
        {
                return 0;
        }
        if(!(m_clnt_session.m_out_q))
        {
                TRC_ERROR("m_clnt_session->m_out_q == NULL\n");
                return HLX_STATUS_ERROR;
        }
        // Get one chunk of the file from disk
        ssize_t l_read = 0;
        l_read = m_clnt_session.m_out_q->write_fd(m_fd, a_len);
        if(l_read < 0)
        {
                TRC_ERROR("performing read. Reason: %s\n", strerror(errno));
                return HLX_STATUS_ERROR;
        }
        else if(l_read >= 0)
        {
                m_read += l_read;
                //NDBG_PRINT("READ: B %9ld / %9lu / %9lu\n", a_len, m_read, m_size);
                if ((size_t)l_read < a_len)
                {
                        // All done; close the file.
                        close(m_fd);
                        m_fd = -1;
                        m_state = UPS_STATE_DONE;
                        return 0;
                }
        }
        return l_read;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel and cleanup
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t file_u::ups_cancel(void)
{
        if(m_fd > 0)
        {
                close(m_fd);
                m_state = UPS_STATE_DONE;
                m_fd = -1;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Get path
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t get_path(const std::string &a_root,
                 const std::string &a_index,
                 const std::string &a_route,
                 const std::string &a_url_path,
                 std::string &ao_path)
{
        // TODO Path normalization and for recursing... "../"
        if(a_root.empty())
        {
                ao_path = "./";
        }
        else
        {
                ao_path = a_root;
        }
        if(ao_path[ao_path.length() - 1] != '/')
        {
                ao_path += '/';
        }
        std::string l_route = a_route;
        if(a_route[a_route.length() - 1] == '*')
        {
                l_route = a_route.substr(0, a_route.length() - 2);
        }
        std::string l_url_path;
        if(!l_route.empty() &&
           (a_url_path.find(l_route, 0) != std::string::npos))
        {
                l_url_path = a_url_path.substr(l_route.length(), a_url_path.length() - l_route.length());
        }
        else
        {
                l_url_path = a_url_path;
        }
        if(!a_index.empty() &&
           (!l_url_path.length() ||
           (l_url_path.length() == 1 && l_url_path[0] == '/')))
        {
                ao_path += a_index;
        }
        else
        {
                if(l_url_path[0] == '/')
                {
                        ao_path += l_url_path.substr(1, l_url_path.length() - 1);
                }
                else
                {
                        ao_path += l_url_path;
                }
        }
        return HLX_STATUS_OK;
}

}
