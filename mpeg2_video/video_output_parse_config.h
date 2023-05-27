#ifndef OGLE_VIDEO_OUTPUT_PARSE_CONFIG_H
#define OGLE_VIDEO_OUTPUT_PARSE_CONFIG_H

typedef struct {
  int width;
  int height;
} cfg_geometry_t;

typedef struct {
  int horizontal_pixels;
  int vertical_pixels;
} cfg_resolution_t;

typedef struct {
  int fullscreen;
} cfg_initial_state_t;

typedef struct _cfg_display_t {
  char *name;
  cfg_geometry_t geometry;
  cfg_resolution_t resolution;
  cfg_initial_state_t initial_state;
  char *geometry_src;
  char *resolution_src;
  int switch_resolution;
  int ewmh_fullscreen;
  struct _cfg_display_t *next;
} cfg_display_t;

typedef struct {
  int dummy;
} cfg_window_t;

typedef struct {
  cfg_display_t *display;
  cfg_window_t window;
} cfg_video_t;


int get_video_config(cfg_video_t **video);

#endif /* OGLE_VIDEO_OUTPUT_PARSE_CONFIG_H */
