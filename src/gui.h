#ifndef __AUTO_BRIGHTNESS_GUI_H__
#define __AUTO_BRIGHTNESS_GUI_H__

extern float widget_h;
extern int nk_theme;

void lux_map_wnd_thread_create(void);
void settings_wnd_thread_create(void);
void brightness_adjust_wnd_create(void);

void gui_init(void);
void gui_exit(void);

#endif // __AUTO_BRIGHTNESS_GUI_H__