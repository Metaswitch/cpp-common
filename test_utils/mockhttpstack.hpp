/**
 * @file mockhttpstack.h Mock HTTP stack.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
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
