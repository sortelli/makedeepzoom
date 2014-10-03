/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Joe Sortelli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wand/MagickWand.h>

#define DIR_MODE    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
#define DZI_DIR_SFX "_files"

#define debug(fmt, ...) if (OPT_DEBUG) _debug(fmt, __VA_ARGS__)
#define error(fmt, ...) _error(__LINE__, fmt, __VA_ARGS__)
#define wand_error(wand) _wand_error(__LINE__, wand)
#define CHECK_WAND(wand, x) if (x == MagickFalse) wand_error(wand)

void _wand_error(long line, MagickWand *wand);
void _error(long  line, char *fmt, ...);
void _debug(char *fmt, ...);

typedef struct {
  char  *format;
  char  *source;
  char  *xml_path;
  char  *files_path;
  size_t width;
  size_t height;
  size_t cur_width;
  size_t cur_height;
  int    levels;
  int    cur_level;
  int    tile_size;
  int    overlap;
  MagickWand *wand;
} DZI;

typedef struct {
  char *format;
  char *xml_path;
  char *files_path;
  int   levels;
  int   tile_size;
  int   next_item;
  int   morton_row;
  int   morton_col;
  FILE *tmp;
} DZC;

DZI *dzi_new(char *source, char *out_dir, int tile_size, int overlap, char *format);
void dzi_destory(DZI *dzi);
void dzi_make_tiles(DZI *dzi, DZC *dzc);
void dzi_make_xml(DZI *dzi);
void dzi_reduce(DZI *dzi);

DZC *dzc_new(char *xml_path, int tile_size, char *format, int max_levels);
void dzc_start_update(DZC *dzc);
void dzc_add_dzi(DZC *dzc, DZI *dzi);
void dzc_save(DZC *dzc);
void dzc_destory(DZC *dzc);

char *basename(char *path);
void make_dir(char *dir);

void morton(int n, int *row, int *col);
char *dzi_dir(char *dzi_file);
int dzi_zoom_depth(int width, int height);

void change_aspect(double aspect, MagickWand *wand);

int OPT_DEBUG     = 0;
int OPT_XML_EXT   = 0;
int OPT_DZC_DEPTH = 8;
int OPT_DZC_START = 0;
int OPT_TILE_SIZE = 256;
int OPT_OVERLAP   = 1;
double OPT_ASPECT = 0;
char *OPT_FORMAT  = "jpg";
char *OPT_DZC     = NULL;

char *APP_NAME = NULL;

void set_app_name(char *argv0) {
  APP_NAME = argv0;

  if (APP_NAME && (argv0 = strrchr(APP_NAME, '/')))
    APP_NAME = argv0 + 1;
}

void usage(void) {
  fprintf(stderr, "usage: %s [OPTIONS] [image, image, ...] \n", APP_NAME);
}

int main(int argc, char **argv) {
  int opt;
  DZC *dzc = NULL;

  set_app_name(*argv);

  while ((opt = getopt(argc, argv, "xdc:t:a:f:n:m:o:")) != -1) {
    switch(opt) {
      case 'x': OPT_XML_EXT   = 1;            break;
      case 'd': OPT_DEBUG     = 1;            break;
      case 'c': OPT_DZC       = optarg;       break;
      case 't': OPT_TILE_SIZE = atoi(optarg); break;
      case 'a': OPT_ASPECT    = atof(optarg); break;
      case 'f': OPT_FORMAT    = optarg;       break;
      case 'n': OPT_DZC_START = atoi(optarg); break;
      case 'm': OPT_DZC_DEPTH = atoi(optarg); break;
      case 'o': OPT_OVERLAP   = atoi(optarg); break;

      case 'h':
        usage();
        exit(0);

      default:
        usage();
        exit(1);
    }
  }

  debug("OPT_XML_EXT   = %d\n",   OPT_XML_EXT);
  debug("OPT_DEBUG     = %d\n",   OPT_DEBUG);
  debug("OPT_TILE_SIZE = %d\n",   OPT_TILE_SIZE);
  debug("OPT_DZC_START = %d\n",   OPT_DZC_START);
  debug("OPT_DZC_DEPTH = %d\n",   OPT_DZC_DEPTH);
  debug("OPT_OVERLAP   = %d\n",   OPT_OVERLAP);
  debug("OPT_DZC       = %s\n",   OPT_DZC    ? OPT_DZC    : "(NULL)");
  debug("OPT_FORMAT    = %s\n",   OPT_FORMAT ? OPT_FORMAT : "(NULL)");
  debug("OPT_ASPECT    = %.3f\n", OPT_ASPECT);

  MagickWandGenesis();

  if (OPT_DZC) {
    dzc = dzc_new(OPT_DZC, 256, OPT_FORMAT, OPT_DZC_DEPTH);
    dzc_start_update(dzc);
  }

  for (; optind < argc; ++optind) {
    DZI *dzi = dzi_new(argv[optind], NULL, OPT_TILE_SIZE, OPT_OVERLAP, OPT_FORMAT);

    dzi_make_tiles(dzi, dzc);
    dzi_make_xml(dzi);

    dzi_destory(dzi);
  }

  if (dzc) {
    dzc_save(dzc);
    dzc_destory(dzc);
  }

  MagickWandTerminus();

  return 0;
}

char *basename(char *path) {
  char *p;

  return (p = strrchr(path, '/')) ? p + 1 : path;
}

DZC *dzc_new(char *xml_path, int tile_size, char *format, int max_levels) {
  DZC *dzc;
  char *p, *q;
  char *sfx = DZI_DIR_SFX;
  size_t len;

  if (!(dzc = malloc(sizeof(DZC))))
    error("malloc() failed, reason: %s\n", strerror(errno));

  if (!(dzc->xml_path = strdup(xml_path)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  p = basename(dzc->xml_path);

  len = strlen(dzc->xml_path) + strlen(sfx) + 1;

  if (!(dzc->files_path = malloc(len)))
    error("malloc() failed, reason: %s\n", strerror(errno));

  if ((q = strrchr(p, '.')))
    *q = '\0';

  snprintf(dzc->files_path, len, "%s%s", dzc->xml_path, sfx);

  if (q)
    *q = '.';

  dzc->tile_size  = tile_size;
  dzc->levels     = max_levels;
  dzc->tmp        = NULL;
  dzc->next_item  = 0;
  dzc->morton_row = 0;
  dzc->morton_col = 0;

  if (!(dzc->format = strdup(format)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  return dzc;
}

void dzc_destory(DZC *dzc) {
  if (!dzc)
    return;

  if (dzc->format)     free(dzc->format);
  if (dzc->xml_path)   free(dzc->xml_path);
  if (dzc->files_path) free(dzc->files_path);

  if (dzc->tmp)
    fclose(dzc->tmp);

  free(dzc);
}

void dzc_start_update(DZC *dzc) {
  char dir[1024];
  int i;

  if (!(dzc->tmp = tmpfile()))
    error("tmpfile() failed, reason: %s\n", strerror(errno));

  dzc->next_item = OPT_DZC_START;

  make_dir(dzc->files_path);

  for (i = 0; i <= dzc->levels; ++i) {
    snprintf(dir, sizeof dir, "%s/%d", dzc->files_path, i);
    make_dir(dir);
  }
}

void dzc_add_dzi(DZC *dzc, DZI *dzi) {
  fprintf(dzc->tmp, " <I Id=\"%d\" N=\"%d\" Source=\"%s\">\n",
          dzc->next_item, dzc->next_item, dzi->xml_path);
  fprintf(dzc->tmp, "  <Size Width=\"%zu\" Height=\"%zu\" />\n",
          dzi->width, dzi->height);
  fprintf(dzc->tmp, " </I>\n");

  morton(dzc->next_item, &(dzc->morton_row), &(dzc->morton_col));

  dzc->next_item++;
}

void dzc_make_tiles(DZC *dzc, DZI *dzi) {
  char file[1536];
  int row, col, x, y;
  int imgs_per_tile;
  double img_size;
  MagickWand *wand;

  if (dzi->cur_level > dzc->levels)
    return;

  img_size      = pow(2, dzi->cur_level);
  imgs_per_tile = dzc->tile_size / img_size;

  col = dzc->morton_col / imgs_per_tile;
  row = dzc->morton_row / imgs_per_tile;

  snprintf(file, sizeof file, "%s/%d/%d_%d.%s",
           dzc->files_path, dzi->cur_level, col, row, dzc->format);

  wand = NewMagickWand();

  if (MagickReadImage(wand, file) == MagickFalse) {
    /* Create tile */
    PixelWand *bg;

    bg = NewPixelWand();

    PixelSetColor(bg, "black");

    debug("creating %s\n", file);

    CHECK_WAND(wand, MagickNewImage(wand, dzc->tile_size, dzc->tile_size, bg));

    DestroyPixelWand(bg);
  }

  x = (dzc->morton_col % imgs_per_tile) * img_size;
  y = (dzc->morton_row % imgs_per_tile) * img_size;

  debug("adding to %s at %dx%d\n", file, x, y);

  CHECK_WAND(wand, MagickCompositeImage(wand, dzi->wand, OverCompositeOp, x, y));
  CHECK_WAND(wand, MagickWriteImage(wand, file));

  DestroyMagickWand(wand);
}

void dzc_save(DZC *dzc) {
  FILE *f;
  char buf[8192];

  debug("writing %s\n", dzc->xml_path);

  if (!(f = fopen(dzc->xml_path, "w")))
    error("fopen(\"%s\") failed, reason: %s\n", dzc->xml_path, strerror(errno));

  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(f, "<Collection MaxLevel=\"%d\" TileSize=\"%d\" "
             "Format=\"%s\" NextItemId=\"%d\">\n",
             dzc->levels, dzc->tile_size, dzc->format, dzc->next_item);
  fprintf(f, "<Items>\n");

  fflush(dzc->tmp);
  fseek(dzc->tmp, 0, SEEK_SET);

  while (fgets(buf, sizeof buf, dzc->tmp))
    fputs(buf, f);

  fclose(dzc->tmp);
  dzc->tmp = NULL;

  fprintf(f, "</Items>\n");
  fprintf(f, "</Collection>\n");

  fclose(f);
}

DZI *dzi_new(char *source, char *out_dir, int tile_size, int overlap, char *format) {
  DZI *dzi;
  char *p, *q;
  char *sfx_dir = DZI_DIR_SFX;
  char *sfx_dzi = OPT_XML_EXT ? ".xml" : ".dzi";
  size_t len_dir, len_dzi;

  if (!out_dir)
    out_dir = ".";

  if (!(dzi = malloc(sizeof(DZI))))
    error("malloc() failed, reason: %s\n", strerror(errno));

  if (!(dzi->source = strdup(source)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  p = basename(dzi->source);

  len_dzi = strlen(out_dir) + strlen(p) + strlen(sfx_dzi) + 2;
  if (!(dzi->xml_path = malloc(len_dzi)))
    error("malloc() failed, reason: %s\n", strerror(errno));

  len_dir = strlen(out_dir) + strlen(p) + strlen(sfx_dir) + 2;
  if (!(dzi->files_path = malloc(len_dir)))
    error("malloc() failed, reason: %s\n", strerror(errno));

  if ((q = strrchr(p, '.')))
    *q = '\0';

  snprintf(dzi->xml_path,   len_dzi, "%s/%s%s", out_dir, p, sfx_dzi);
  snprintf(dzi->files_path, len_dir, "%s/%s%s", out_dir, p, sfx_dir);

  if (q)
    *q = '.';

  dzi->height = dzi->width = dzi->levels = 0;

  dzi->wand = NewMagickWand();

  CHECK_WAND(dzi->wand, MagickReadImage(dzi->wand, dzi->source));
  CHECK_WAND(dzi->wand, MagickSetImageFormat(dzi->wand, OPT_FORMAT));

  debug("read image %s\n", MagickGetImageFilename(dzi->wand));

  dzi->cur_width  = dzi->width  = MagickGetImageWidth(dzi->wand);
  dzi->cur_height = dzi->height = MagickGetImageHeight(dzi->wand);

  debug("image is %dx%d\n", dzi->width, dzi->height);

  if (OPT_ASPECT > 0) {
    change_aspect(OPT_ASPECT, dzi->wand);

    dzi->cur_width  = dzi->width  = MagickGetImageWidth(dzi->wand);
    dzi->cur_height = dzi->height = MagickGetImageHeight(dzi->wand);

    debug("image is %dx%d\n", dzi->width, dzi->height);
  }

  dzi->cur_level = dzi->levels = dzi_zoom_depth(dzi->width, dzi->height);

  dzi->tile_size = tile_size;
  dzi->overlap   = overlap;

  if (!(dzi->format = strdup(format)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  return dzi;
}

void dzi_destory(DZI *dzi) {
  if (!dzi)
    return;

  if (dzi->source)     free(dzi->source);
  if (dzi->format)     free(dzi->format);
  if (dzi->xml_path)   free(dzi->xml_path);
  if (dzi->files_path) free(dzi->files_path);
  if (dzi->wand)       DestroyMagickWand(dzi->wand);

  free(dzi);
}

void dzi_reduce(DZI *dzi) {
  dzi->cur_width  = round(dzi->cur_width  / 2.0);
  dzi->cur_height = round(dzi->cur_height / 2.0);

  CHECK_WAND(dzi->wand, MagickScaleImage(dzi->wand, dzi->cur_width, dzi->cur_height));
}

void dzi_save_tile(DZI *dzi, size_t x, size_t y, size_t w, size_t h, char *file) {
  MagickWand *crop;

  debug("dzi_save_tile: making tile %s, %dx%d\n", file, w, h);

  if (!(crop = CloneMagickWand(dzi->wand)))
    wand_error(dzi->wand);

  CHECK_WAND(crop, MagickCropImage(crop, w, h, x, y));
  CHECK_WAND(crop, MagickWriteImage(crop, file));

  DestroyMagickWand(crop);
}

void dzi_make_tiles(DZI *dzi, DZC *dzc) {
  if (dzc)
    dzc_add_dzi(dzc, dzi);

  make_dir(dzi->files_path);

  for (dzi->cur_level = dzi->levels; dzi->cur_level >= 0; dzi->cur_level -= 1) {
    int x, y, c, r;
    int pt, pr, pb, pl;
    char dir[1024];
    char file[1536];

    snprintf(dir, sizeof dir, "%s/%d", dzi->files_path, dzi->cur_level);

    make_dir(dir);

    debug("level %d size %dx%d\n", dzi->cur_level, dzi->cur_width, dzi->cur_height);

    x = c = 0;

    while (x < dzi->cur_width) {
      y = r = 0;

      while (y < dzi->cur_height) {
        snprintf(file, sizeof file, "%s/%d_%d.%s", dir, c, r, dzi->format);

        pt = y > 0;
        pr = ((c + 1) * dzi->tile_size) < dzi->cur_width;
        pb = ((r + 1) * dzi->tile_size) < dzi->cur_height;
        pl = x > 0;

        debug("tile %d_%d: [%d, %d, %d, %d], [%d, %d, %d, %d]\n", c, r, pt, pr, pb, pl,
                       x - pl, y - pt, dzi->tile_size + pr + pl, dzi->tile_size + pb + pt);

        dzi_save_tile(dzi, x - pl, y - pt, dzi->tile_size + pr + pl,
                                           dzi->tile_size + pb + pt, file);

        y += dzi->tile_size;
        ++r;
      }

      x += dzi->tile_size;
      ++c;
    }

    if (dzc)
      dzc_make_tiles(dzc, dzi);

    dzi_reduce(dzi);
  }
}

void dzi_make_xml(DZI *dzi) {
  FILE *f;

  debug("writing %s\n", dzi->xml_path);

  if (!(f = fopen(dzi->xml_path, "w")))
    error("fopen(\"%s\") failed, reason: %s\n", dzi->xml_path, strerror(errno));

  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(f, "<Image xmlns=\"http://schemas.microsoft.com/deepzoom/2008\"");
  fprintf(f, " TileSize=\"%d\" Overlap=\"%d\" Format=\"%s\">\n",
              dzi->tile_size, dzi->overlap, dzi->format);
  fprintf(f, "  <Size Width=\"%zu\" Height=\"%zu\"/>\n", dzi->width, dzi->height);
  fprintf(f, "</Image>\n");

  fclose(f);
}

void _debug(char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void _error(long  line, char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "%s, error on line %ld: ", APP_NAME, line);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);

  exit(1);
}

void _wand_error(long line, MagickWand *wand) {
  char *msg;
  ExceptionType type;

  msg = MagickGetException(wand, &type); \

  _error(line, "MagickWand error: %s", msg);
}

int dzi_zoom_depth(int width, int height) {
  int x = width > height ? width : height;

  return ceil(log2(x));
}

void morton(int n, int *row, int *col) {
  int x, m, i;
  int *a[2];

  a[0] = col;
  a[1] = row;

  for (m = n, i = x = *row = *col = 0; m > 0; m = m >> 1) {
    *a[i] += (m & 1) << x;

    if (!(i = !i))
      ++x;
  }

  debug("morton: %d: %d, %d\n", n, *row, *col);
}

void make_dir(char *dir) {
  if (mkdir(dir, DIR_MODE) && errno != EEXIST)
    error("mkdir(\"%s\") failed, reason: %s", dir, strerror(errno));
}

void change_aspect(double aspect, MagickWand *wand) {
  size_t w, h, nw, nh, bw, bh;
  double cur_aspect;
  PixelWand *bg;

  bg = NewPixelWand();

  PixelSetColor(bg, "black");

  w = MagickGetImageWidth(wand);
  h = MagickGetImageHeight(wand);

  cur_aspect = (double) w / h;

  nw = w;
  nh = h;

  if (cur_aspect < aspect)
    nw  = lrint(h * aspect);
  else
    nh = lrint(w / aspect);

  bw = (nw - w) / 2;
  bh = (nh - h) / 2;

  debug("aspect ratio is %.2f should be %.2f, adding border %dx%d\n", cur_aspect, aspect, bw, bh);

  CHECK_WAND(wand, MagickFrameImage(wand, bg, (nw - w) / 2, (nh - h) / 2, 0, 0));

  DestroyPixelWand(bg);
}
