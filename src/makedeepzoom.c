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

#define PROGNAME  "makedeepzoom"
#define DIR_MODE  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH

#define error(fmt, ...) _error(__LINE__, fmt, __VA_ARGS__)
#define wand_error(wand) _wand_error(__LINE__, wand)
#define CHECK_WAND(wand, x) if (x == MagickFalse) wand_error(wand)

void _wand_error(long line, MagickWand *wand);
void _error(long  line, char *fmt, ...);
void debug(char *fmt, ...);

typedef struct {
  char  *format;
  char  *source;
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
  char *files_path;
  int   levels;
  int   tile_size;
  int   morton_row;
  int   morton_col;
} DZC;

DZI *dzi_new(char *source, char *files_path, int tile_size, int overlap, char *format);
void dzi_destory(DZI *dzi);
void dzi_make_tiles(DZI *dzi, DZC *dzc);
void dzi_reduce(DZI *dzi);

DZC *dzc_new(char *files_path, int dzc_num, int tile_size, char *format, int max_levels);
void dzc_make_dirs(DZC *dzc);
void dzc_make_tiles(DZC *dzc, DZI *dzi);
void dzc_destory(DZC *dzc);

char *basename(char *path);
void make_dir(char *dir);

void morton(int n, int *row, int *col);
char *dzi_dir(char *dzi_file);
int dzi_zoom_depth(int width, int height);

int OPT_DEBUG     = 0;
int OPT_XML_EXT   = 0;
int OPT_DZC_DEPTH = 8;
int OPT_DZC_NUM   = 0;
int OPT_TILE_SIZE = 256;
int OPT_OVERLAP   = 1;
char *OPT_FORMAT  = "jpg";
char *OPT_DZI     = NULL;
char *OPT_DZC     = NULL;
char *OPT_SOURCE  = NULL;

void usage(void) {
  fprintf(stderr, "usage: " PROGNAME 
                  " [OPTIONS] [-c dzc_dir -n dzc_num] -i dzi_dir -s source_image \n");
}

int main(int argc, char **argv) {
  int opt;
  DZC *dzc = NULL;

  while ((opt = getopt(argc, argv, "dc:i:s:t:f:n:m:o:")) != -1) {
    switch(opt) {
      case 'd': OPT_DEBUG     = 1;            break;
      case 'c': OPT_DZC       = optarg;       break;
      case 'i': OPT_DZI       = optarg;       break;
      case 's': OPT_SOURCE    = optarg;       break;
      case 't': OPT_TILE_SIZE = atoi(optarg); break;
      case 'f': OPT_FORMAT    = optarg;       break;
      case 'n': OPT_DZC_NUM   = atoi(optarg); break;
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

  debug("OPT_DEBUG     = %d\n", OPT_DEBUG);
  debug("OPT_TILE_SIZE = %d\n", OPT_TILE_SIZE);
  debug("OPT_DZC_NUM   = %d\n", OPT_DZC_NUM);
  debug("OPT_DZC_DEPTH = %d\n", OPT_DZC_DEPTH);
  debug("OPT_OVERLAP   = %d\n", OPT_OVERLAP);
  debug("OPT_DZC       = %s\n", OPT_DZC    ? OPT_DZC    : "(NULL)");
  debug("OPT_DZI       = %s\n", OPT_DZI    ? OPT_DZI    : "(NULL)");
  debug("OPT_SOURCE    = %s\n", OPT_SOURCE ? OPT_SOURCE : "(NULL)");
  debug("OPT_FORMAT    = %s\n", OPT_FORMAT ? OPT_FORMAT : "(NULL)");

  if (!OPT_DZI || !OPT_SOURCE) {
    usage();
    exit(0);
  }

  MagickWandGenesis();

  if (OPT_DZC) {
    dzc = dzc_new(OPT_DZC, OPT_DZC_NUM, 256, OPT_FORMAT, OPT_DZC_DEPTH);
    dzc_make_dirs(dzc);
  }

  DZI *dzi = dzi_new(OPT_SOURCE, OPT_DZI, OPT_TILE_SIZE, OPT_OVERLAP, OPT_FORMAT);
  dzi_make_tiles(dzi, dzc);
  dzi_destory(dzi);

  if (dzc)
    dzc_destory(dzc);

  MagickWandTerminus();

  return 0;
}

DZC *dzc_new(char *files_path, int dzc_num, int tile_size, char *format, int max_levels) {
  DZC *dzc;

  if (!(dzc = malloc(sizeof(DZC))))
    error("malloc() failed, reason: %s\n", strerror(errno));

  if (!(dzc->files_path = strdup(files_path)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  if (!(dzc->format = strdup(format)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  dzc->tile_size  = tile_size;
  dzc->levels     = max_levels;
  dzc->morton_row = 0;
  dzc->morton_col = 0;

  morton(dzc_num, &(dzc->morton_row), &(dzc->morton_col));

  return dzc;
}

void dzc_destory(DZC *dzc) {
  if (!dzc)
    return;

  if (dzc->format)     free(dzc->format);
  if (dzc->files_path) free(dzc->files_path);

  free(dzc);
}

void dzc_make_dirs(DZC *dzc) {
  char dir[1024];
  int i;

  make_dir(dzc->files_path);

  for (i = 0; i <= dzc->levels; ++i) {
    snprintf(dir, sizeof dir, "%s/%d", dzc->files_path, i);
    make_dir(dir);
  }
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

DZI *dzi_new(char *source, char *files_path, int tile_size, int overlap, char *format) {
  DZI *dzi;

  if (!(dzi = malloc(sizeof(DZI))))
    error("malloc() failed, reason: %s\n", strerror(errno));

  if (!(dzi->source = strdup(source)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  if (!(dzi->files_path = strdup(files_path)))
    error("strdup() failed, reason: %s\n", strerror(errno));

  dzi->height = dzi->width = dzi->levels = 0;

  dzi->wand = NewMagickWand();


  if (!strcmp(dzi->source, "-")) {
    debug("reading from stdin");
    CHECK_WAND(dzi->wand, MagickReadImageFile(dzi->wand, stdin));
  }
  else {
    debug("reading from %s", dzi->source);
    CHECK_WAND(dzi->wand, MagickReadImage(dzi->wand, dzi->source));
  }

  CHECK_WAND(dzi->wand, MagickSetImageFormat(dzi->wand, OPT_FORMAT));

  debug("read image %s\n", MagickGetImageFilename(dzi->wand));

  dzi->cur_width  = dzi->width  = MagickGetImageWidth(dzi->wand);
  dzi->cur_height = dzi->height = MagickGetImageHeight(dzi->wand);

  debug("image is %dx%d\n", dzi->width, dzi->height);

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

void debug(char *fmt, ...) {
  va_list args;

  if (OPT_DEBUG) {
    va_start(args, fmt);  
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
}

void _error(long  line, char *fmt, ...) {
  va_list args;

  va_start(args, fmt);  
  fprintf(stderr, "%s, error on line %ld: ", PROGNAME, line);
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
