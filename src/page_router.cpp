#include <string.h>
#include "page_router.h"
#include "web_pages.h"

static const Page pages[] = {
  { "/board-edit.html", BOARD_EDIT_HTML_GZ, BOARD_EDIT_HTML_GZ_LEN, "text/html", true },
  { "/board-view.html", BOARD_VIEW_HTML_GZ, BOARD_VIEW_HTML_GZ_LEN, "text/html", true },
  { "/game.html", GAME_HTML_GZ, GAME_HTML_GZ_LEN, "text/html", true },
  { "/", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN, "text/html", true },
  { "/styles.css", STYLES_CSS_GZ, STYLES_CSS_GZ_LEN, "text/css", true },
};

const Page *findPage(const char *path)
{
  size_t plen = strlen(path);

  for (auto &p : pages)
  {
    const char *ppath = p.path;
    size_t flen = strlen(ppath);

    // 1) Exact match: "/foo.js" == "/foo.js"
    if (plen == flen && memcmp(path, ppath, plen) == 0)
      return &p;

    // 2) Extensionless match: "/foo" == "/foo.<ext>"
    // Find '.' in stored path
    const char *dot = strchr(ppath, '.');
    if (!dot)
      continue;

    size_t baseLen = dot - ppath;

    if (plen == baseLen && memcmp(path, ppath, baseLen) == 0)
      return &p;
  }

  return nullptr;
}
