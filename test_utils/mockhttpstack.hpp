/**
 * @file mockhttpstack.h Mock HTTP stack.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#ifndef MOCKHTTPSTACK_H__
#define MOCKHTTPSTACK_H__

#include "gmock/gmock.h"
#include "httpstack.h"

class MockHttpStack : public HttpStack
{
public:
  class Request : public HttpStack::Request
  {
  public:
    Request(HttpStack* stack, std::string path, std::string file, std::string query = "", std::string body = "", htp_method method = htp_method_GET) : HttpStack::Request(stack, evhtp_request(path, file, query))
    {
      _rx_body = body;
      _method = method;
      _rx_body_set = true;
    }
    ~Request()
    {
      evhtp_connection_t* conn = _req->conn;
      evhtp_request_free(_req);
      free(conn);
    }
    std::string content()
    {
      size_t len = evbuffer_get_length(_req->buffer_out);
      return std::string((char*)evbuffer_pullup(_req->buffer_out, len), len);
    }
    std::string response_header(const std::string& name)
    {
      const char* val = evhtp_header_find(_req->headers_out, name.c_str());
      return std::string((val != NULL ? val : ""));
    }
    void add_header_to_incoming_req(const std::string& name,
                                    const std::string& value)
    {
      // This takes the name and value for the new header. These
      // are copied to avoid memory scribblers (controlled by the 1, 1
      // parameters)
      evhtp_header_t* new_header = evhtp_header_new(name.c_str(),
                                                    value.c_str(),
                                                    1, 1);
      evhtp_headers_add_header(_req->headers_in, new_header);
    }

  private:
    static evhtp_request_t* evhtp_request(std::string path, std::string file, std::string query = "")
    {
      evhtp_request_t* req = evhtp_request_new(NULL, NULL);
      req->conn = (evhtp_connection_t*)calloc(sizeof(evhtp_connection_t), 1);
      req->uri = (evhtp_uri_t*)calloc(sizeof(evhtp_uri_t), 1);
      req->uri->path = (evhtp_path_t*)calloc(sizeof(evhtp_path_t), 1);
      req->uri->path->full = strdup((path + file).c_str());
      req->uri->path->file = strdup(file.c_str());
      req->uri->path->path = strdup(path.c_str());
      req->uri->path->match_start = (char*)calloc(1, 1);
      req->uri->path->match_end = (char*)calloc(1, 1);
      req->uri->query = evhtp_parse_query(query.c_str(), query.length());
      return req;
    }
  };

  MockHttpStack() : HttpStack(1, nullptr, nullptr, nullptr, nullptr) {}

  MOCK_METHOD0(initialize, void());
  MOCK_METHOD2(bind_tcp_socket, void(const std::string& bind_address,
                                     unsigned short port));
  MOCK_METHOD1(bind_unix_socket, void(const std::string& bind_path));
  MOCK_METHOD2(register_handler, void(const char*, HandlerInterface*));
  MOCK_METHOD1(start, void(evhtp_thread_init_cb));
  MOCK_METHOD0(stop, void());
  MOCK_METHOD0(wait_stopped, void());
  MOCK_METHOD3(send_reply, void(HttpStack::Request&, int, SAS::TrailId));
  MOCK_METHOD0(record_penalty, void());
};

#endif
