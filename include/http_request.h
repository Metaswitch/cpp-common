/**
 * @file http_request.h Definitions for HttpRequest class.
 *
 * Copyright (C) Metaswitch Networks 2018
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

class HttpRequest
{
public:
  HttpRequest(std::string server,
              std::string scheme,
              HttpClient* client,
              std::string path);
  virtual ~HttpRequest();


  // SET methods will overwrite any previous settings
  virtual void set_req_body(std::string body);
  virtual void set_req_headers(std::string req_header);
  virtual void set_sas_trail(SAS::TrailId trail);
  virtual void set_allowed_host_state(int allowed_host_state);
  virtual void set_username(std::string username); //Unclear if we ever actually use this atm, may be unnecessary

  // Sends the request and populates ret code, recv headers, and recv body
  virtual void send(RequestType request_type);

  // GET methods return empty if not set yet
  virtual HTTPCode get_return_code(); 
  virtual std::string get_recv_body();
  virtual std::map<std::string, std::string> get_recv_headers();

private:
  // member variables for storing the request information pre and post send
  std::string _server;
  std::string _scheme;
  std::string _req_url_tail;
  std::string _req_body;
  std::vector<std::string> _req_headers;
  HTTPCode _return_code;
  std::string _recv_body;
  std::map<std::string, std::string> _recv_headers;
  SAS::TrailId _trail;
  int _allowed_host_state = BaseResolver::ALL_LISTS;
};
