#include <uv.h>
#include <mysql.h>
#include <memory.h>
#include <string>
#include <map>
#include <iostream>
#include <vector>
#include <sstream>

#include "http-parser/http_parser.h"

const char *mysql_address = "192.168.33.20";
const char *mysql_user = "root";
const char *mysql_password = "password";
const char *mysql_database = "";
const int   mysqld_port = 3306;

const int httpd_port = 3380;

namespace{
  uv_loop_t uv;
  uv_tcp_t httpd;
  bool halt = false;
  bool monitor = true;
}

int main()
{
  using namespace std;
  typedef map<string, string> KeyValue;
  KeyValue slave_status;
  uv_loop_init(&uv);
  uv_timer_t ask_mysql;
  uv_timer_init(&uv, &ask_mysql);
  uv_timer_start(&ask_mysql, [](uv_timer_t*ctx){
    MYSQL *sql = mysql_init(0);
    bool error = false;
    if (mysql_real_connect(sql, mysql_address, mysql_user, mysql_password, mysql_database, mysqld_port, 0, 0))
    {
      error = mysql_query(sql, "set names utf8");
      error || mysql_character_set_name(sql);
      error = error || mysql_query(sql, "show global variables");
      if (MYSQL_RES *res = mysql_store_result(sql))mysql_free_result(res);
      else error = true;
      error = error || mysql_query(sql, "show slave status");
      if (MYSQL_RES *res = mysql_store_result(sql))mysql_free_result(res);
      else error = true;
      error = error || mysql_query(sql, "show databases");
      if (MYSQL_RES *res = mysql_store_result(sql))mysql_free_result(res);
      else error = true;
    }
    else{
      cout << "cant connect mysql " << mysql_error(sql) << endl;
      *reinterpret_cast<bool*>(ctx->data) = false;
    }
    if (error){
      cout << "has mysql error!" << endl;
    }
    halt = (halt || error) && monitor;
    mysql_close(sql);
    if (halt){
      uv_timer_stop(ctx);
      uv_close(reinterpret_cast<uv_handle_t*>(&httpd), [](uv_handle_t*){});
    }
  }, 0, 1000);
  sockaddr_in addr;
  uv_ip4_addr("0.0.0.0", httpd_port, &addr);
  if (uv_tcp_init(&uv, &httpd) ||
    uv_tcp_bind(&httpd, (const struct sockaddr*) &addr, 0) ||
    uv_tcp_simultaneous_accepts((uv_tcp_t*)&httpd, 1) ||
    uv_listen((uv_stream_t*)&httpd, SOMAXCONN, [](uv_stream_t* listener, int status){
    uv_stream_t *stream = new uv_stream_t;
    if (uv_tcp_init(&uv, reinterpret_cast<uv_tcp_t*>(stream)) ||
      uv_tcp_simultaneous_accepts(reinterpret_cast<uv_tcp_t*>(stream), 1) ||
      uv_accept(listener, stream) ||
      uv_tcp_nodelay(reinterpret_cast<uv_tcp_t*>(stream), 1) ||
      uv_read_start(stream, [](uv_handle_t*, size_t suggested_size, uv_buf_t* buf){
      buf->base = new char[suggested_size];
      buf->len = suggested_size;
    }, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
      if (nread < 0) { //error
        uv_close(reinterpret_cast<uv_handle_t*>(stream), [](uv_handle_t* handle){
        });
      }
      else if (0 == nread){ //remote close
      }
      else{ //good
        string str(buf->base, buf->base + nread);
        //cout << str;
        static const http_parser_settings settings = {
          NULL, [](http_parser *parser, const char *at, size_t length)->int{ //url
            const string uri(at, at + length);
            bool halt_confirm = false;
            if (uri == "/halt"){
              cout << "halt request" << endl;
              halt = true;
            }
            else if (uri == "/halt_confirm"){
              halt_confirm = true;
            }
            else if (uri == "/ignore"){
              monitor = false;
            }
            else if (uri == "/monitor"){
              monitor = true;
            }
            uv_write_t *ctx = new uv_write_t;
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "\r\n"
              "<a href = \"/halt_confirm\">halt</a><br/>"
              "<a href = \"/ignore\">ignore any error</a><br/>"
              "<a href = \"/monitor\">normal monitoring</a><br/>"
              "<br/>"
              "monitoring : " << (monitor ? "on" : "OFF") << "<br/>"
              << (halt_confirm ? "<a href = \"/halt\">CONFIRM HALT</a><br/>" : "")
              ;
            uv_buf_t buf;
            buf.len = res.str().length();
            buf.base = new char[buf.len];
            ctx->data = buf.base;
            memcpy(buf.base, res.str().data(), buf.len);
            uv_write(ctx, reinterpret_cast<uv_stream_t*>(parser->data), &buf, 1, [](uv_write_t *ctx, int status){
              delete[]reinterpret_cast<char*>(ctx->data);
              uv_shutdown_t *st = new uv_shutdown_t;
              uv_shutdown(st, ctx->handle, [](uv_shutdown_t *st, int status){
                uv_close(reinterpret_cast<uv_handle_t*>(st->handle), [](uv_handle_t* handle){
                  delete reinterpret_cast<http_parser*>(handle->data);
                  delete reinterpret_cast<uv_stream_t*>(handle);
                });
                delete st;
              });
              delete ctx;
            });
            return length;
          }, NULL, NULL, NULL, NULL, NULL, NULL
        };
        http_parser_execute(reinterpret_cast<http_parser*>(stream->data), &settings, buf->base, nread);
      }
      delete[]buf->base;
    }))
    {
      //setup error
      uv_close(reinterpret_cast<uv_handle_t*>(stream), [](uv_handle_t* handle){
      });
    }
    else{
      http_parser *parser = new http_parser;
      http_parser_init(parser, HTTP_REQUEST);
      stream->data = parser;
      parser->data = stream;
    }
  }))
  {
    cout << "listen error" << endl;;
    return -1;
  }
  uv_run(&uv, UV_RUN_DEFAULT);
  return 0;
}