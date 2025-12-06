// #define KOROUTINE_DEBUG
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

// 简单的链接提取，匹配绝对或相对链接 href="http... 或 href="/..."
std::vector<std::string> extract_links(const std::string& html) {
  std::vector<std::string> links;
  std::regex url_regex("href=[\"'](https?://[^\"']+|/[^\"']+)[\"']",
                       std::regex::icase);
  auto words_begin = std::sregex_iterator(html.begin(), html.end(), url_regex);
  auto words_end = std::sregex_iterator();

  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    std::smatch match = *i;
    links.push_back(match.str(1));
  }
  return links;
}

std::string trim_newlines(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\n' || c == '\r' || c == '\t')
      out.push_back(' ');
    else
      out.push_back(c);
  }
  return out;
}

std::string to_lower(const std::string& s) {
  std::string t = s;
  std::transform(t.begin(), t.end(), t.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return t;
}

// path 必须以 '/' 开头 (即相对路径)。base_host 形如 "http://httpbin.org"
Task<void> crawl_page(Client& cli, const std::string& base_host,
                      std::string path, int depth,
                      std::set<std::string>& visited) {
  const int MAX_DEPTH = 4;
  const int MAX_LINKS_PER_PAGE = 10;

  if (depth > MAX_DEPTH) co_return;
  if (visited.count(path)) co_return;
  visited.insert(path);

  std::string indent(depth * 2, ' ');
  std::cout << indent << "[Depth " << depth << "] Fetching: " << path
            << std::endl;

  auto start = std::chrono::steady_clock::now();
  auto res = co_await cli.Get(path.c_str());
  auto end = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

  std::cout << indent << "[Depth " << depth << "] Got response (" << ms
            << " ms)" << std::endl;

  if (res) {
    std::cout << indent << "  -> Status: " << res->status
              << ", Size: " << res->body.size() << " bytes" << std::endl;

    // 尝试获取 Content-Type（不区分大小写）
    std::string content_type;
    for (const auto& h : res->headers) {
      if (to_lower(h.first) == "content-type") {
        content_type = h.second;
        break;
      }
    }
    if (!content_type.empty()) {
      std::cout << indent << "  -> Content-Type: " << content_type << std::endl;
    }

    // 提取<title>（宽松匹配，包括跨行）
    std::string title;
    {
      std::regex title_re("<title>([\\s\\S]*?)</title>", std::regex::icase);
      std::smatch m;
      if (std::regex_search(res->body, m, title_re) && m.size() >= 2) {
        title = trim_newlines(m.str(1));
        // 截断防止太长
        if (title.size() > 200) title = title.substr(0, 200) + "...";
      }
    }
    if (!title.empty()) {
      std::cout << indent << "  -> Title: " << title << std::endl;
    }

    // 输出正文摘要
    if (!res->body.empty()) {
      std::string snippet = trim_newlines(
          res->body.substr(0, std::min<size_t>(200, res->body.size())));
      if (res->body.size() > 200) snippet += "...";
      std::cout << indent << "  -> Snippet: " << snippet << std::endl;
    }

    if (res->status == 200) {
      auto links = extract_links(res->body);
      std::vector<std::string> filtered_links;
      filtered_links.reserve(links.size());

      // 规范化：将相对路径转换为带 host 的绝对路径，并且只爬取同一主机的链接
      for (const auto& link : links) {
        if (link.empty()) continue;
        std::string full;
        if (link[0] == '/') {
          full = base_host + link;
        } else if (link.rfind("http://", 0) == 0 ||
                   link.rfind("https://", 0) == 0) {
          // 只保留同 host 的绝对链接
          if (link.rfind(base_host, 0) == 0) {
            full = link;
          } else {
            continue;
          }
        } else {
          // 忽略其他相对或协议相对的链接
          continue;
        }

        // 将 full 转换为 path 部分（去掉 scheme+host）
        if (full.size() > base_host.size() && full.rfind(base_host, 0) == 0) {
          std::string only_path = full.substr(base_host.size());
          if (only_path.empty()) only_path = "/";
          filtered_links.push_back(only_path);
        }
      }

      if (!filtered_links.empty()) {
        std::cout << indent << "  -> Found " << filtered_links.size()
                  << " same-host links." << std::endl;

        // 展示前几个完整链接
        int show_count =
            std::min<int>(filtered_links.size(), MAX_LINKS_PER_PAGE);
        for (int i = 0; i < show_count; ++i) {
          std::cout << indent << "     [" << i + 1 << "] " << base_host
                    << filtered_links[i] << std::endl;
        }

        // 递归爬取（限制数量）
        int count = 0;
        for (const auto& link_path : filtered_links) {
          if (count++ >= MAX_LINKS_PER_PAGE) break;
          co_await crawl_page(cli, base_host, link_path, depth + 1, visited);
        }
      }
    }
  } else {
    std::cout << indent << "  -> Error: " << to_string(res.error())
              << std::endl;
  }
}

Task<void> run_crawler() {
  // 使用 httpbin.org 进行测试
  std::string base_host = "http://httpbin.org";
  Client cli(base_host);
  cli.set_address_family(AF_INET);

  // 设置超时（秒）
  cli.set_connection_timeout(10);
  cli.set_read_timeout(10);

  std::cout << "Starting crawler on " << base_host << "..." << std::endl;

  std::set<std::string> visited;
  // 从 /links/5/0 开始
  co_await crawl_page(cli, base_host, "/links/5/0", 1, visited);

  std::cout << "Crawling finished. Visited " << visited.size() << " paths."
            << std::endl;
}

int main() {
  debug::set_level(debug::Level::Debug);
  debug::set_detail_flags(debug::Detail::Level | debug::Detail::Timestamp |
                          debug::Detail::ThreadId | debug::Detail::FileLine);
  Runtime::block_on(run_crawler());
  return 0;
}
