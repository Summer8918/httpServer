## 工作流程
（1） 服务器启动，在指定端口绑定 httpd 服务。           

（2）收到一个 HTTP 请求时（listen 的端口 accpet 时），将 accept_request任务放入线程池中。

（3）继续listen端口

 处理请求：
（1）取出 HTTP 请求中的 method (GET 或 POST) 和 url。对于 GET 方法，如果有携带参数，则 query_string 指针指向 url 中 ？ 后面的 GET 参数。

（2） 格式化 url 到 path 数组，表示浏览器请求的服务器文件路径，在 tinyhttpd 中服务器文件是在 htdocs 文件夹下。当 url 以 / 结尾，或 url 是个目录，则默认在 path 中加上 index.html，表示访问主页。

（3）如果文件路径合法，对于无参数的 GET 请求，直接输出服务器文件到浏览器，即用 HTTP 格式写到套接字上。其他情况（带参数 GET，POST 方式，url 为可执行文件），则调用 excute_cgi 函数执行 cgi 脚本。

（4）读取整个 HTTP 请求并丢弃，如果是 POST 则找出 Content-Length. 把 HTTP 200  状态码写到套接字。

（5） 建立两个管道，cgi_input 和 cgi_output, 并 fork 创建一个新进程。

（6） 在子进程中，把 STDOUT 重定向到 cgi_output 的写入端，把 STDIN 重定向到 cgi_input 的读取端，关闭 cgi_input 的写入端 和 cgi_output 的读取端，设置 request_method 的环境变量，GET 的话设置 query_string 的环境变量，POST 的话设置 content_length 的环境变量，这些环境变量都是为了给 cgi 脚本调用，接着用 execl 运行 cgi 程序。

（7） 在父进程中，关闭 cgi_input 的读取端 和 cgi_output 的写入端，如果 POST 的话，把 POST 数据写入 cgi_input，已被重定向到 STDIN，读取 cgi_output 的管道发送到客户端，该管道输入是 STDOUT。接着关闭所有管道，等待子进程结束。管道数据流如下图示：

![image](https://github.com/Summer8918/httpServer/blob/master/images/%E7%88%B6%E5%AD%90%E8%BF%9B%E7%A8%8B%E7%AE%A1%E9%81%93%E9%80%9A%E4%BF%A1.png)
## 函数
startup: 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。

accept_request: 处理从套接字上监听到的一个 HTTP 请求，在这里可以很大一部分地体现服务器处理请求流程。

get_line: 读取套接字的一行，把回车换行等情况都统一为换行符结束。

sever_file: 调用headers把HTTP响应的头部写到套接字，调用 cat 把服务器文件返回给浏览器。

headers: 把 HTTP 响应的头部写到套接字。

execute_cgi: 运行 cgi 程序的处理，也是个主要函数。

bad_request: 返回给客户端这是个错误请求，HTTP 状态码 400 BAD REQUEST.

cannot_execute: 主要处理发生在执行 cgi 程序时出现的错误。

unimplemented: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。
